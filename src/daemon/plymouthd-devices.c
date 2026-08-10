/* plymouthd-devices.c - internal device coordination
 *
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#include "plymouthd-devices-private.h"

#include <math.h>
#include <stdlib.h>

#include "ply-device-manager.h"
#include "ply-keyboard.h"
#include "ply-list.h"
#include "ply-logger.h"
#include "ply-terminal.h"
#include "ply-utils.h"
#include "plymouthd-display-private.h"
#include "plymouthd-input-private.h"
#include "plymouthd-policy-private.h"
#include "plymouthd-state-private.h"

struct _plymouthd_devices
{
        ply_device_manager_t              *device_manager;
        ply_terminal_t                    *local_console_terminal;
        plymouthd_devices_event_handlers_t event_handlers;
};

bool
plymouthd_has_displays (plymouthd_t *daemon)
{
        return ply_device_manager_has_displays (daemon->devices->device_manager);
}

bool
plymouthd_has_active_vt (plymouthd_t *daemon)
{
        if (daemon->devices->local_console_terminal == NULL)
                return false;

        return ply_terminal_is_active (daemon->devices->local_console_terminal);
}

bool
plymouthd_has_vt_console (plymouthd_t *daemon)
{
        if (daemon->devices->local_console_terminal == NULL)
                return false;

        return ply_terminal_is_vt (daemon->devices->local_console_terminal);
}

void
plymouthd_for_each_keyboard (
        plymouthd_t                         *daemon,
        plymouthd_devices_keyboard_handler_t handler,
        void                                *user_data)
{
        ply_list_t *keyboards;
        ply_list_node_t *node;

        keyboards = ply_device_manager_get_keyboards (daemon->devices->device_manager);
        node = ply_list_get_first_node (keyboards);
        while (node != NULL) {
                ply_keyboard_t *keyboard;
                ply_list_node_t *next_node;

                keyboard = ply_list_node_get_data (node);
                next_node = ply_list_get_next_node (keyboards, node);
                handler (keyboard, user_data);
                node = next_node;
        }
}

void
plymouthd_for_each_pixel_display (
        plymouthd_t                              *daemon,
        plymouthd_devices_pixel_display_handler_t handler,
        void                                     *user_data)
{
        ply_list_t *pixel_displays;
        ply_list_node_t *node;

        pixel_displays =
                ply_device_manager_get_pixel_displays (daemon->devices->device_manager);
        node = ply_list_get_first_node (pixel_displays);
        while (node != NULL) {
                ply_pixel_display_t *pixel_display;
                ply_list_node_t *next_node;

                pixel_display = ply_list_node_get_data (node);
                next_node = ply_list_get_next_node (pixel_displays, node);
                handler (pixel_display, user_data);
                node = next_node;
        }
}

void
plymouthd_for_each_text_display (
        plymouthd_t                             *daemon,
        plymouthd_devices_text_display_handler_t handler,
        void                                    *user_data)
{
        ply_list_t *text_displays;
        ply_list_node_t *node;

        text_displays =
                ply_device_manager_get_text_displays (daemon->devices->device_manager);
        node = ply_list_get_first_node (text_displays);
        while (node != NULL) {
                ply_text_display_t *text_display;
                ply_list_node_t *next_node;

                text_display = ply_list_node_get_data (node);
                next_node = ply_list_get_next_node (text_displays, node);
                handler (text_display, user_data);
                node = next_node;
        }
}

void
plymouthd_activate_renderers (plymouthd_t *daemon)
{
        ply_device_manager_activate_renderers (daemon->devices->device_manager);
}

void
plymouthd_deactivate_renderers (plymouthd_t *daemon)
{
        ply_device_manager_deactivate_renderers (daemon->devices->device_manager);
}

void
plymouthd_activate_keyboards (plymouthd_t *daemon)
{
        ply_device_manager_activate_keyboards (daemon->devices->device_manager);
}

void
plymouthd_deactivate_keyboards (plymouthd_t *daemon)
{
        ply_device_manager_deactivate_keyboards (daemon->devices->device_manager);
}

void
plymouthd_prepare_console (plymouthd_t *daemon)
{
        if (daemon->devices->local_console_terminal == NULL)
                return;

        ply_terminal_set_mode (daemon->devices->local_console_terminal,
                               PLY_TERMINAL_MODE_GRAPHICS);
}

void
plymouthd_restore_text_console (plymouthd_t *daemon)
{
        if (daemon->devices->local_console_terminal == NULL)
                return;

        ply_terminal_set_mode (daemon->devices->local_console_terminal,
                               PLY_TERMINAL_MODE_TEXT);
        ply_terminal_set_buffered_input (daemon->devices->local_console_terminal);
}

void
plymouthd_release_console (plymouthd_t *daemon)
{
        if (daemon->devices->local_console_terminal == NULL)
                return;

        ply_trace ("Not retaining splash, so deallocating VT");
        ply_terminal_deactivate_vt (daemon->devices->local_console_terminal);
        ply_terminal_close (daemon->devices->local_console_terminal);
}

void
plymouthd_deactivate_console (plymouthd_t *daemon)
{
        if (daemon->devices->local_console_terminal == NULL)
                return;

        ply_trace ("deactivating terminal");
        ply_terminal_stop_watching_for_vt_changes (
                daemon->devices->local_console_terminal);
        ply_terminal_set_buffered_input (daemon->devices->local_console_terminal);
        ply_terminal_close (daemon->devices->local_console_terminal);
}

void
plymouthd_reactivate_console (plymouthd_t *daemon)
{
        if (daemon->devices->local_console_terminal == NULL)
                return;

        ply_terminal_open (daemon->devices->local_console_terminal);
        ply_terminal_watch_for_vt_changes (daemon->devices->local_console_terminal);
        ply_terminal_set_unbuffered_input (daemon->devices->local_console_terminal);
        ply_terminal_ignore_mode_changes (daemon->devices->local_console_terminal,
                                          false);
}

void
plymouthd_pause_devices (plymouthd_t *daemon)
{
        ply_device_manager_pause (daemon->devices->device_manager);
}

void
plymouthd_unpause_devices (plymouthd_t *daemon)
{
        ply_device_manager_unpause (daemon->devices->device_manager);
}

void
plymouthd_free_devices (plymouthd_t *daemon)
{
        if (daemon->devices == NULL)
                return;

        ply_device_manager_free (daemon->devices->device_manager);
        free (daemon->devices);
        daemon->devices = NULL;
}

static void
on_keyboard_added (void           *user_data,
                   ply_keyboard_t *keyboard)
{
        plymouthd_t *daemon = user_data;

        daemon->devices->event_handlers.keyboard_added (daemon, keyboard);
}

static void
load_devices (plymouthd_t                              *daemon,
              ply_device_manager_flags_t                flags,
              const plymouthd_devices_event_handlers_t *event_handlers)
{
        daemon->devices = calloc (1, sizeof(plymouthd_devices_t));
        daemon->devices->event_handlers = *event_handlers;
        daemon->devices->device_manager =
                ply_device_manager_new (daemon->default_tty,
                                        flags,
                                        daemon->settings.extra_esc_key);
        daemon->devices->local_console_terminal =
                ply_device_manager_get_default_terminal (daemon->devices->device_manager);

        ply_device_manager_watch_devices (
                daemon->devices->device_manager,
                daemon->settings.device_timeout,
                on_keyboard_added,
                (ply_keyboard_removed_handler_t)
                plymouthd_handle_keyboard_removed,
                (ply_pixel_display_added_handler_t)
                plymouthd_handle_pixel_display_added,
                (ply_pixel_display_removed_handler_t)
                plymouthd_handle_pixel_display_removed,
                (ply_text_display_added_handler_t)
                plymouthd_handle_text_display_added,
                (ply_text_display_removed_handler_t)
                plymouthd_handle_text_display_removed,
                daemon);

        if (ply_device_manager_has_serial_consoles (daemon->devices->device_manager))
                daemon->should_force_details = true;
}

void
plymouthd_initialize_devices (
        plymouthd_t                              *daemon,
        bool                                      should_ignore_serial_consoles,
        const plymouthd_devices_event_handlers_t *event_handlers)
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
        load_devices (daemon, flags, event_handlers);
}
