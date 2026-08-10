/* plymouthd-devices-private.h - internal device coordination
 *
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#ifndef PLYMOUTHD_DEVICES_PRIVATE_H
#define PLYMOUTHD_DEVICES_PRIVATE_H

#include <stdbool.h>

#include <xkbcommon/xkbcommon.h>

#include "ply-device-manager.h"
#include "ply-private.h"

typedef struct _ply_keyboard ply_keyboard_t;
typedef struct _ply_pixel_display ply_pixel_display_t;
typedef struct _ply_text_display ply_text_display_t;
typedef struct _plymouthd_devices plymouthd_devices_t;

typedef void (*plymouthd_devices_keyboard_handler_t)(ply_keyboard_t *keyboard,
                                                     void           *user_data);
typedef void (*plymouthd_devices_pixel_display_handler_t)(ply_pixel_display_t *display,
                                                          void                *user_data);
typedef void (*plymouthd_devices_text_display_handler_t)(ply_text_display_t *display,
                                                         void               *user_data);

typedef struct
{
        void (*keyboard_added)(void           *user_data,
                               ply_keyboard_t *keyboard);
        void (*keyboard_removed)(void           *user_data,
                                 ply_keyboard_t *keyboard);
        void (*pixel_display_added)(void                *user_data,
                                    ply_pixel_display_t *display);
        void (*pixel_display_removed)(void                *user_data,
                                      ply_pixel_display_t *display);
        void (*text_display_added)(void               *user_data,
                                   ply_text_display_t *display);
        void (*text_display_removed)(void               *user_data,
                                     ply_text_display_t *display);
} plymouthd_devices_event_handlers_t;

PLY_PRIVATE plymouthd_devices_t *plymouthd_devices_new (const char                               *default_tty,
                                                        ply_device_manager_flags_t                flags,
                                                        xkb_keysym_t                              extra_escape_key,
                                                        double                                    device_timeout,
                                                        const plymouthd_devices_event_handlers_t *event_handlers,
                                                        void                                     *event_user_data);
PLY_PRIVATE bool plymouthd_devices_has_displays (plymouthd_devices_t *devices);
PLY_PRIVATE bool plymouthd_devices_has_serial_consoles (plymouthd_devices_t *devices);
PLY_PRIVATE bool plymouthd_devices_has_active_vt (plymouthd_devices_t *devices);
PLY_PRIVATE bool plymouthd_devices_has_vt_console (plymouthd_devices_t *devices);
PLY_PRIVATE void plymouthd_devices_for_each_keyboard (plymouthd_devices_t                 *devices,
                                                      plymouthd_devices_keyboard_handler_t handler,
                                                      void                                *user_data);
PLY_PRIVATE void plymouthd_devices_for_each_pixel_display (plymouthd_devices_t                      *devices,
                                                           plymouthd_devices_pixel_display_handler_t handler,
                                                           void                                     *user_data);
PLY_PRIVATE void plymouthd_devices_for_each_text_display (plymouthd_devices_t                     *devices,
                                                          plymouthd_devices_text_display_handler_t handler,
                                                          void                                    *user_data);
PLY_PRIVATE void plymouthd_devices_activate_renderers (plymouthd_devices_t *devices);
PLY_PRIVATE void plymouthd_devices_deactivate_renderers (plymouthd_devices_t *devices);
PLY_PRIVATE void plymouthd_devices_activate_keyboards (plymouthd_devices_t *devices);
PLY_PRIVATE void plymouthd_devices_deactivate_keyboards (plymouthd_devices_t *devices);
PLY_PRIVATE void plymouthd_devices_prepare_console (plymouthd_devices_t *devices);
PLY_PRIVATE void plymouthd_devices_restore_text_console (plymouthd_devices_t *devices);
PLY_PRIVATE void plymouthd_devices_release_console (plymouthd_devices_t *devices);
PLY_PRIVATE void plymouthd_devices_deactivate_console (plymouthd_devices_t *devices);
PLY_PRIVATE void plymouthd_devices_reactivate_console (plymouthd_devices_t *devices);
PLY_PRIVATE void plymouthd_devices_pause (plymouthd_devices_t *devices);
PLY_PRIVATE void plymouthd_devices_unpause (plymouthd_devices_t *devices);
PLY_PRIVATE void plymouthd_devices_free (plymouthd_devices_t *devices);

#endif /* PLYMOUTHD_DEVICES_PRIVATE_H */
