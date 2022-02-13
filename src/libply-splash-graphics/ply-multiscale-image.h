/* ply-multiscale-image.h - wrapper of ply-image for images available in multiple resolutions
 *
 * Copyright (C) 2022 Hans Christian Schmitz.
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
 * Written By: Hans Christian Schmitz <git@hcsch.eu>
 */

#ifndef PLY_MULTISCALE_IMAGE_H
#define PLY_MULTISCALE_IMAGE_H

#include "ply-image.h"

typedef struct _ply_multiscale_image ply_multiscale_image_t;

#ifndef PLY_HIDE_FUNCTION_DECLARATIONS

ply_multiscale_image_t *ply_multiscale_image_new (const char *filepath_base);

void ply_multiscale_image_free (ply_multiscale_image_t *image);

bool ply_multiscale_image_load (ply_multiscale_image_t *image);

long ply_multiscale_image_get_width (ply_multiscale_image_t *image);

long ply_multiscale_image_get_height (ply_multiscale_image_t *image);

ply_pixel_buffer_t *ply_multiscale_image_get_buffer (ply_multiscale_image_t *image,
                                                     int                     device_scale);

#endif

#endif /* PLY_MULTISCALE_IMAGE_H */
/* vim: set ts=4 sw=4 expandtab autoindent cindent cino={.5s,(0: */
