/* ply-image-sequence.h - management for sequences of images
 *
 * Copyright (C) 2022 Hans Christian Schmitz
 *
 * Based on the code of ply-animation.c from project at commit
 * 6f298131a7326608dfc138fd4e9f56a29f35762f
 * Copyright (C) 2009 Red Hat, Inc.
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
 * Written by: Hans Christian Schmitz <git@hcsch.eu>
 */
#ifndef IMAGE_SEQUENCE_H
#define IMAGE_SEQUENCE_H

#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>

#include "ply-pixel-display.h"

typedef struct _ply_image_sequence ply_image_sequence_t;

#ifndef PLY_HIDE_FUNCTION_DECLARATIONS
ply_image_sequence_t *ply_image_sequence_new (const char *image_dir,
                                              const char *frames_prefix,
                                              int         device_scale);
void ply_image_sequence_free (ply_image_sequence_t *image_sequence);

bool ply_image_sequence_load (ply_image_sequence_t *image_sequence);

void ply_image_sequence_draw_area (ply_image_sequence_t *image_sequence,
                                   int                   frame_index,
                                   ply_pixel_buffer_t   *buffer,
                                   long                  x,
                                   long                  y,
                                   unsigned long         width,
                                   unsigned long         height);

long ply_image_sequence_get_width (ply_image_sequence_t *image_sequence);
long ply_image_sequence_get_height (ply_image_sequence_t *image_sequence);
int ply_image_sequence_get_number_of_frames (ply_image_sequence_t *image_sequence);
#endif

#endif /* IMAGE_SEQUENCE_H */
/* vim: set ts=4 sw=4 expandtab autoindent cindent cino={.5s,(0: */
