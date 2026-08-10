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
#include <fcntl.h>
#include <math.h>
#include <signal.h>
#include <stdlib.h>
#include <sysexits.h>
#include <unistd.h>

#include "ply-boot-server.h"
#include "ply-boot-splash.h"
#include "ply-buffer.h"
#include "ply-device-manager.h"
#include "ply-event-loop.h"
#include "ply-logger.h"
#include "ply-utils.h"
#include "plymouthd-commands-private.h"
#include "plymouthd-control-private.h"
#include "plymouthd-display-private.h"
#include "plymouthd-interaction-private.h"
#include "plymouthd-logging-private.h"
#include "plymouthd-messages-private.h"
#include "plymouthd-options-private.h"
#include "plymouthd-policy-private.h"
#include "plymouthd-process-private.h"
#include "plymouthd-progress-private.h"
#include "plymouthd-session-private.h"
#include "plymouthd-settings-private.h"
#include "plymouthd-state-private.h"
#include "plymouthd-transition-private.h"

static bool
redirect_standard_io_to_dev_null (void)
{
        int fd;

        fd = open ("/dev/null", O_RDWR | O_APPEND);

        if (fd < 0)
                return false;

        dup2 (fd, STDIN_FILENO);
        dup2 (fd, STDOUT_FILENO);
        dup2 (fd, STDERR_FILENO);
        close (fd);

        return true;
}

static const char *
find_fallback_tty (plymouthd_t *daemon)
{
        static const char *tty_list[] =
        {
                "/dev/ttyS0",
                "/dev/hvc0",
                "/dev/xvc0",
                "/dev/ttySG0",
                NULL
        };
        int i;

        for (i = 0; tty_list[i] != NULL; i++) {
                if (ply_character_device_exists (tty_list[i]))
                        return tty_list[i];
        }

        return daemon->default_tty;
}

static bool
initialize_environment (plymouthd_t *daemon,
                        const char  *debug_path,
                        bool         capture_debug,
                        char        *pid_file)
{
        ply_trace ("initializing minimal work environment");

        if (daemon->default_tty == NULL &&
            getenv ("DISPLAY") != NULL &&
            access (PLYMOUTH_PLUGIN_PATH "renderers/x11.so", F_OK) == 0) {
                daemon->default_tty = "/dev/tty";
        }

        if (daemon->default_tty == NULL) {
                if (daemon->mode == PLY_BOOT_SPLASH_MODE_SHUTDOWN ||
                    daemon->mode == PLY_BOOT_SPLASH_MODE_REBOOT)
                        daemon->default_tty = SHUTDOWN_TTY;
                else
                        daemon->default_tty = BOOT_TTY;

                ply_trace ("checking if '%s' exists", daemon->default_tty);
                if (!ply_character_device_exists (daemon->default_tty)) {
                        if (!daemon->should_force_default_splash) {
                                ply_trace ("nope, forcing details mode");
                                daemon->should_force_details = true;
                        }

                        daemon->default_tty = find_fallback_tty (daemon);
                        ply_trace ("going to go with '%s'", daemon->default_tty);
                }
        }

        daemon->process = plymouthd_process_new (daemon->mode,
                                                 daemon->default_tty,
                                                 debug_path,
                                                 capture_debug,
                                                 pid_file);
        plymouthd_process_install_crash_handlers (daemon->process);

        ply_trace ("source built on %s", __DATE__);

        daemon->interaction = plymouthd_interaction_new ();
        daemon->messages = plymouthd_messages_new ();

        if (!ply_is_tracing_to_terminal ())
                redirect_standard_io_to_dev_null ();

        ply_trace ("Making sure " PLYMOUTH_RUNTIME_DIR " exists");
        if (!ply_create_directory (PLYMOUTH_RUNTIME_DIR))
                ply_trace ("could not create " PLYMOUTH_RUNTIME_DIR ": %m");

        ply_trace ("initialized minimal work environment");
        return true;
}

plymouthd_t *
plymouthd_new (plymouthd_options_t *options,
               char                *program_name,
               int                 *exit_code)
{
        plymouthd_t *daemon;
        ply_daemon_handle_t *daemon_handle = NULL;
        ply_device_manager_flags_t device_manager_flags = PLY_DEVICE_MANAGER_FLAGS_NONE;
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

        if (!initialize_environment (
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

        daemon->boot_buffer = ply_buffer_new ();
        daemon->transition = plymouthd_transition_new ();
        daemon->session = plymouthd_session_new (
                daemon->loop,
                (plymouthd_session_output_handler_t)
                plymouthd_handle_session_output,
                (plymouthd_session_hangup_handler_t)
                plymouthd_handle_session_hangup,
                (plymouthd_session_kmsg_handler_t)
                plymouthd_handle_kmsg,
                daemon);

        if (plymouthd_options_should_attach_to_session (options)) {
                daemon->should_be_attached = true;
                if (!plymouthd_attach_session (daemon)) {
                        ply_trace ("could not redirect console session: %m");
                        if (daemon_handle != NULL)
                                ply_detach_daemon (daemon_handle,
                                                   EX_UNAVAILABLE);
                        *exit_code = EX_UNAVAILABLE;
                        goto failed;
                }
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

        if (ply_kernel_command_line_has_argument (
                    "plymouth.ignore-serial-consoles") ||
            should_ignore_serial_consoles) {
                device_manager_flags |=
                        PLY_DEVICE_MANAGER_FLAGS_IGNORE_SERIAL_CONSOLES;
        }

        if (ply_kernel_command_line_has_argument ("plymouth.ignore-udev") ||
            getenv ("DISPLAY") != NULL) {
                device_manager_flags |= PLY_DEVICE_MANAGER_FLAGS_IGNORE_UDEV;
        }

        if (ply_kernel_command_line_has_argument (
                    "plymouth.force-frame-buffer-on-boot") &&
            daemon->mode != PLY_BOOT_SPLASH_MODE_SHUTDOWN &&
            daemon->mode != PLY_BOOT_SPLASH_MODE_REBOOT) {
                device_manager_flags |=
                        PLY_DEVICE_MANAGER_FLAGS_FORCE_FRAME_BUFFER;
        }

        if (!plymouthd_should_show_default_splash (
                    daemon->should_force_details,
                    daemon->should_force_default_splash)) {
                device_manager_flags |= PLY_DEVICE_MANAGER_FLAGS_SKIP_RENDERERS;
                device_manager_flags |= PLY_DEVICE_MANAGER_FLAGS_IGNORE_UDEV;
                daemon->settings.splash_delay = NAN;
        }

        if (daemon->settings.device_scale != -1)
                ply_set_device_scale (daemon->settings.device_scale);

        device_manager_flags = plymouthd_add_simpledrm_flags (
                device_manager_flags,
                daemon->settings.use_simpledrm);
        plymouthd_load_devices (daemon, device_manager_flags);

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
        ply_device_manager_free (daemon->device_manager);

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
