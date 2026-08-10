/* main.c - boot messages monitor
 *
 * Copyright (C) 2007 Red Hat, Inc
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License,
 * or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this; see the file COPYING.  If not, write to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Written by: Ray Strode <rstrode@redhat.com>
 */

#include <sys/stat.h>
#include <sys/types.h>
#include <math.h>
#include <limits.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <sysexits.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <wchar.h>
#include <paths.h>
#include <assert.h>
#include <values.h>
#include <locale.h>

#include <linux/kd.h>
#include <linux/vt.h>

#include "ply-buffer.h"
#include "ply-boot-server-private.h"
#include "ply-boot-splash.h"
#include "ply-device-manager.h"
#include "ply-event-loop.h"
#include "ply-hashtable.h"
#include "ply-list.h"
#include "ply-logger.h"
#include "ply-renderer.h"
#include "ply-terminal-session.h"
#include "ply-trigger.h"
#include "ply-utils.h"
#include "ply-progress.h"
#include "ply-kmsg-reader.h"
#include "plymouthd-interaction-private.h"
#include "plymouthd-diagnostics-private.h"
#include "plymouthd-commands-private.h"
#include "plymouthd-display-private.h"
#include "plymouthd-logging-private.h"
#include "plymouthd-messages-private.h"
#include "plymouthd-options-private.h"
#include "plymouthd-policy-private.h"
#include "plymouthd-progress-private.h"
#include "plymouthd-session-private.h"
#include "plymouthd-state-private.h"
#include "plymouthd-settings-private.h"
#include "plymouthd-splash-private.h"
#include "plymouthd-transition-private.h"
#include "plymouthd-private.h"

static plymouthd_diagnostics_t *diagnostics;

typedef plymouthd_t state_t;

static bool attach_to_running_session (state_t *state);
static void detach_from_running_session (state_t *state);
static void dump_details_and_quit_splash (state_t *state);

static char *pid_file = NULL;
#ifdef PLY_ENABLE_SYSTEMD_INTEGRATION
static void tell_systemd_to_print_details (state_t *state);
static void tell_systemd_to_stop_printing_details (state_t *state);
#endif
static void on_new_kmsg_message (state_t        *state,
                                 kmsg_message_t *kmsg_message);
static void
on_session_output (state_t    *state,
                   const char *output,
                   size_t      size)
{
        ply_buffer_append_bytes (state->boot_buffer, output, size);
        if (state->boot_splash != NULL)
                ply_boot_splash_update_output (state->boot_splash,
                                               output, size);
}

static void
on_session_hangup (state_t *state)
{
        ply_trace ("got hang up on terminal session fd");
}

void
plymouthd_handle_update (state_t    *state,
                         const char *status)
{
        ply_trace ("updating status to '%s'", status);
        plymouthd_progress_status_update (state->progress, status);
        if (state->boot_splash != NULL)
                ply_boot_splash_update_status (state->boot_splash,
                                               status);
}

void
plymouthd_handle_change_mode (state_t    *state,
                              const char *mode)
{
        ply_boot_splash_mode_t new_mode;

        ply_trace ("updating mode to '%s'", mode);
        new_mode = plymouthd_mode_from_string (mode);
        if (new_mode == PLY_BOOT_SPLASH_MODE_INVALID)
                return;

        state->mode = new_mode;
        plymouthd_logging_set_mode (state->logging, new_mode);
        plymouthd_progress_set_mode (state->progress, new_mode);

        plymouthd_logging_prepare (state->logging, state->session);

        if (state->boot_splash == NULL) {
                ply_trace ("no splash set");
                return;
        }

        if (!ply_boot_splash_show (state->boot_splash, state->mode)) {
                ply_trace ("failed to update splash");
                return;
        }
}

void
plymouthd_handle_system_update (state_t *state,
                                int      progress)
{
        if (state->boot_splash == NULL) {
                ply_trace ("no splash set");
                return;
        }

        ply_trace ("setting system update to '%i'", progress);
        if (!ply_boot_splash_system_update (state->boot_splash, progress)) {
                ply_trace ("failed to update splash");
                return;
        }
}

void
plymouthd_handle_ask_for_password (state_t               *state,
                                   const char            *prompt,
                                   ply_trigger_t         *answer,
                                   ply_boot_connection_t *connection)
{
        if (state->boot_splash == NULL) {
                /* Waiting to be shown, boot splash will
                 * arrive shortly so just sit tight
                 */
                if (state->is_shown) {
                        bool has_displays;

                        plymouthd_cancel_pending_show (state);

                        has_displays = ply_device_manager_has_displays (state->device_manager);

                        if (has_displays) {
                                ply_trace ("displays available now, showing splash immediately");
                                plymouthd_show_splash (state);
                        } else {
                                ply_trace ("splash still coming up, waiting a bit");
                        }
                } else {
                        /* No splash, client will have to get password */
                        ply_trace ("no splash loaded, replying immediately with no password");
                        ply_trigger_pull (answer, NULL);
                        return;
                }
        }

        plymouthd_interaction_queue_password (state->interaction,
                                              state->boot_splash,
                                              prompt,
                                              answer,
                                              connection);
}

void
plymouthd_handle_ask_question (state_t               *state,
                               const char            *prompt,
                               ply_trigger_t         *answer,
                               ply_boot_connection_t *connection)
{
        plymouthd_interaction_queue_question (state->interaction,
                                              state->boot_splash,
                                              prompt,
                                              answer,
                                              connection);
}

void
plymouthd_handle_display_message (state_t    *state,
                                  const char *message)
{
        plymouthd_messages_display (state->messages,
                                    state->boot_splash,
                                    message);
}

void
plymouthd_handle_hide_message (state_t    *state,
                               const char *message)
{
        plymouthd_messages_hide (state->messages,
                                 state->boot_splash,
                                 message);
}

void
plymouthd_handle_watch_for_keystroke (state_t               *state,
                                      const char            *keys,
                                      ply_trigger_t         *trigger,
                                      ply_boot_connection_t *connection)
{
        plymouthd_interaction_watch_keystroke (state->interaction,
                                               keys,
                                               trigger,
                                               connection);
}

void
plymouthd_handle_connection_hangup (state_t               *state,
                                    ply_boot_connection_t *connection)
{
        plymouthd_interaction_cancel_connection (state->interaction,
                                                 state->boot_splash,
                                                 connection);
}

void
plymouthd_handle_ignore_keystroke (state_t    *state,
                                   const char *keys)
{
        plymouthd_interaction_ignore_keystroke (state->interaction, keys);
}

void
plymouthd_handle_progress_pause (state_t *state)
{
        ply_trace ("pausing progress");
        plymouthd_progress_pause (state->progress);
}

void
plymouthd_handle_progress_unpause (state_t *state)
{
        ply_trace ("unpausing progress");
        plymouthd_progress_unpause (state->progress);
}

void
plymouthd_handle_newroot (state_t    *state,
                          const char *root_dir)
{
        if (plymouthd_shell_is_init ()) {
                ply_trace ("new root mounted at \"%s\", exiting since init= a shell", root_dir);
                plymouthd_handle_quit (state, false, ply_trigger_new (NULL));
                return;
        }

        ply_trace ("new root mounted at \"%s\", switching to it", root_dir);

        if (!strcmp (root_dir, "/run/initramfs") &&
            plymouthd_diagnostics_has_buffer (diagnostics)) {
                ply_trace ("switching back to initramfs, dumping debug-buffer now");
                plymouthd_diagnostics_dump (diagnostics);
        }

        chdir (root_dir);
        chroot (".");
        chdir ("/");
        /* Update local now that we have /usr/share/locale available */
        setlocale (LC_ALL, "");
        plymouthd_progress_load_cache (state->progress);
        if (state->boot_splash != NULL)
                ply_boot_splash_root_mounted (state->boot_splash);
}

void
plymouthd_handle_system_initialized (state_t *state)
{
        ply_trace ("system now initialized, opening log");

#ifdef PLY_ENABLE_SYSTEMD_INTEGRATION
        if (plymouthd_session_is_attached (state->session))
                tell_systemd_to_print_details (state);
#endif

        plymouthd_logging_system_initialized (state->logging, state->session);
}

void
plymouthd_handle_error (state_t *state)
{
        ply_trace ("encountered error during boot up");

        plymouthd_logging_record_error (state->logging);
}

void
plymouthd_handle_reload (state_t *state)
{
        ply_trace ("reloading");
        if (state->boot_splash != NULL) {
                ply_boot_splash_hide (state->boot_splash);
                ply_boot_splash_free (state->boot_splash);
                state->boot_splash = NULL;
        }

        plymouthd_settings_reload_theme_paths (&state->settings);

        if (state->is_inactive) {
                ply_trace ("reload while inactive");
                return;
        }

        if (!state->is_shown) {
                ply_trace ("reload while not shown");
                return;
        }

        if (state->showing_details) {
                plymouthd_show_detailed_splash (state);
        } else {
                plymouthd_show_default_splash (state);
        }
}


void
plymouthd_handle_show_splash (state_t *state)
{
        bool has_displays;

        if (state->is_shown) {
                ply_trace ("show splash called while already shown");
                return;
        }

        if (state->is_inactive) {
                ply_trace ("show splash called while inactive");
                return;
        }

        if (plymouthd_should_ignore_show_splash_calls (state->mode)) {
                ply_trace ("show splash called while ignoring show splash calls");
                plymouthd_transition_set_retain_splash (state->transition,
                                                        true);
                dump_details_and_quit_splash (state);
                return;
        }

        state->is_shown = true;
        has_displays = ply_device_manager_has_displays (state->device_manager);

        if (!plymouthd_session_is_attached (state->session) &&
            state->should_be_attached && has_displays)
                attach_to_running_session (state);

        if (state->local_console_terminal != NULL)
                ply_terminal_set_mode (state->local_console_terminal, PLY_TERMINAL_MODE_GRAPHICS);

#ifdef PLY_ENABLE_SYSTEMD_INTEGRATION
        if (plymouthd_session_is_attached (state->session))
                tell_systemd_to_print_details (state);
#endif

        if (has_displays) {
                ply_trace ("at least one display already available, so loading splash");
                plymouthd_show_splash (state);
        } else {
                ply_trace ("no displays available to show splash on, waiting...");
        }
}

static void
quit_splash (state_t *state)
{
        ply_trace ("quitting splash");
        if (state->boot_splash != NULL) {
                ply_trace ("freeing splash");
                ply_boot_splash_free (state->boot_splash);
                state->boot_splash = NULL;
        }

        ply_device_manager_deactivate_keyboards (state->device_manager);

        if (state->local_console_terminal != NULL) {
                if (!plymouthd_transition_should_retain_splash (
                            state->transition)) {
                        ply_trace ("Not retaining splash, so deallocating VT");
                        ply_terminal_deactivate_vt (state->local_console_terminal);
                        ply_terminal_close (state->local_console_terminal);
                }
        }

        detach_from_running_session (state);
}

static void
dump_details_and_quit_splash (state_t *state)
{
        state->showing_details = false;
        plymouthd_toggle_details (state);

        plymouthd_hide_splash (state);
        quit_splash (state);
}

void
plymouthd_handle_hide_splash (state_t *state)
{
        if (state->is_inactive)
                return;

        if (state->boot_splash == NULL)
                return;

        ply_trace ("hiding boot splash");
        plymouthd_transition_set_retain_splash (state->transition, true);
        dump_details_and_quit_splash (state);
}

static void
quit_program (state_t *state)
{
        ply_trace ("cleaning up devices");
        ply_device_manager_free (state->device_manager);
        state->device_manager = NULL;

        ply_trace ("exiting event loop");
        ply_event_loop_exit (state->loop, 0);

        if (pid_file != NULL) {
                unlink (pid_file);
                free (pid_file);
                pid_file = NULL;
        }

        plymouthd_transition_complete_all (state->transition);
}

static void
deactivate_console (state_t *state)
{
        detach_from_running_session (state);

        if (state->local_console_terminal != NULL) {
                ply_trace ("deactivating terminal");
                ply_terminal_stop_watching_for_vt_changes (state->local_console_terminal);
                ply_terminal_set_buffered_input (state->local_console_terminal);
                ply_terminal_close (state->local_console_terminal);
        }

        /* do not let any tty opened where we could write after deactivate */
        if (ply_kernel_command_line_has_argument ("plymouth.debug"))
                ply_logger_close_file (ply_logger_get_error_default ());
}

static void
deactivate_splash (state_t *state)
{
        assert (!state->is_inactive);

        if (state->boot_splash && ply_boot_splash_uses_pixel_displays (state->boot_splash))
                ply_device_manager_deactivate_renderers (state->device_manager);

        deactivate_console (state);

        state->is_inactive = true;

        plymouthd_transition_complete_deactivate (state->transition);
}

static void
on_boot_splash_idle (state_t *state)
{
        ply_trace ("boot splash idle");

        /* In the case where we've received both a deactivate command and a
         * quit command, the quit command takes precedence.
         */
        if (plymouthd_transition_has_quit (state->transition)) {
                if (!plymouthd_transition_should_retain_splash (
                            state->transition)) {
                        ply_trace ("hiding splash");
                        plymouthd_hide_splash (state);
                }

                ply_trace ("quitting splash");
                quit_splash (state);
                ply_trace ("quitting program");
                quit_program (state);
        } else if (plymouthd_transition_has_deactivate (state->transition)) {
                ply_trace ("deactivating splash");
                deactivate_splash (state);
        }

        plymouthd_transition_end_idle (state->transition);
}

void
plymouthd_handle_deactivate (state_t       *state,
                             ply_trigger_t *deactivate_trigger)
{
        if (state->is_inactive) {
                deactivate_console (state);
                ply_trigger_pull (deactivate_trigger, NULL);
                return;
        }

        if (!plymouthd_transition_queue_deactivate (state->transition,
                                                    deactivate_trigger)) {
                return;
        }

        ply_trace ("deactivating");
        plymouthd_cancel_pending_show (state);

        ply_device_manager_pause (state->device_manager);
        ply_device_manager_deactivate_keyboards (state->device_manager);

        if (state->boot_splash != NULL) {
                if (plymouthd_transition_begin_idle (state->transition)) {
                        ply_boot_splash_become_idle (state->boot_splash,
                                                     (ply_boot_splash_on_idle_handler_t)
                                                     on_boot_splash_idle,
                                                     state);
                }
        } else {
                ply_trace ("deactivating splash");
                deactivate_splash (state);
        }
}

void
plymouthd_handle_reactivate (state_t *state)
{
        if (!state->is_inactive)
                return;

        if (state->local_console_terminal != NULL) {
                ply_terminal_open (state->local_console_terminal);
                ply_terminal_watch_for_vt_changes (state->local_console_terminal);
                ply_terminal_set_unbuffered_input (state->local_console_terminal);
                ply_terminal_ignore_mode_changes (state->local_console_terminal, false);
        }

        if (plymouthd_session_get_terminal_session (state->session) != NULL &&
            state->should_be_attached) {
                ply_trace ("reactivating terminal session");
                attach_to_running_session (state);
        }

        ply_device_manager_activate_keyboards (state->device_manager);
        if (state->boot_splash && ply_boot_splash_uses_pixel_displays (state->boot_splash))
                ply_device_manager_activate_renderers (state->device_manager);

        ply_device_manager_unpause (state->device_manager);

        state->is_inactive = false;

        plymouthd_update_display (state);
}

void
plymouthd_handle_quit (state_t       *state,
                       bool           retain_splash,
                       ply_trigger_t *quit_trigger)
{
        ply_trace ("quitting (retain splash: %s)", retain_splash ? "true" : "false");

        if (!plymouthd_transition_queue_quit (state->transition,
                                              retain_splash,
                                              quit_trigger)) {
                ply_trace ("quit trigger already pending, so chaining to it");
                return;
        }

        if (plymouthd_logging_is_initialized (state->logging)) {
                ply_trace ("system initialized so saving boot-duration file");
                plymouthd_progress_save_cache (state->progress);
        } else {
                ply_trace ("system not initialized so skipping saving boot-duration file");
        }
        ply_trace ("closing log");
        plymouthd_session_close_log (state->session);

        ply_device_manager_deactivate_keyboards (state->device_manager);

        ply_trace ("unloading splash");
        if (state->is_inactive && !retain_splash) {
                /* We've been deactivated and X failed to start
                 */
                dump_details_and_quit_splash (state);
                quit_program (state);
        } else if (state->boot_splash != NULL) {
                if (plymouthd_transition_begin_idle (state->transition)) {
                        ply_boot_splash_become_idle (state->boot_splash,
                                                     (ply_boot_splash_on_idle_handler_t)
                                                     on_boot_splash_idle,
                                                     state);
                }
        } else {
                if (!plymouthd_transition_should_retain_splash (
                            state->transition)) {
                        plymouthd_hide_splash (state);
                }
                quit_splash (state);
                quit_program (state);
        }
}

void
on_new_kmsg_message (state_t        *state,
                     kmsg_message_t *kmsg_message)
{
        ply_buffer_append (state->boot_buffer, "%s\n", kmsg_message->message);

        if (state->boot_splash != NULL) {
                ply_boot_splash_update_output (state->boot_splash, kmsg_message->message, strlen (kmsg_message->message));
                ply_boot_splash_update_output (state->boot_splash, "\n", 1);
        }
}

bool
plymouthd_handle_has_active_vt (state_t *state)
{
        if (state->local_console_terminal != NULL)
                return ply_terminal_is_active (state->local_console_terminal);
        else
                return false;
}

#ifdef PLY_ENABLE_SYSTEMD_INTEGRATION
static void
tell_systemd_to_print_details (state_t *state)
{
        ply_trace ("telling systemd to start printing details");
        if (kill (1, SIGRTMIN + 20) < 0)
                ply_trace ("could not tell systemd to print details: %m");
}

static void
tell_systemd_to_stop_printing_details (state_t *state)
{
        ply_trace ("telling systemd to stop printing details");
        if (kill (1, SIGRTMIN + 21) < 0)
                ply_trace ("could not tell systemd to stop printing details: %m");
}
#endif

static bool
attach_to_running_session (state_t *state)
{
        bool should_be_redirected;

        should_be_redirected = plymouthd_logging_is_enabled (state->logging);

        if (!plymouthd_session_attach (state->session, should_be_redirected)) {
                ply_buffer_free (state->boot_buffer);
                state->boot_buffer = NULL;
                return false;
        }

#ifdef PLY_ENABLE_SYSTEMD_INTEGRATION
        tell_systemd_to_print_details (state);
#endif

        return true;
}

static void
detach_from_running_session (state_t *state)
{
        if (!plymouthd_session_is_attached (state->session))
                return;

#ifdef PLY_ENABLE_SYSTEMD_INTEGRATION
        tell_systemd_to_stop_printing_details (state);
#endif

        plymouthd_session_detach (state->session);
}

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
find_fallback_tty (state_t *state)
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

        return state->default_tty;
}

static bool
initialize_environment (state_t    *state,
                        const char *debug_path,
                        bool        capture_debug)
{
        ply_trace ("initializing minimal work environment");

        if (!state->default_tty)
                if (getenv ("DISPLAY") != NULL && access (PLYMOUTH_PLUGIN_PATH "renderers/x11.so", F_OK) == 0)
                        state->default_tty = "/dev/tty";
        if (!state->default_tty) {
                if (state->mode == PLY_BOOT_SPLASH_MODE_SHUTDOWN ||
                    state->mode == PLY_BOOT_SPLASH_MODE_REBOOT)
                        state->default_tty = SHUTDOWN_TTY;
                else
                        state->default_tty = BOOT_TTY;

                ply_trace ("checking if '%s' exists", state->default_tty);
                if (!ply_character_device_exists (state->default_tty)) {
                        if (!state->should_force_default_splash) {
                                ply_trace ("nope, forcing details mode");
                                state->should_force_details = true;
                        }

                        state->default_tty = find_fallback_tty (state);
                        ply_trace ("going to go with '%s'", state->default_tty);
                }
        }

        diagnostics = plymouthd_diagnostics_new (state->mode,
                                                 state->default_tty,
                                                 debug_path,
                                                 capture_debug);

        ply_trace ("source built on %s", __DATE__);

        state->interaction = plymouthd_interaction_new ();
        state->messages = plymouthd_messages_new ();

        if (!ply_is_tracing_to_terminal ())
                redirect_standard_io_to_dev_null ();

        ply_trace ("Making sure " PLYMOUTH_RUNTIME_DIR " exists");
        if (!ply_create_directory (PLYMOUTH_RUNTIME_DIR))
                ply_trace ("could not create " PLYMOUTH_RUNTIME_DIR ": %m");

        ply_trace ("initialized minimal work environment");
        return true;
}

#include <termios.h>
#include <unistd.h>
#include <execinfo.h>

#define BACKTRACE_SIZE 1024
#define MAPS_SIZE 8192
#define BACKTRACE_FRAMES_TO_SKIP 2 /* write_backtrace and on_crash themselves */

static void
write_maps (int output_fd)
{
        char maps_buffer[MAPS_SIZE];
        ssize_t bytes_read;
        ssize_t line_start = 0, buffer_end = 0;
        int fd;

        write (output_fd, "maps:\n", strlen ("maps:\n"));
        fd = open ("/proc/self/maps", O_RDONLY);

        if (fd < 0)
                return;

        while ((bytes_read = read (fd, maps_buffer + buffer_end, MAPS_SIZE - buffer_end)) > 0) {
                bytes_read += buffer_end;
                buffer_end = 0;

                for (ssize_t i = line_start; i < bytes_read; ++i) {
                        if (maps_buffer[i] == '\n') {
                                write (output_fd, maps_buffer + line_start, i - line_start + 1);
                                line_start = i + 1;
                        }
                }

                if (line_start < bytes_read) {
                        memmove (maps_buffer, maps_buffer + line_start, bytes_read - line_start);
                        buffer_end = bytes_read - line_start;
                        line_start = 0;
                } else {
                        line_start = 0;
                }
        }

        if (buffer_end > 0) {
                write (output_fd, maps_buffer, buffer_end);
        }

        close (fd);
}

static void
write_backtrace (int output_fd)
{
        void *addresses[BACKTRACE_SIZE];
        int number_of_addresses;

        write (output_fd, "backtrace:\n", strlen ("backtrace:\n"));
        number_of_addresses = backtrace (addresses, BACKTRACE_SIZE);

        if (number_of_addresses <= BACKTRACE_FRAMES_TO_SKIP)
                return;

        backtrace_symbols_fd (addresses + BACKTRACE_FRAMES_TO_SKIP,
                              number_of_addresses - BACKTRACE_FRAMES_TO_SKIP,
                              output_fd);
}

static void
on_crash (int signum)
{
        struct termios term_attributes;
        int fd;
        static const char *show_cursor_sequence = "\033[?25h";

        if (plymouthd_diagnostics_get_crash_fd (diagnostics) != -1) {
                fd = plymouthd_diagnostics_get_crash_fd (diagnostics);
        } else {
                fd = open ("/dev/tty1", O_RDWR | O_NOCTTY);
                if (fd < 0) fd = open ("/dev/hvc0", O_RDWR | O_NOCTTY);
        }

        if (fd >= 0) {
                ioctl (fd, KDSETMODE, KD_TEXT);

                write (fd, show_cursor_sequence, sizeof(show_cursor_sequence) - 1);

                tcgetattr (fd, &term_attributes);

                term_attributes.c_iflag |= BRKINT | IGNPAR | ICRNL | IXON;
                term_attributes.c_oflag |= OPOST;
                term_attributes.c_lflag |= ECHO | ICANON | ISIG | IEXTEN;

                tcsetattr (fd, TCSAFLUSH, &term_attributes);

                write_maps (fd);
                write_backtrace (fd);
        }

        if (plymouthd_diagnostics_has_buffer (diagnostics)) {
                plymouthd_diagnostics_dump (diagnostics);
                sleep (30);
        }

        if (pid_file != NULL) {
                unlink (pid_file);
                free (pid_file);
                pid_file = NULL;
        }

        signal (signum, SIG_DFL);
        raise (signum);
}

static void
start_plymouthd_fd_escrow (void)
{
        pid_t pid;

        pid = fork ();
        if (pid == 0) {
                const char *argv[] = { PLYMOUTH_DRM_ESCROW_DIRECTORY "/plymouthd-fd-escrow", NULL };

                execve (argv[0], (char * const *) argv, NULL);
                ply_trace ("could not launch fd escrow process: %m");
                _exit (1);
        }
}

static void
on_term_signal (state_t *state)
{
        bool retain_splash = false;

        ply_trace ("received SIGTERM");

        /*
         * On shutdown/reboot with pixel-displays active, start the plymouthd-fd-escrow
         * helper to hold on to the pixel-displays fds until the end.
         */
        if ((state->mode == PLY_BOOT_SPLASH_MODE_SHUTDOWN ||
             state->mode == PLY_BOOT_SPLASH_MODE_REBOOT) &&
            !state->is_inactive && state->boot_splash &&
            ply_boot_splash_uses_pixel_displays (state->boot_splash)) {
                start_plymouthd_fd_escrow ();
                retain_splash = true;
        }

        plymouthd_handle_quit (state,
                               retain_splash,
                               ply_trigger_new (NULL));
}

static void
write_pid_file (const char *filename)
{
        FILE *fp;

        fp = fopen (filename, "w");
        if (fp == NULL) {
                ply_error ("could not write pid file %s: %m", filename);
        } else {
                fprintf (fp, "%d\n", (int) getpid ());
                fclose (fp);
        }
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

        pid_file = plymouthd_options_take_pid_file (options);
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

        signal (SIGABRT, on_crash);
        signal (SIGSEGV, on_crash);
        signal (SIGFPE, on_crash);

        if (plymouthd_options_should_use_graphical_boot (options) ||
            ply_kernel_command_line_has_argument ("plymouth.graphical") ||
            plymouthd_kernel_console_is_ttynull ()) {
                daemon->should_force_default_splash = true;
                should_ignore_serial_consoles = true;
        }

        if (!initialize_environment (
                    daemon,
                    plymouthd_options_get_debug_path (options),
                    plymouthd_options_should_debug (options))) {
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

        /* Make the first byte in argv be '@' so that we can survive systemd's killing
         * spree when going from initrd to /
         * http://www.freedesktop.org/wiki/Software/systemd/RootStorageDaemons
         * Note ply_file_exists () does not work here because /etc/initrd-release
         * is a symlink when using a dracut generated initrd.
         */
        if (daemon->mode == PLY_BOOT_SPLASH_MODE_BOOT_UP &&
            access ("/etc/initrd-release", F_OK) >= 0)
                program_name[0] = '@';

        ply_event_loop_watch_signal (daemon->loop,
                                     SIGTERM,
                                     (ply_event_handler_t) on_term_signal,
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
                (plymouthd_session_output_handler_t) on_session_output,
                (plymouthd_session_hangup_handler_t) on_session_hangup,
                (plymouthd_session_kmsg_handler_t) on_new_kmsg_message,
                daemon);

        if (plymouthd_options_should_attach_to_session (options)) {
                daemon->should_be_attached = true;
                if (!attach_to_running_session (daemon)) {
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

        if (pid_file != NULL)
                write_pid_file (pid_file);

        if (daemon_handle != NULL && !ply_detach_daemon (daemon_handle, 0)) {
                ply_error ("plymouthd: could not tell parent to exit: %m");
                *exit_code = EX_UNAVAILABLE;
                goto failed;
        }

        plymouthd_settings_load (&daemon->settings);

        if (ply_kernel_command_line_has_argument ("plymouth.ignore-serial-consoles") ||
            should_ignore_serial_consoles)
                device_manager_flags |= PLY_DEVICE_MANAGER_FLAGS_IGNORE_SERIAL_CONSOLES;

        if (ply_kernel_command_line_has_argument ("plymouth.ignore-udev") ||
            getenv ("DISPLAY") != NULL)
                device_manager_flags |= PLY_DEVICE_MANAGER_FLAGS_IGNORE_UDEV;

        if (ply_kernel_command_line_has_argument ("plymouth.force-frame-buffer-on-boot") &&
            daemon->mode != PLY_BOOT_SPLASH_MODE_SHUTDOWN &&
            daemon->mode != PLY_BOOT_SPLASH_MODE_REBOOT)
                device_manager_flags |= PLY_DEVICE_MANAGER_FLAGS_FORCE_FRAME_BUFFER;

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

        plymouthd_diagnostics_dump (diagnostics);
        ply_free_error_log ();
        plymouthd_diagnostics_free (diagnostics);
        diagnostics = NULL;

        if (pid_file != NULL) {
                unlink (pid_file);
                free (pid_file);
                pid_file = NULL;
        }

        free (daemon);
}
