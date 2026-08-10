/* plymouthd-display.c - internal display coordination
 *
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#include "plymouthd-display-private.h"

#include <math.h>

#include "ply-boot-splash.h"
#include "ply-event-loop.h"
#include "ply-logger.h"
#include "ply-utils.h"
#include "plymouthd-devices-private.h"
#include "plymouthd-interaction-private.h"
#include "plymouthd-messages-private.h"
#include "plymouthd-policy-private.h"
#include "plymouthd-progress-private.h"
#include "plymouthd-splash-private.h"
#include "plymouthd-state-private.h"

static void
show_messages (plymouthd_t *daemon)
{
        plymouthd_messages_replay (daemon->messages, daemon->boot_splash);
}

static void
attach_pixel_display_to_splash (ply_pixel_display_t *display,
                                void                *user_data)
{
        ply_boot_splash_t *splash = user_data;

        ply_boot_splash_add_pixel_display (splash, display);
}

void
plymouthd_attach_pixel_displays_to_splash (plymouthd_t       *daemon,
                                           ply_boot_splash_t *splash)
{
        plymouthd_for_each_pixel_display (
                daemon,
                attach_pixel_display_to_splash,
                splash);
}

static ply_boot_splash_t *
show_theme (plymouthd_t *daemon,
            const char  *theme_path)
{
        ply_boot_splash_t *splash;

        splash = plymouthd_load_splash (
                theme_path,
                PLYMOUTH_PLUGIN_PATH,
                daemon->boot_buffer,
                daemon->loop);

        if (splash == NULL)
                return NULL;

        plymouthd_progress_attach_to_splash (daemon->progress, splash);
        plymouthd_attach_splash_to_devices (daemon, splash);
        if (ply_boot_splash_uses_pixel_displays (splash))
                plymouthd_activate_renderers (daemon);

        if (!ply_boot_splash_show (splash, daemon->mode)) {
                ply_save_errno ();
                ply_boot_splash_free (splash);
                ply_restore_errno ();
                return NULL;
        }

        plymouthd_activate_keyboards (daemon);
        return splash;
}

void
plymouthd_update_display (plymouthd_t *daemon)
{
        plymouthd_interaction_update_display (daemon->interaction,
                                              daemon->boot_splash);
}

void
plymouthd_handle_pixel_display_added (plymouthd_t         *daemon,
                                      ply_pixel_display_t *display)
{
        if (!daemon->is_shown)
                return;

        if (daemon->boot_splash == NULL) {
                ply_trace ("pixel display added before splash loaded, so loading splash now");
                plymouthd_show_splash (daemon);
        } else {
                ply_trace ("pixel display added after splash loaded, so attaching to splash");
                ply_boot_splash_add_pixel_display (daemon->boot_splash, display);
                plymouthd_update_display (daemon);
        }
}

void
plymouthd_handle_pixel_display_removed (plymouthd_t         *daemon,
                                        ply_pixel_display_t *display)
{
        if (daemon->boot_splash != NULL)
                ply_boot_splash_remove_pixel_display (daemon->boot_splash,
                                                      display);
}

void
plymouthd_handle_text_display_added (plymouthd_t        *daemon,
                                     ply_text_display_t *display)
{
        if (!daemon->is_shown)
                return;

        if (daemon->boot_splash == NULL) {
                ply_trace ("text display added before splash loaded, so loading splash now");
                plymouthd_show_splash (daemon);
        } else {
                ply_trace ("text display added after splash loaded, so attaching to splash");
                ply_boot_splash_add_text_display (daemon->boot_splash, display);
                plymouthd_update_display (daemon);
        }
}

void
plymouthd_handle_text_display_removed (plymouthd_t        *daemon,
                                       ply_text_display_t *display)
{
        if (daemon->boot_splash != NULL)
                ply_boot_splash_remove_text_display (daemon->boot_splash,
                                                     display);
}

void
plymouthd_cancel_pending_show (plymouthd_t *daemon)
{
        if (isnan (daemon->settings.splash_delay))
                return;

        ply_event_loop_stop_watching_for_timeout (
                daemon->loop,
                (ply_event_loop_timeout_handler_t) plymouthd_show_splash,
                daemon);
        daemon->settings.splash_delay = NAN;
}

void
plymouthd_show_detailed_splash (plymouthd_t *daemon)
{
        ply_boot_splash_t *splash;

        plymouthd_cancel_pending_show (daemon);

        if (daemon->boot_splash != NULL)
                return;

        ply_trace ("Showing detailed splash screen");
        splash = show_theme (daemon, NULL);

        if (splash == NULL) {
                ply_trace ("Could not start detailed splash screen, this could be a problem.");
                return;
        }

        daemon->boot_splash = splash;
        show_messages (daemon);
        plymouthd_update_display (daemon);
}

void
plymouthd_show_default_splash (plymouthd_t *daemon)
{
        if (daemon->boot_splash != NULL)
                return;

        ply_trace ("Showing splash screen");
        if (daemon->settings.override_splash_path != NULL) {
                ply_trace ("Trying override splash at '%s'",
                           daemon->settings.override_splash_path);
                daemon->boot_splash =
                        show_theme (daemon,
                                    daemon->settings.override_splash_path);
        }

        if (daemon->boot_splash == NULL &&
            daemon->settings.system_default_splash_path != NULL) {
                ply_trace ("Trying system default splash");
                daemon->boot_splash =
                        show_theme (daemon,
                                    daemon->settings.system_default_splash_path);
        }

        if (daemon->boot_splash == NULL &&
            daemon->settings.distribution_default_splash_path != NULL) {
                ply_trace ("Trying distribution default splash");
                daemon->boot_splash = show_theme (
                        daemon,
                        daemon->settings.distribution_default_splash_path);
        }

        if (daemon->boot_splash == NULL) {
                ply_trace ("Trying old scheme for default splash");
                daemon->boot_splash =
                        show_theme (daemon,
                                    PLYMOUTH_THEME_PATH "default.plymouth");
        }

        if (daemon->boot_splash == NULL) {
                ply_trace ("Could not start default splash screen,"
                           "showing text splash screen");
                daemon->boot_splash = show_theme (
                        daemon,
                        PLYMOUTH_THEME_PATH "text/text.plymouth");
        }

        if (daemon->boot_splash == NULL) {
                ply_trace ("Could not start text splash screen,"
                           "showing built-in splash screen");
                daemon->boot_splash = show_theme (daemon, NULL);
        }

        if (daemon->boot_splash == NULL) {
                ply_error ("plymouthd: could not start boot splash: %m");
                return;
        }

        show_messages (daemon);
        plymouthd_update_display (daemon);
}

void
plymouthd_show_splash (plymouthd_t *daemon)
{
        if (daemon->boot_splash != NULL)
                return;

        if (!isnan (daemon->settings.splash_delay)) {
                double now, running_time;

                now = ply_get_timestamp ();
                running_time = now - daemon->start_time;
                if (daemon->settings.splash_delay > running_time) {
                        double time_left;

                        time_left = daemon->settings.splash_delay - running_time;
                        ply_trace ("delaying show splash for %lf seconds",
                                   time_left);
                        ply_event_loop_stop_watching_for_timeout (
                                daemon->loop,
                                (ply_event_loop_timeout_handler_t)
                                plymouthd_show_splash,
                                daemon);
                        ply_event_loop_watch_for_timeout (
                                daemon->loop,
                                time_left,
                                (ply_event_loop_timeout_handler_t)
                                plymouthd_show_splash,
                                daemon);
                        plymouthd_activate_keyboards (daemon);
                        return;
                }
        }

        if (plymouthd_should_show_default_splash (
                    daemon->should_force_details,
                    daemon->should_force_default_splash)) {
                plymouthd_show_default_splash (daemon);
                daemon->showing_details = false;
        } else {
                plymouthd_show_detailed_splash (daemon);
                daemon->showing_details = true;
        }
}

void
plymouthd_hide_splash (plymouthd_t *daemon)
{
        if (daemon->boot_splash != NULL &&
            ply_boot_splash_uses_pixel_displays (daemon->boot_splash))
                plymouthd_deactivate_renderers (daemon);

        daemon->is_shown = false;
        plymouthd_cancel_pending_show (daemon);

        if (daemon->boot_splash != NULL)
                ply_boot_splash_hide (daemon->boot_splash);

        plymouthd_restore_text_console (daemon);
}

void
plymouthd_toggle_details (plymouthd_t *daemon)
{
        ply_trace ("toggling between splash and details");
        if (daemon->boot_splash != NULL) {
                ply_trace ("hiding and freeing current splash");
                plymouthd_hide_splash (daemon);
                ply_boot_splash_free (daemon->boot_splash);
                daemon->boot_splash = NULL;
        }

        if (!daemon->showing_details) {
                plymouthd_show_detailed_splash (daemon);
                daemon->showing_details = true;
        } else {
                plymouthd_show_default_splash (daemon);
                daemon->showing_details = false;
        }
}
