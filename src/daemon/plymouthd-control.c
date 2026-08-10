/* plymouthd-control.c - daemon command and transition control
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
#include <unistd.h>
#include <wchar.h>
#include <paths.h>
#include <assert.h>
#include <values.h>
#include <locale.h>

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
#include "plymouthd-commands-private.h"
#include "plymouthd-control-private.h"
#include "plymouthd-display-private.h"
#include "plymouthd-logging-private.h"
#include "plymouthd-messages-private.h"
#include "plymouthd-options-private.h"
#include "plymouthd-policy-private.h"
#include "plymouthd-process-private.h"
#include "plymouthd-progress-private.h"
#include "plymouthd-session-private.h"
#include "plymouthd-state-private.h"
#include "plymouthd-settings-private.h"
#include "plymouthd-splash-private.h"
#include "plymouthd-transition-private.h"
#include "plymouthd-private.h"

typedef plymouthd_t state_t;

bool plymouthd_attach_session (state_t *state);
static void detach_from_running_session (state_t *state);
static void dump_details_and_quit_splash (state_t *state);

#ifdef PLY_ENABLE_SYSTEMD_INTEGRATION
static void tell_systemd_to_print_details (state_t *state);
static void tell_systemd_to_stop_printing_details (state_t *state);
#endif
void
plymouthd_handle_session_output (state_t    *state,
                                 const char *output,
                                 size_t      size)
{
        ply_buffer_append_bytes (state->boot_buffer, output, size);
        if (state->boot_splash != NULL)
                ply_boot_splash_update_output (state->boot_splash,
                                               output, size);
}

void
plymouthd_handle_session_hangup (state_t *state)
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
            plymouthd_process_has_diagnostics (state->process)) {
                ply_trace ("switching back to initramfs, dumping debug-buffer now");
                plymouthd_process_dump_diagnostics (state->process);
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
                plymouthd_attach_session (state);

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

        plymouthd_process_remove_pid_file (state->process);

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

        if (plymouthd_session_has_terminal (state->session) &&
            state->should_be_attached) {
                ply_trace ("reactivating terminal session");
                plymouthd_attach_session (state);
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
plymouthd_handle_kmsg (state_t        *state,
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

bool
plymouthd_attach_session (state_t *state)
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

void
plymouthd_handle_term_signal (state_t *state)
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
