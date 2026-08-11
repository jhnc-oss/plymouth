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

#include "ply-boot-splash.h"
#include "ply-event-loop.h"
#include "ply-logger.h"
#include "ply-utils.h"
#include "plymouthd-devices-private.h"
#include "plymouthd-input-private.h"
#include "plymouthd-interaction-private.h"
#include "plymouthd-messages-private.h"
#include "plymouthd-output-private.h"
#include "plymouthd-policy-private.h"
#include "plymouthd-progress-private.h"
#include "plymouthd-settings-private.h"
#include "plymouthd-splash-private.h"
#include "plymouthd-splash-delay-private.h"
#include "plymouthd-state-private.h"

static void
show_messages (plymouthd_t *daemon)
{
        plymouthd_messages_replay (daemon->messages,
                                   plymouthd_splash_get (daemon->splash));
}

static void
attach_pixel_display_to_splash (ply_pixel_display_t *display,
                                void                *user_data)
{
        ply_boot_splash_t *splash = user_data;

        ply_boot_splash_add_pixel_display (splash, display);
}

static void
attach_pixel_displays_to_splash (plymouthd_t       *daemon,
                                 ply_boot_splash_t *splash)
{
        plymouthd_devices_for_each_pixel_display (
                daemon->devices,
                attach_pixel_display_to_splash,
                splash);
}

static void
attach_text_display_to_splash (ply_text_display_t *display,
                               void               *user_data)
{
        ply_boot_splash_t *splash = user_data;

        ply_boot_splash_add_text_display (splash, display);
}

static void
attach_text_displays_to_splash (plymouthd_t       *daemon,
                                ply_boot_splash_t *splash)
{
        plymouthd_devices_for_each_text_display (
                daemon->devices,
                attach_text_display_to_splash,
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
                plymouthd_output_get_buffer (daemon->output),
                daemon->loop);

        if (splash == NULL)
                return NULL;

        plymouthd_progress_attach_to_splash (daemon->progress, splash);
        plymouthd_attach_keyboards_to_splash (daemon, splash);
        attach_pixel_displays_to_splash (daemon, splash);
        attach_text_displays_to_splash (daemon, splash);
        if (ply_boot_splash_uses_pixel_displays (splash))
                plymouthd_devices_activate_renderers (daemon->devices);

        if (!ply_boot_splash_show (splash, daemon->mode)) {
                ply_save_errno ();
                ply_boot_splash_free (splash);
                ply_restore_errno ();
                return NULL;
        }

        plymouthd_devices_activate_keyboards (daemon->devices);
        return splash;
}

void
plymouthd_update_display (plymouthd_t *daemon)
{
        plymouthd_interaction_update_display (daemon->interaction,
                                              plymouthd_splash_get (daemon->splash));
}

void
plymouthd_handle_pixel_display_added (plymouthd_t         *daemon,
                                      ply_pixel_display_t *display)
{
        if (!daemon->is_shown)
                return;

        if (plymouthd_splash_get (daemon->splash) == NULL) {
                ply_trace ("pixel display added before splash loaded, so loading splash now");
                plymouthd_show_splash (daemon);
        } else {
                ply_trace ("pixel display added after splash loaded, so attaching to splash");
                ply_boot_splash_add_pixel_display (
                        plymouthd_splash_get (daemon->splash),
                        display);
                plymouthd_update_display (daemon);
        }
}

void
plymouthd_handle_pixel_display_removed (plymouthd_t         *daemon,
                                        ply_pixel_display_t *display)
{
        if (plymouthd_splash_get (daemon->splash) != NULL)
                ply_boot_splash_remove_pixel_display (
                        plymouthd_splash_get (daemon->splash),
                        display);
}

void
plymouthd_handle_text_display_added (plymouthd_t        *daemon,
                                     ply_text_display_t *display)
{
        if (!daemon->is_shown)
                return;

        if (plymouthd_splash_get (daemon->splash) == NULL) {
                ply_trace ("text display added before splash loaded, so loading splash now");
                plymouthd_show_splash (daemon);
        } else {
                ply_trace ("text display added after splash loaded, so attaching to splash");
                ply_boot_splash_add_text_display (
                        plymouthd_splash_get (daemon->splash),
                        display);
                plymouthd_update_display (daemon);
        }
}

void
plymouthd_handle_text_display_removed (plymouthd_t        *daemon,
                                       ply_text_display_t *display)
{
        if (plymouthd_splash_get (daemon->splash) != NULL)
                ply_boot_splash_remove_text_display (
                        plymouthd_splash_get (daemon->splash),
                        display);
}

void
plymouthd_cancel_pending_show (plymouthd_t *daemon)
{
        plymouthd_splash_delay_cancel (daemon->splash_delay);
}

void
plymouthd_show_detailed_splash (plymouthd_t *daemon)
{
        ply_boot_splash_t *splash;

        plymouthd_cancel_pending_show (daemon);

        if (plymouthd_splash_get (daemon->splash) != NULL)
                return;

        ply_trace ("Showing detailed splash screen");
        splash = show_theme (daemon, NULL);

        if (splash == NULL) {
                ply_trace ("Could not start detailed splash screen, this could be a problem.");
                return;
        }

        plymouthd_splash_take (daemon->splash, splash);
        show_messages (daemon);
        plymouthd_update_display (daemon);
}

void
plymouthd_show_default_splash (plymouthd_t *daemon)
{
        ply_boot_splash_t *splash = NULL;
        const char *override_splash_path;
        const char *system_default_splash_path;
        const char *distribution_default_splash_path;

        if (plymouthd_splash_get (daemon->splash) != NULL)
                return;

        override_splash_path =
                plymouthd_settings_get_override_splash_path (daemon->settings);
        system_default_splash_path =
                plymouthd_settings_get_system_default_splash_path (daemon->settings);
        distribution_default_splash_path =
                plymouthd_settings_get_distribution_default_splash_path (daemon->settings);

        ply_trace ("Showing splash screen");
        if (override_splash_path != NULL) {
                ply_trace ("Trying override splash at '%s'", override_splash_path);
                splash = show_theme (daemon, override_splash_path);
        }

        if (splash == NULL &&
            system_default_splash_path != NULL) {
                ply_trace ("Trying system default splash");
                splash = show_theme (daemon, system_default_splash_path);
        }

        if (splash == NULL &&
            distribution_default_splash_path != NULL) {
                ply_trace ("Trying distribution default splash");
                splash = show_theme (daemon,
                                     distribution_default_splash_path);
        }

        if (splash == NULL) {
                ply_trace ("Trying old scheme for default splash");
                splash = show_theme (daemon,
                                     PLYMOUTH_THEME_PATH "default.plymouth");
        }

        if (splash == NULL) {
                ply_trace ("Could not start default splash screen,"
                           "showing text splash screen");
                splash = show_theme (
                        daemon,
                        PLYMOUTH_THEME_PATH "text/text.plymouth");
        }

        if (splash == NULL) {
                ply_trace ("Could not start text splash screen,"
                           "showing built-in splash screen");
                splash = show_theme (daemon, NULL);
        }

        if (splash == NULL) {
                ply_error ("plymouthd: could not start boot splash: %m");
                return;
        }

        plymouthd_splash_take (daemon->splash, splash);
        show_messages (daemon);
        plymouthd_update_display (daemon);
}

void
plymouthd_show_splash (plymouthd_t *daemon)
{
        double time_left;

        if (plymouthd_splash_get (daemon->splash) != NULL)
                return;

        if (plymouthd_splash_delay_defer (daemon->splash_delay, &time_left)) {
                ply_trace ("delaying show splash for %lf seconds", time_left);
                plymouthd_devices_activate_keyboards (daemon->devices);
                return;
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
        ply_boot_splash_t *boot_splash;

        boot_splash = plymouthd_splash_get (daemon->splash);
        if (boot_splash != NULL &&
            ply_boot_splash_uses_pixel_displays (boot_splash))
                plymouthd_devices_deactivate_renderers (daemon->devices);

        daemon->is_shown = false;
        plymouthd_cancel_pending_show (daemon);

        if (boot_splash != NULL)
                ply_boot_splash_hide (boot_splash);

        plymouthd_devices_restore_text_console (daemon->devices);
}

void
plymouthd_toggle_details (plymouthd_t *daemon)
{
        ply_trace ("toggling between splash and details");
        if (plymouthd_splash_get (daemon->splash) != NULL) {
                ply_trace ("hiding and freeing current splash");
                plymouthd_hide_splash (daemon);
                plymouthd_splash_clear (daemon->splash);
        }

        if (!daemon->showing_details) {
                plymouthd_show_detailed_splash (daemon);
                daemon->showing_details = true;
        } else {
                plymouthd_show_default_splash (daemon);
                daemon->showing_details = false;
        }
}
