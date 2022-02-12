/* ply-image.h - png file loader
 *
 * Copyright (C) 2022 Hans Christian Schmitz.
 * Copyright (C) 2007 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Written By: Ray Strode <rstrode@redhat.com>
 *             Hans Christian Schmitz <git@hcsch.eu>
 */
#ifndef PLY_IMAGE_H
#define PLY_IMAGE_H

#include "ply-pixel-buffer.h"

#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>

typedef struct _ply_image ply_image_t;

typedef enum _ply_image_lasof_res
{
        PLY_IMAGE_LOAD_FAILED     = 0,
        PLY_IMAGE_LOADED_FALLBACK = 1,
        PLY_IMAGE_LOADED_AT_SCALE = 2,
} ply_image_lasof_res_t;

#ifndef PLY_HIDE_FUNCTION_DECLARATIONS
ply_image_t *ply_image_new (const char *filename);
void ply_image_free (ply_image_t *image);
bool ply_image_load (ply_image_t *image);
bool ply_image_load_assuming_scale (ply_image_t *image,
                                    int          device_scale);
bool ply_image_load_at_scale (ply_image_t *image,
                              int          device_scale);
ply_image_lasof_res_t ply_image_load_at_scale_or_fallback (ply_image_t *image,
                                                           int          device_scale);
const char *ply_image_lasof_res_desc_string (ply_image_lasof_res_t result);
uint32_t *ply_image_get_data (ply_image_t *image);
long ply_image_get_width (ply_image_t *image);
long ply_image_get_height (ply_image_t *image);
ply_image_t *ply_image_resize (ply_image_t *image,
                               long         width,
                               long         height);
ply_image_t *ply_image_rotate (ply_image_t *oldimage,
                               long         center_x,
                               long         center_y,
                               double       theta_offset);
ply_image_t *ply_image_tile (ply_image_t *image,
                             long         width,
                             long         height);
ply_pixel_buffer_t *ply_image_get_buffer (ply_image_t *image);
ply_pixel_buffer_t *ply_image_convert_to_pixel_buffer (ply_image_t *image);

#endif

#endif /* PLY_IMAGE_H */
/* vim: set ts=4 sw=4 expandtab autoindent cindent cino={.5s,(0: */
