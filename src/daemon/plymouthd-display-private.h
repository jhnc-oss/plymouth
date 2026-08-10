/* plymouthd-display-private.h - internal display coordination
 *
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#ifndef PLYMOUTHD_DISPLAY_PRIVATE_H
#define PLYMOUTHD_DISPLAY_PRIVATE_H

#include "ply-private.h"

typedef struct _ply_boot_splash ply_boot_splash_t;
typedef struct _plymouthd plymouthd_t;
typedef struct _ply_pixel_display ply_pixel_display_t;
typedef struct _ply_text_display ply_text_display_t;

PLY_PRIVATE void plymouthd_show_detailed_splash (plymouthd_t *daemon);
PLY_PRIVATE void plymouthd_show_default_splash (plymouthd_t *daemon);
PLY_PRIVATE void plymouthd_cancel_pending_show (plymouthd_t *daemon);
PLY_PRIVATE void plymouthd_show_splash (plymouthd_t *daemon);
PLY_PRIVATE void plymouthd_update_display (plymouthd_t *daemon);
PLY_PRIVATE void plymouthd_attach_pixel_displays_to_splash (plymouthd_t       *daemon,
                                                            ply_boot_splash_t *splash);
PLY_PRIVATE void plymouthd_attach_text_displays_to_splash (plymouthd_t       *daemon,
                                                           ply_boot_splash_t *splash);
PLY_PRIVATE void plymouthd_handle_pixel_display_added (plymouthd_t         *daemon,
                                                       ply_pixel_display_t *display);
PLY_PRIVATE void plymouthd_handle_pixel_display_removed (plymouthd_t         *daemon,
                                                         ply_pixel_display_t *display);
PLY_PRIVATE void plymouthd_handle_text_display_added (plymouthd_t        *daemon,
                                                      ply_text_display_t *display);
PLY_PRIVATE void plymouthd_handle_text_display_removed (plymouthd_t        *daemon,
                                                        ply_text_display_t *display);
PLY_PRIVATE void plymouthd_toggle_details (plymouthd_t *daemon);
PLY_PRIVATE void plymouthd_hide_splash (plymouthd_t *daemon);

#endif /* PLYMOUTHD_DISPLAY_PRIVATE_H */
