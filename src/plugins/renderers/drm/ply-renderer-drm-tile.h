/*
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#ifndef PLY_RENDERER_DRM_TILE_H
#define PLY_RENDERER_DRM_TILE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct
{
        uint32_t group_id;
        uint32_t num_h;
        uint32_t num_v;
        uint32_t h_loc;
        uint32_t v_loc;
        uint32_t h_size;
        uint32_t v_size;
        bool     is_single_monitor;
} ply_renderer_drm_tile_info_t;

bool ply_renderer_drm_tile_info_parse (const void                   *data,
                                       size_t                        size,
                                       ply_renderer_drm_tile_info_t *tile_info);

#endif
