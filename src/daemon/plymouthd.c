/* plymouthd.c - daemon lifecycle
 *
 * Copyright (C) 2007 Red Hat, Inc
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License,
 * or (at your option) any later version.
 */

#include "plymouthd-private.h"

#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <sysexits.h>
#include <unistd.h>

#include "ply-boot-server.h"
#include "ply-boot-splash.h"
#include "ply-buffer.h"
#include "ply-event-loop.h"
#include "ply-logger.h"
#include "ply-utils.h"
#include "plymouthd-commands-private.h"
#include "plymouthd-devices-private.h"
#include "plymouthd-display-private.h"
#include "plymouthd-environment-private.h"
#include "plymouthd-input-private.h"
#include "plymouthd-interaction-private.h"
#include "plymouthd-logging-private.h"
#include "plymouthd-messages-private.h"
#include "plymouthd-options-private.h"
#include "plymouthd-policy-private.h"
#include "plymouthd-process-private.h"
#include "plymouthd-progress-private.h"
#include "plymouthd-runtime-private.h"
#include "plymouthd-session-private.h"
#include "plymouthd-settings-private.h"
#include "plymouthd-state-private.h"
#include "plymouthd-transition-private.h"

static void
on_escape_pressed (plymouthd_t *daemon)
{
        if (plymouthd_handle_escape (daemon))
                plymouthd_toggle_details (daemon);
}

static void
on_keyboard_added (plymouthd_t    *daemon,
                   ply_keyboard_t *keyboard)
{
        plymouthd_handle_keyboard_added (daemon,
                                         keyboard,
                                         on_escape_pressed);
}

static void
on_keyboard_removed (plymouthd_t    *daemon,
                     ply_keyboard_t *keyboard)
{
        plymouthd_handle_keyboard_removed (daemon,
                                           keyboard,
                                           on_escape_pressed);
}

static const plymouthd_devices_event_handlers_t device_event_handlers = {
        .keyboard_added        = on_keyboard_added,
        .keyboard_removed      = on_keyboard_removed,
        .pixel_display_added   = plymouthd_handle_pixel_display_added,
        .pixel_display_removed = plymouthd_handle_pixel_display_removed,
        .text_display_added    = plymouthd_handle_text_display_added,
        .text_display_removed  = plymouthd_handle_text_display_removed,
};

plymouthd_t *
plymouthd_new (plymouthd_options_t *options,
               char                *program_name,
               int                 *exit_code)
{
        plymouthd_t *daemon;
        ply_daemon_handle_t *daemon_handle = NULL;
        bool should_ignore_serial_consoles;

        daemon = calloc (1, sizeof(plymouthd_t));
        daemon->start_time = ply_get_timestamp ();
        daemon->loop = ply_event_loop_get_default ();
        daemon->mode = plymouthd_options_get_mode (options);
        plymouthd_settings_init (&daemon->settings);

        should_ignore_serial_consoles =
                plymouthd_options_should_ignore_serial_consoles (options);

        if (plymouthd_options_get_tty (options) != NULL)
                daemon->default_tty = plymouthd_options_get_tty (options);

        daemon->logging = plymouthd_logging_new (
                daemon->mode,
                plymouthd_options_get_boot_log_path (options),
                !plymouthd_options_should_log_boot (options));

        chdir ("/");
        signal (SIGPIPE, SIG_IGN);

        if (plymouthd_options_should_daemonize (options)) {
                daemon_handle = ply_create_daemon ();

                if (daemon_handle == NULL) {
                        ply_error ("plymouthd: cannot daemonize: %m");
                        *exit_code = EX_UNAVAILABLE;
                        goto failed;
                }
        }

        if (plymouthd_options_should_use_graphical_boot (options) ||
            ply_kernel_command_line_has_argument ("plymouth.graphical") ||
            plymouthd_kernel_console_is_ttynull ()) {
                daemon->should_force_default_splash = true;
                should_ignore_serial_consoles = true;
        }

        if (!plymouthd_initialize_environment (
                    daemon,
                    plymouthd_options_get_debug_path (options),
                    plymouthd_options_should_debug (options),
                    plymouthd_options_take_pid_file (options))) {
                if (errno == 0) {
                        *exit_code = EX_OK;
                } else {
                        ply_error ("plymouthd: could not setup basic operating environment: %m");
                        *exit_code = EX_OSERR;
                }

                if (daemon_handle != NULL)
                        ply_detach_daemon (daemon_handle, *exit_code);
                goto failed;
        }

        /* Make the first byte in argv be '@' so that we can survive systemd's
         * killing spree when going from initrd to /. Note ply_file_exists ()
         * does not work here because /etc/initrd-release is a symlink when
         * using a dracut generated initrd.
         */
        if (daemon->mode == PLY_BOOT_SPLASH_MODE_BOOT_UP &&
            access ("/etc/initrd-release", F_OK) >= 0)
                program_name[0] = '@';

        ply_event_loop_watch_signal (
                daemon->loop,
                SIGTERM,
                (ply_event_handler_t) plymouthd_handle_term_signal,
                daemon);

        daemon->boot_server = plymouthd_start_commands (daemon->loop, daemon);
        if (daemon->boot_server == NULL) {
                ply_trace ("plymouthd is already running");
                if (daemon_handle != NULL)
                        ply_detach_daemon (daemon_handle, EX_OK);
                *exit_code = EX_OK;
                goto failed;
        }

        if (!plymouthd_initialize_session (
                    daemon,
                    plymouthd_options_should_attach_to_session (options))) {
                ply_trace ("could not redirect console session: %m");
                if (daemon_handle != NULL)
                        ply_detach_daemon (daemon_handle, EX_UNAVAILABLE);
                *exit_code = EX_UNAVAILABLE;
                goto failed;
        }

        daemon->progress = plymouthd_progress_new (daemon->mode);
        plymouthd_progress_load_cache (daemon->progress);

        plymouthd_process_write_pid_file (daemon->process);

        if (daemon_handle != NULL && !ply_detach_daemon (daemon_handle, 0)) {
                ply_error ("plymouthd: could not tell parent to exit: %m");
                *exit_code = EX_UNAVAILABLE;
                goto failed;
        }

        plymouthd_settings_load (&daemon->settings);
        plymouthd_initialize_devices (daemon,
                                      should_ignore_serial_consoles,
                                      &device_event_handlers);

        *exit_code = EX_OK;
        return daemon;

failed:
        plymouthd_free (daemon);
        return NULL;
}

int
plymouthd_run (plymouthd_t *daemon)
{
        int exit_code;

        ply_trace ("entering event loop");
        exit_code = ply_event_loop_run (daemon->loop);
        ply_trace ("exited event loop");
        ply_trace ("exiting with code %d", exit_code);

        return exit_code;
}

void
plymouthd_free (plymouthd_t *daemon)
{
        if (daemon == NULL)
                return;

        ply_boot_splash_free (daemon->boot_splash);
        ply_boot_server_free (daemon->boot_server);
        plymouthd_free_devices (daemon);
        ply_trace ("freeing terminal session");
        plymouthd_session_free (daemon->session);
        plymouthd_transition_free (daemon->transition);
        ply_buffer_free (daemon->boot_buffer);
        plymouthd_progress_free (daemon->progress);
        plymouthd_interaction_free (daemon->interaction);
        plymouthd_logging_free (daemon->logging);
        plymouthd_messages_free (daemon->messages);
        plymouthd_settings_free (&daemon->settings);
        plymouthd_process_free (daemon->process);
        free (daemon);
}
