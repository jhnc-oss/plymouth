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

#include <xf86drmMode.h>

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

typedef struct
{
        ply_renderer_drm_tile_info_t tile;
        drmModeModeInfo              mode;
        uint32_t                     controller_id;
        int                          device_scale;
        bool                         tiled;
        bool                         connected;
        bool                         upright_rotation;
        bool                         uses_hw_rotation;
} ply_renderer_drm_tile_output_t;

typedef struct
{
        uint32_t width;
        uint32_t height;
        uint32_t x;
        uint32_t y;
} ply_renderer_drm_tile_geometry_t;

bool ply_renderer_drm_tile_info_parse (const void                   *data,
                                       size_t                        size,
                                       ply_renderer_drm_tile_info_t *tile_info);
bool ply_renderer_drm_modes_are_equal (const drmModeModeInfo *a,
                                       const drmModeModeInfo *b);
drmModeModeInfo *ply_renderer_drm_find_tile_mode (drmModeModeInfo                    *modes,
                                                  size_t                              mode_count,
                                                  const ply_renderer_drm_tile_info_t *tile_info);
bool ply_renderer_drm_tile_group_get_geometry (const ply_renderer_drm_tile_output_t *outputs,
                                               size_t                                output_count,
                                               size_t                                output_index,
                                               uint32_t                              max_width,
                                               uint32_t                              max_height,
                                               ply_renderer_drm_tile_geometry_t     *geometry);

#endif
