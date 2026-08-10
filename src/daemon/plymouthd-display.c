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
#include <stdlib.h>

#include "ply-boot-splash.h"
#include "ply-device-manager.h"
#include "ply-event-loop.h"
#include "ply-keyboard.h"
#include "ply-list.h"
#include "ply-logger.h"
#include "ply-terminal.h"
#include "ply-utils.h"
#include "plymouthd-interaction-private.h"
#include "plymouthd-messages-private.h"
#include "plymouthd-policy-private.h"
#include "plymouthd-progress-private.h"
#include "plymouthd-splash-private.h"
#include "plymouthd-state-private.h"

static void on_keyboard_input (plymouthd_t *daemon,
                               const char  *keyboard_input,
                               size_t       character_size);
static void on_escape_pressed (plymouthd_t *daemon);
static void on_backspace (plymouthd_t *daemon);
static void on_enter (plymouthd_t *daemon,
                      const char  *line);

static void
show_messages (plymouthd_t *daemon)
{
        plymouthd_messages_replay (daemon->messages, daemon->boot_splash);
}

static void
attach_splash_to_devices (plymouthd_t       *daemon,
                          ply_boot_splash_t *splash)
{
        ply_list_t *keyboards;
        ply_list_t *pixel_displays;
        ply_list_t *text_displays;
        ply_list_node_t *node;

        keyboards = ply_device_manager_get_keyboards (daemon->device_manager);
        node = ply_list_get_first_node (keyboards);
        while (node != NULL) {
                ply_keyboard_t *keyboard;
                ply_list_node_t *next_node;

                keyboard = ply_list_node_get_data (node);
                next_node = ply_list_get_next_node (keyboards, node);
                ply_boot_splash_set_keyboard (splash, keyboard);
                node = next_node;
        }

        pixel_displays =
                ply_device_manager_get_pixel_displays (daemon->device_manager);
        node = ply_list_get_first_node (pixel_displays);
        while (node != NULL) {
                ply_pixel_display_t *pixel_display;
                ply_list_node_t *next_node;

                pixel_display = ply_list_node_get_data (node);
                next_node = ply_list_get_next_node (pixel_displays, node);
                ply_boot_splash_add_pixel_display (splash, pixel_display);
                node = next_node;
        }

        text_displays =
                ply_device_manager_get_text_displays (daemon->device_manager);
        node = ply_list_get_first_node (text_displays);
        while (node != NULL) {
                ply_text_display_t *text_display;
                ply_list_node_t *next_node;

                text_display = ply_list_node_get_data (node);
                next_node = ply_list_get_next_node (text_displays, node);
                ply_boot_splash_add_text_display (splash, text_display);
                node = next_node;
        }
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
        attach_splash_to_devices (daemon, splash);
        if (ply_boot_splash_uses_pixel_displays (splash))
                ply_device_manager_activate_renderers (daemon->device_manager);

        if (!ply_boot_splash_show (splash, daemon->mode)) {
                ply_save_errno ();
                ply_boot_splash_free (splash);
                ply_restore_errno ();
                return NULL;
        }

        ply_device_manager_activate_keyboards (daemon->device_manager);
        return splash;
}

void
plymouthd_update_display (plymouthd_t *daemon)
{
        plymouthd_interaction_update_display (daemon->interaction,
                                              daemon->boot_splash);
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
                        ply_device_manager_activate_keyboards (
                                daemon->device_manager);
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
                ply_device_manager_deactivate_renderers (daemon->device_manager);

        daemon->is_shown = false;
        plymouthd_cancel_pending_show (daemon);

        if (daemon->boot_splash != NULL)
                ply_boot_splash_hide (daemon->boot_splash);

        if (daemon->local_console_terminal != NULL) {
                ply_terminal_set_mode (daemon->local_console_terminal,
                                       PLY_TERMINAL_MODE_TEXT);
                ply_terminal_set_buffered_input (daemon->local_console_terminal);
        }
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

static void
on_escape_pressed (plymouthd_t *daemon)
{
        bool has_vt_consoles = true;

        ply_trace ("escape key pressed");
        if (daemon->local_console_terminal == NULL ||
            !ply_terminal_is_vt (daemon->local_console_terminal))
                has_vt_consoles = false;

        if (plymouthd_validate_prompt_input (daemon->boot_splash,
                                             "",
                                             "\e") &&
            has_vt_consoles)
                plymouthd_toggle_details (daemon);
}

static void
on_keyboard_input (plymouthd_t *daemon,
                   const char  *keyboard_input,
                   size_t       character_size)
{
        plymouthd_interaction_handle_input (daemon->interaction,
                                            daemon->boot_splash,
                                            keyboard_input,
                                            character_size);
}

static void
on_backspace (plymouthd_t *daemon)
{
        plymouthd_interaction_handle_backspace (daemon->interaction,
                                                daemon->boot_splash);
}

static void
on_enter (plymouthd_t *daemon,
          const char  *line)
{
        plymouthd_interaction_handle_enter (daemon->interaction,
                                            daemon->boot_splash,
                                            line);
}

static void
on_keyboard_added (plymouthd_t    *daemon,
                   ply_keyboard_t *keyboard)
{
        ply_trace ("listening for keystrokes");
        ply_keyboard_add_input_handler (
                keyboard,
                (ply_keyboard_input_handler_t) on_keyboard_input,
                daemon);
        ply_trace ("listening for escape");
        ply_keyboard_add_escape_handler (
                keyboard,
                (ply_keyboard_escape_handler_t) on_escape_pressed,
                daemon);
        ply_trace ("listening for backspace");
        ply_keyboard_add_backspace_handler (
                keyboard,
                (ply_keyboard_backspace_handler_t) on_backspace,
                daemon);
        ply_trace ("listening for enter");
        ply_keyboard_add_enter_handler (
                keyboard,
                (ply_keyboard_enter_handler_t) on_enter,
                daemon);

        if (daemon->boot_splash != NULL) {
                ply_trace ("keyboard set after splash loaded, so attaching to splash");
                ply_boot_splash_set_keyboard (daemon->boot_splash, keyboard);
        }
}

static void
on_keyboard_removed (plymouthd_t    *daemon,
                     ply_keyboard_t *keyboard)
{
        ply_trace ("no longer listening for keystrokes");
        ply_keyboard_remove_input_handler (
                keyboard,
                (ply_keyboard_input_handler_t) on_keyboard_input);
        ply_trace ("no longer listening for escape");
        ply_keyboard_remove_escape_handler (
                keyboard,
                (ply_keyboard_escape_handler_t) on_escape_pressed);
        ply_trace ("no longer listening for backspace");
        ply_keyboard_remove_backspace_handler (
                keyboard,
                (ply_keyboard_backspace_handler_t) on_backspace);
        ply_trace ("no longer listening for enter");
        ply_keyboard_remove_enter_handler (
                keyboard,
                (ply_keyboard_enter_handler_t) on_enter);

        if (daemon->boot_splash != NULL)
                ply_boot_splash_unset_keyboard (daemon->boot_splash);
}

static void
on_pixel_display_added (plymouthd_t         *daemon,
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

static void
on_pixel_display_removed (plymouthd_t         *daemon,
                          ply_pixel_display_t *display)
{
        if (daemon->boot_splash != NULL)
                ply_boot_splash_remove_pixel_display (daemon->boot_splash,
                                                      display);
}

static void
on_text_display_added (plymouthd_t        *daemon,
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

static void
on_text_display_removed (plymouthd_t        *daemon,
                         ply_text_display_t *display)
{
        if (daemon->boot_splash != NULL)
                ply_boot_splash_remove_text_display (daemon->boot_splash,
                                                     display);
}

void
plymouthd_load_devices (plymouthd_t               *daemon,
                        ply_device_manager_flags_t flags)
{
        daemon->device_manager =
                ply_device_manager_new (daemon->default_tty,
                                        flags,
                                        daemon->settings.extra_esc_key);
        daemon->local_console_terminal =
                ply_device_manager_get_default_terminal (daemon->device_manager);

        ply_device_manager_watch_devices (
                daemon->device_manager,
                daemon->settings.device_timeout,
                (ply_keyboard_added_handler_t) on_keyboard_added,
                (ply_keyboard_removed_handler_t) on_keyboard_removed,
                (ply_pixel_display_added_handler_t) on_pixel_display_added,
                (ply_pixel_display_removed_handler_t) on_pixel_display_removed,
                (ply_text_display_added_handler_t) on_text_display_added,
                (ply_text_display_removed_handler_t) on_text_display_removed,
                daemon);

        if (ply_device_manager_has_serial_consoles (daemon->device_manager))
                daemon->should_force_details = true;
}

void
plymouthd_initialize_devices (plymouthd_t *daemon,
                              bool         should_ignore_serial_consoles)
{
        ply_device_manager_flags_t flags = PLY_DEVICE_MANAGER_FLAGS_NONE;

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

        if (!plymouthd_should_show_default_splash (
                    daemon->should_force_details,
                    daemon->should_force_default_splash)) {
                flags |= PLY_DEVICE_MANAGER_FLAGS_SKIP_RENDERERS;
                flags |= PLY_DEVICE_MANAGER_FLAGS_IGNORE_UDEV;
                daemon->settings.splash_delay = NAN;
        }

        if (daemon->settings.device_scale != -1)
                ply_set_device_scale (daemon->settings.device_scale);

        flags = plymouthd_add_simpledrm_flags (
                flags,
                daemon->settings.use_simpledrm);
        plymouthd_load_devices (daemon, flags);
}
