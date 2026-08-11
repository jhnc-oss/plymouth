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

#include "ply-boot-splash.h"
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
#include "plymouthd-output-private.h"
#include "plymouthd-policy-private.h"
#include "plymouthd-process-private.h"
#include "plymouthd-progress-private.h"
#include "plymouthd-runtime-private.h"
#include "plymouthd-session-private.h"
#include "plymouthd-settings-private.h"
#include "plymouthd-splash-delay-private.h"
#include "plymouthd-state-private.h"
#include "plymouthd-transition-private.h"

static void
on_escape_pressed (plymouthd_t *daemon)
{
        if (plymouthd_handle_escape (daemon))
                plymouthd_toggle_details (daemon);
}

static void
on_keyboard_added (void           *user_data,
                   ply_keyboard_t *keyboard)
{
        plymouthd_t *daemon = user_data;

        plymouthd_handle_keyboard_added (daemon,
                                         keyboard,
                                         on_escape_pressed);
}

static void
on_keyboard_removed (void           *user_data,
                     ply_keyboard_t *keyboard)
{
        plymouthd_t *daemon = user_data;

        plymouthd_handle_keyboard_removed (daemon,
                                           keyboard,
                                           on_escape_pressed);
}

static void
on_pixel_display_added (void                *user_data,
                        ply_pixel_display_t *display)
{
        plymouthd_handle_pixel_display_added (user_data, display);
}

static void
on_pixel_display_removed (void                *user_data,
                          ply_pixel_display_t *display)
{
        plymouthd_handle_pixel_display_removed (user_data, display);
}

static void
on_text_display_added (void               *user_data,
                       ply_text_display_t *display)
{
        plymouthd_handle_text_display_added (user_data, display);
}

static void
on_text_display_removed (void               *user_data,
                         ply_text_display_t *display)
{
        plymouthd_handle_text_display_removed (user_data, display);
}

static const plymouthd_devices_event_handlers_t device_event_handlers = {
        .keyboard_added        = on_keyboard_added,
        .keyboard_removed      = on_keyboard_removed,
        .pixel_display_added   = on_pixel_display_added,
        .pixel_display_removed = on_pixel_display_removed,
        .text_display_added    = on_text_display_added,
        .text_display_removed  = on_text_display_removed,
};

static void
on_splash_delay_elapsed (plymouthd_t      *daemon,
                         ply_event_loop_t *loop)
{
        plymouthd_show_splash (daemon);
}

static void
create_devices (plymouthd_t *daemon,
                bool         should_ignore_serial_consoles,
                bool         should_show_default_splash)
{
        ply_device_manager_flags_t flags = PLY_DEVICE_MANAGER_FLAGS_NONE;
        int device_scale;

        if (ply_kernel_command_line_has_argument (
                    "plymouth.ignore-serial-consoles") ||
            should_ignore_serial_consoles) {
                flags |= PLY_DEVICE_MANAGER_FLAGS_IGNORE_SERIAL_CONSOLES;
        }

        if (ply_kernel_command_line_has_argument ("plymouth.ignore-udev") ||
            getenv ("DISPLAY") != NULL) {
                flags |= PLY_DEVICE_MANAGER_FLAGS_IGNORE_UDEV;
        }

        if (ply_kernel_command_line_has_argument (
                    "plymouth.force-frame-buffer-on-boot") &&
            daemon->mode != PLY_BOOT_SPLASH_MODE_SHUTDOWN &&
            daemon->mode != PLY_BOOT_SPLASH_MODE_REBOOT) {
                flags |= PLY_DEVICE_MANAGER_FLAGS_FORCE_FRAME_BUFFER;
        }

        if (!should_show_default_splash) {
                flags |= PLY_DEVICE_MANAGER_FLAGS_SKIP_RENDERERS;
                flags |= PLY_DEVICE_MANAGER_FLAGS_IGNORE_UDEV;
        }

        device_scale = daemon->settings->device_scale;
        if (device_scale != -1)
                ply_set_device_scale (device_scale);

        flags = plymouthd_add_simpledrm_flags (
                flags,
                daemon->settings->use_simpledrm);

        daemon->devices = plymouthd_devices_new (
                daemon->default_tty,
                flags,
                daemon->settings->extra_esc_key,
                daemon->settings->device_timeout,
                &device_event_handlers,
                daemon);

        if (plymouthd_devices_has_serial_consoles (daemon->devices))
                daemon->should_force_details = true;
}

plymouthd_t *
plymouthd_new (plymouthd_options_t *options,
               char                *program_name,
               int                 *exit_code)
{
        plymouthd_t *daemon;
        ply_daemon_handle_t *daemon_handle = NULL;
        double start_time;
        bool should_ignore_serial_consoles;
        bool should_show_default_splash;

        start_time = ply_get_timestamp ();
        daemon = calloc (1, sizeof(plymouthd_t));
        daemon->loop = ply_event_loop_get_default ();
        daemon->mode = plymouthd_options_get_mode (options);
        daemon->settings = plymouthd_settings_new ();

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

        daemon->commands = plymouthd_commands_new (daemon->loop, daemon);
        if (daemon->commands == NULL) {
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

        plymouthd_settings_load (daemon->settings);
        should_show_default_splash = plymouthd_should_show_default_splash (
                daemon->should_force_details,
                daemon->should_force_default_splash);
        daemon->splash_delay = plymouthd_splash_delay_new (
                daemon->loop,
                start_time,
                plymouthd_settings_get_splash_delay (daemon->settings),
                (ply_event_loop_timeout_handler_t) on_splash_delay_elapsed,
                daemon);
        if (!should_show_default_splash)
                plymouthd_splash_delay_cancel (daemon->splash_delay);
        create_devices (daemon,
                        should_ignore_serial_consoles,
                        should_show_default_splash);

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
        plymouthd_commands_free (daemon->commands);
        plymouthd_devices_free (daemon->devices);
        ply_trace ("freeing terminal session");
        plymouthd_session_free (daemon->session);
        plymouthd_transition_free (daemon->transition);
        plymouthd_output_free (daemon->output);
        plymouthd_progress_free (daemon->progress);
        plymouthd_interaction_free (daemon->interaction);
        plymouthd_logging_free (daemon->logging);
        plymouthd_messages_free (daemon->messages);
        plymouthd_settings_free (daemon->settings);
        plymouthd_splash_delay_free (daemon->splash_delay);
        plymouthd_process_free (daemon->process);
        free (daemon);
}
