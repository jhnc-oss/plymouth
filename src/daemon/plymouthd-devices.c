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

#include <stdlib.h>

#include "ply-device-manager.h"
#include "ply-keyboard.h"
#include "ply-list.h"
#include "ply-logger.h"
#include "ply-terminal.h"

struct _plymouthd_devices
{
        ply_device_manager_t              *device_manager;
        ply_terminal_t                    *local_console_terminal;
        plymouthd_devices_event_handlers_t event_handlers;
        void                              *event_user_data;
};

bool
plymouthd_devices_has_displays (plymouthd_devices_t *devices)
{
        return ply_device_manager_has_displays (devices->device_manager);
}

bool
plymouthd_devices_has_serial_consoles (plymouthd_devices_t *devices)
{
        return ply_device_manager_has_serial_consoles (devices->device_manager);
}

bool
plymouthd_devices_has_active_vt (plymouthd_devices_t *devices)
{
        if (devices->local_console_terminal == NULL)
                return false;

        return ply_terminal_is_active (devices->local_console_terminal);
}

bool
plymouthd_devices_has_vt_console (plymouthd_devices_t *devices)
{
        if (devices->local_console_terminal == NULL)
                return false;

        return ply_terminal_is_vt (devices->local_console_terminal);
}

void
plymouthd_devices_for_each_keyboard (
        plymouthd_devices_t                 *devices,
        plymouthd_devices_keyboard_handler_t handler,
        void                                *user_data)
{
        ply_list_t *keyboards;
        ply_list_node_t *node;

        keyboards = ply_device_manager_get_keyboards (devices->device_manager);
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
plymouthd_devices_for_each_pixel_display (
        plymouthd_devices_t                      *devices,
        plymouthd_devices_pixel_display_handler_t handler,
        void                                     *user_data)
{
        ply_list_t *pixel_displays;
        ply_list_node_t *node;

        pixel_displays =
                ply_device_manager_get_pixel_displays (devices->device_manager);
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
plymouthd_devices_for_each_text_display (
        plymouthd_devices_t                     *devices,
        plymouthd_devices_text_display_handler_t handler,
        void                                    *user_data)
{
        ply_list_t *text_displays;
        ply_list_node_t *node;

        text_displays =
                ply_device_manager_get_text_displays (devices->device_manager);
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
plymouthd_devices_activate_renderers (plymouthd_devices_t *devices)
{
        ply_device_manager_activate_renderers (devices->device_manager);
}

void
plymouthd_devices_deactivate_renderers (plymouthd_devices_t *devices)
{
        ply_device_manager_deactivate_renderers (devices->device_manager);
}

void
plymouthd_devices_activate_keyboards (plymouthd_devices_t *devices)
{
        ply_device_manager_activate_keyboards (devices->device_manager);
}

void
plymouthd_devices_deactivate_keyboards (plymouthd_devices_t *devices)
{
        ply_device_manager_deactivate_keyboards (devices->device_manager);
}

void
plymouthd_devices_prepare_console (plymouthd_devices_t *devices)
{
        if (devices->local_console_terminal == NULL)
                return;

        ply_terminal_set_mode (devices->local_console_terminal,
                               PLY_TERMINAL_MODE_GRAPHICS);
}

void
plymouthd_devices_restore_text_console (plymouthd_devices_t *devices)
{
        if (devices->local_console_terminal == NULL)
                return;

        ply_terminal_set_mode (devices->local_console_terminal,
                               PLY_TERMINAL_MODE_TEXT);
        ply_terminal_set_buffered_input (devices->local_console_terminal);
}

void
plymouthd_devices_release_console (plymouthd_devices_t *devices)
{
        if (devices->local_console_terminal == NULL)
                return;

        ply_trace ("Not retaining splash, so deallocating VT");
        ply_terminal_deactivate_vt (devices->local_console_terminal);
        ply_terminal_close (devices->local_console_terminal);
}

void
plymouthd_devices_deactivate_console (plymouthd_devices_t *devices)
{
        if (devices->local_console_terminal == NULL)
                return;

        ply_trace ("deactivating terminal");
        ply_terminal_stop_watching_for_vt_changes (
                devices->local_console_terminal);
        ply_terminal_set_buffered_input (devices->local_console_terminal);
        ply_terminal_close (devices->local_console_terminal);
}

void
plymouthd_devices_reactivate_console (plymouthd_devices_t *devices)
{
        if (devices->local_console_terminal == NULL)
                return;

        ply_terminal_open (devices->local_console_terminal);
        ply_terminal_watch_for_vt_changes (devices->local_console_terminal);
        ply_terminal_set_unbuffered_input (devices->local_console_terminal);
        ply_terminal_ignore_mode_changes (devices->local_console_terminal,
                                          false);
}

void
plymouthd_devices_pause (plymouthd_devices_t *devices)
{
        ply_device_manager_pause (devices->device_manager);
}

void
plymouthd_devices_unpause (plymouthd_devices_t *devices)
{
        ply_device_manager_unpause (devices->device_manager);
}

void
plymouthd_devices_free (plymouthd_devices_t *devices)
{
        if (devices == NULL)
                return;

        ply_device_manager_free (devices->device_manager);
        free (devices);
}

static void
on_keyboard_added (void           *user_data,
                   ply_keyboard_t *keyboard)
{
        plymouthd_devices_t *devices = user_data;

        devices->event_handlers.keyboard_added (devices->event_user_data,
                                                keyboard);
}

static void
on_keyboard_removed (void           *user_data,
                     ply_keyboard_t *keyboard)
{
        plymouthd_devices_t *devices = user_data;

        devices->event_handlers.keyboard_removed (devices->event_user_data,
                                                  keyboard);
}

static void
on_pixel_display_added (void                *user_data,
                        ply_pixel_display_t *display)
{
        plymouthd_devices_t *devices = user_data;

        devices->event_handlers.pixel_display_added (devices->event_user_data,
                                                     display);
}

static void
on_pixel_display_removed (void                *user_data,
                          ply_pixel_display_t *display)
{
        plymouthd_devices_t *devices = user_data;

        devices->event_handlers.pixel_display_removed (devices->event_user_data,
                                                       display);
}

static void
on_text_display_added (void               *user_data,
                       ply_text_display_t *display)
{
        plymouthd_devices_t *devices = user_data;

        devices->event_handlers.text_display_added (devices->event_user_data,
                                                    display);
}

static void
on_text_display_removed (void               *user_data,
                         ply_text_display_t *display)
{
        plymouthd_devices_t *devices = user_data;

        devices->event_handlers.text_display_removed (devices->event_user_data,
                                                      display);
}

plymouthd_devices_t *
plymouthd_devices_new (const char                               *default_tty,
                       ply_device_manager_flags_t                flags,
                       xkb_keysym_t                              extra_escape_key,
                       double                                    device_timeout,
                       const plymouthd_devices_event_handlers_t *event_handlers,
                       void                                     *event_user_data)
{
        plymouthd_devices_t *devices;

        devices = calloc (1, sizeof(plymouthd_devices_t));
        devices->event_handlers = *event_handlers;
        devices->event_user_data = event_user_data;
        devices->device_manager = ply_device_manager_new (default_tty,
                                                          flags,
                                                          extra_escape_key);
        devices->local_console_terminal =
                ply_device_manager_get_default_terminal (devices->device_manager);

        ply_device_manager_watch_devices (
                devices->device_manager,
                device_timeout,
                on_keyboard_added,
                on_keyboard_removed,
                on_pixel_display_added,
                on_pixel_display_removed,
                on_text_display_added,
                on_text_display_removed,
                devices);

        return devices;
}
