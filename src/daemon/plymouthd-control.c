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

#include "plymouthd-control-private.h"

#include <assert.h>

#include "ply-boot-splash.h"
#include "ply-event-loop.h"
#include "ply-logger.h"
#include "ply-trigger.h"
#include "ply-utils.h"
#include "plymouthd-devices-private.h"
#include "plymouthd-display-private.h"
#include "plymouthd-logging-private.h"
#include "plymouthd-policy-private.h"
#include "plymouthd-process-private.h"
#include "plymouthd-progress-private.h"
#include "plymouthd-runtime-private.h"
#include "plymouthd-session-private.h"
#include "plymouthd-splash-private.h"
#include "plymouthd-state-private.h"
#include "plymouthd-transition-private.h"

static void detach_from_running_session (plymouthd_t *daemon);
static void dump_details_and_quit_splash (plymouthd_t *daemon);
void
plymouthd_handle_show_splash (plymouthd_t *daemon)
{
        bool has_displays;

        if (plymouthd_splash_is_shown (daemon->splash)) {
                ply_trace ("show splash called while already shown");
                return;
        }

        if (plymouthd_transition_is_inactive (daemon->transition)) {
                ply_trace ("show splash called while inactive");
                return;
        }

        if (plymouthd_should_ignore_show_splash_calls (daemon->mode)) {
                ply_trace ("show splash called while ignoring show splash calls");
                plymouthd_transition_set_retain_splash (daemon->transition,
                                                        true);
                dump_details_and_quit_splash (daemon);
                return;
        }

        plymouthd_splash_set_shown (daemon->splash, true);
        has_displays = plymouthd_devices_has_displays (daemon->devices);

        if (!plymouthd_session_is_attached (daemon->session) &&
            plymouthd_session_should_attach (daemon->session) && has_displays)
                plymouthd_attach_session (daemon);

        plymouthd_devices_prepare_console (daemon->devices);

        plymouthd_session_request_details (daemon->session);

        if (has_displays) {
                ply_trace ("at least one display already available, so loading splash");
                plymouthd_show_splash (daemon);
        } else {
                ply_trace ("no displays available to show splash on, waiting...");
        }
}

static void
quit_splash (plymouthd_t *daemon)
{
        ply_trace ("quitting splash");
        if (plymouthd_splash_get (daemon->splash) != NULL) {
                ply_trace ("freeing splash");
                plymouthd_splash_clear (daemon->splash);
        }

        plymouthd_devices_deactivate_keyboards (daemon->devices);

        if (!plymouthd_transition_should_retain_splash (daemon->transition))
                plymouthd_devices_release_console (daemon->devices);

        detach_from_running_session (daemon);
}

static void
dump_details_and_quit_splash (plymouthd_t *daemon)
{
        plymouthd_splash_set_showing_details (daemon->splash, false);
        plymouthd_toggle_details (daemon);

        plymouthd_hide_splash (daemon);
        quit_splash (daemon);
}

void
plymouthd_handle_hide_splash (plymouthd_t *daemon)
{
        if (plymouthd_transition_is_inactive (daemon->transition))
                return;

        if (plymouthd_splash_get (daemon->splash) == NULL)
                return;

        ply_trace ("hiding boot splash");
        plymouthd_transition_set_retain_splash (daemon->transition, true);
        dump_details_and_quit_splash (daemon);
}

static void
quit_program (plymouthd_t *daemon)
{
        ply_trace ("cleaning up devices");
        plymouthd_devices_free (daemon->devices);
        daemon->devices = NULL;

        ply_trace ("exiting event loop");
        ply_event_loop_exit (daemon->loop, 0);

        plymouthd_process_remove_pid_file (daemon->process);

        plymouthd_transition_complete_all (daemon->transition);
}

static void
deactivate_console (plymouthd_t *daemon)
{
        detach_from_running_session (daemon);

        plymouthd_devices_deactivate_console (daemon->devices);

        /* do not let any tty opened where we could write after deactivate */
        if (ply_kernel_command_line_has_argument ("plymouth.debug"))
                ply_logger_close_file (ply_logger_get_error_default ());
}

static void
deactivate_splash (plymouthd_t *daemon)
{
        assert (!plymouthd_transition_is_inactive (daemon->transition));

        if (plymouthd_splash_get (daemon->splash) != NULL &&
            ply_boot_splash_uses_pixel_displays (
                    plymouthd_splash_get (daemon->splash)))
                plymouthd_devices_deactivate_renderers (daemon->devices);

        deactivate_console (daemon);

        plymouthd_transition_complete_deactivate (daemon->transition);
}

static void
on_boot_splash_idle (plymouthd_t *daemon)
{
        ply_trace ("boot splash idle");

        /* In the case where we've received both a deactivate command and a
         * quit command, the quit command takes precedence.
         */
        if (plymouthd_transition_has_quit (daemon->transition)) {
                if (!plymouthd_transition_should_retain_splash (
                            daemon->transition)) {
                        ply_trace ("hiding splash");
                        plymouthd_hide_splash (daemon);
                }

                ply_trace ("quitting splash");
                quit_splash (daemon);
                ply_trace ("quitting program");
                quit_program (daemon);
        } else if (plymouthd_transition_has_deactivate (daemon->transition)) {
                ply_trace ("deactivating splash");
                deactivate_splash (daemon);
        }

        plymouthd_transition_end_idle (daemon->transition);
}

void
plymouthd_handle_deactivate (plymouthd_t   *daemon,
                             ply_trigger_t *deactivate_trigger)
{
        if (plymouthd_transition_is_inactive (daemon->transition)) {
                deactivate_console (daemon);
                ply_trigger_pull (deactivate_trigger, NULL);
                return;
        }

        if (!plymouthd_transition_queue_deactivate (daemon->transition,
                                                    deactivate_trigger)) {
                return;
        }

        ply_trace ("deactivating");
        plymouthd_cancel_pending_show (daemon);

        plymouthd_devices_pause (daemon->devices);
        plymouthd_devices_deactivate_keyboards (daemon->devices);

        if (plymouthd_splash_get (daemon->splash) != NULL) {
                if (plymouthd_transition_begin_idle (daemon->transition)) {
                        ply_boot_splash_become_idle (
                                plymouthd_splash_get (daemon->splash),
                                (ply_boot_splash_on_idle_handler_t)
                                on_boot_splash_idle,
                                daemon);
                }
        } else {
                ply_trace ("deactivating splash");
                deactivate_splash (daemon);
        }
}

void
plymouthd_handle_reactivate (plymouthd_t *daemon)
{
        if (!plymouthd_transition_is_inactive (daemon->transition))
                return;

        plymouthd_devices_reactivate_console (daemon->devices);

        if (plymouthd_session_has_terminal (daemon->session) &&
            plymouthd_session_should_attach (daemon->session)) {
                ply_trace ("reactivating terminal session");
                plymouthd_attach_session (daemon);
        }

        plymouthd_devices_activate_keyboards (daemon->devices);
        if (plymouthd_splash_get (daemon->splash) != NULL &&
            ply_boot_splash_uses_pixel_displays (
                    plymouthd_splash_get (daemon->splash)))
                plymouthd_devices_activate_renderers (daemon->devices);

        plymouthd_devices_unpause (daemon->devices);

        plymouthd_transition_activate (daemon->transition);

        plymouthd_update_display (daemon);
}

void
plymouthd_handle_quit (plymouthd_t   *daemon,
                       bool           retain_splash,
                       ply_trigger_t *quit_trigger)
{
        ply_trace ("quitting (retain splash: %s)", retain_splash ? "true" : "false");

        if (!plymouthd_transition_queue_quit (daemon->transition,
                                              retain_splash,
                                              quit_trigger)) {
                ply_trace ("quit trigger already pending, so chaining to it");
                return;
        }

        if (plymouthd_logging_is_initialized (daemon->logging)) {
                ply_trace ("system initialized so saving boot-duration file");
                plymouthd_progress_save_cache (daemon->progress);
        } else {
                ply_trace ("system not initialized so skipping saving boot-duration file");
        }
        ply_trace ("closing log");
        plymouthd_session_close_log (daemon->session);

        plymouthd_devices_deactivate_keyboards (daemon->devices);

        ply_trace ("unloading splash");
        if (plymouthd_transition_is_inactive (daemon->transition) &&
            !retain_splash) {
                /* We've been deactivated and X failed to start
                 */
                dump_details_and_quit_splash (daemon);
                quit_program (daemon);
        } else if (plymouthd_splash_get (daemon->splash) != NULL) {
                if (plymouthd_transition_begin_idle (daemon->transition)) {
                        ply_boot_splash_become_idle (
                                plymouthd_splash_get (daemon->splash),
                                (ply_boot_splash_on_idle_handler_t)
                                on_boot_splash_idle,
                                daemon);
                }
        } else {
                if (!plymouthd_transition_should_retain_splash (
                            daemon->transition)) {
                        plymouthd_hide_splash (daemon);
                }
                quit_splash (daemon);
                quit_program (daemon);
        }
}

static void
detach_from_running_session (plymouthd_t *daemon)
{
        plymouthd_session_detach (daemon->session);
}
