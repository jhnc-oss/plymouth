/* ply-bgrt-private.h - internal firmware logo metadata reader
 *
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#ifndef PLY_BGRT_PRIVATE_H
#define PLY_BGRT_PRIVATE_H

#include <stdbool.h>

#include "ply-pixel-buffer.h"
#include "ply-private.h"

typedef struct
{
        int                         x_offset;
        int                         y_offset;
        ply_pixel_buffer_rotation_t rotation;
} ply_bgrt_info_t;

PLY_PRIVATE bool ply_bgrt_info_load (const char      *directory,
                                     ply_bgrt_info_t *info);

#endif /* PLY_BGRT_PRIVATE_H */
