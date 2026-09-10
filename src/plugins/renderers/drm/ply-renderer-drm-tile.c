/*
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#include "ply-renderer-drm-tile.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool
ply_renderer_drm_tile_info_parse (const void                   *data,
                                  size_t                        size,
                                  ply_renderer_drm_tile_info_t *tile_info)
{
        char *tile_data;
        const char *bytes = data;
        size_t text_size;
        unsigned int group_id, single_monitor, num_h, num_v;
        unsigned int h_loc, v_loc, h_size, v_size;
        int parsed_length = 0;
        bool parsed = false;

        if (data == NULL || size == 0 || tile_info == NULL)
                return false;

        text_size = strnlen (bytes, size);
        if (text_size < size && text_size + 1 != size)
                return false;

        tile_data = strndup (bytes, text_size);
        if (tile_data == NULL)
                return false;

        if (sscanf (tile_data, "%u:%u:%u:%u:%u:%u:%u:%u%n",
                    &group_id, &single_monitor, &num_h, &num_v,
                    &h_loc, &v_loc, &h_size, &v_size, &parsed_length) == 8 &&
            tile_data[parsed_length] == '\0' &&
            group_id > 0 && single_monitor <= 1 &&
            num_h > 0 && num_v > 0 &&
            h_loc < num_h && v_loc < num_v &&
            h_size > 0 && v_size > 0) {
                tile_info->group_id = group_id;
                tile_info->is_single_monitor = single_monitor;
                tile_info->num_h = num_h;
                tile_info->num_v = num_v;
                tile_info->h_loc = h_loc;
                tile_info->v_loc = v_loc;
                tile_info->h_size = h_size;
                tile_info->v_size = v_size;
                parsed = true;
        }

        free (tile_data);
        return parsed;
}

bool
ply_renderer_drm_modes_are_equal (const drmModeModeInfo *a,
                                  const drmModeModeInfo *b)
{
        return a->clock == b->clock &&
               a->hdisplay == b->hdisplay &&
               a->hsync_start == b->hsync_start &&
               a->hsync_end == b->hsync_end &&
               a->htotal == b->htotal &&
               a->hskew == b->hskew &&
               a->vdisplay == b->vdisplay &&
               a->vsync_start == b->vsync_start &&
               a->vsync_end == b->vsync_end &&
               a->vtotal == b->vtotal &&
               a->vscan == b->vscan &&
               a->vrefresh == b->vrefresh &&
               a->flags == b->flags &&
               a->type == b->type;
}

drmModeModeInfo *
ply_renderer_drm_find_tile_mode (drmModeModeInfo                    *modes,
                                 size_t                              mode_count,
                                 const ply_renderer_drm_tile_info_t *tile_info)
{
        size_t i;

        if (modes == NULL || tile_info == NULL)
                return NULL;

        for (i = 0; i < mode_count; i++) {
                if (modes[i].hdisplay == tile_info->h_size &&
                    modes[i].vdisplay == tile_info->v_size)
                        return &modes[i];
        }

        return NULL;
}

static bool
tile_info_is_valid (const ply_renderer_drm_tile_info_t *tile)
{
        return tile->group_id > 0 &&
               tile->num_h > 0 && tile->num_v > 0 &&
               tile->h_loc < tile->num_h && tile->v_loc < tile->num_v &&
               tile->h_size > 0 && tile->v_size > 0;
}

static bool
tile_layouts_match (const ply_renderer_drm_tile_info_t *a,
                    const ply_renderer_drm_tile_info_t *b)
{
        return a->group_id == b->group_id &&
               a->is_single_monitor && b->is_single_monitor &&
               a->num_h == b->num_h &&
               a->num_v == b->num_v &&
               a->h_size == b->h_size &&
               a->v_size == b->v_size;
}

bool
ply_renderer_drm_tile_group_get_geometry (const ply_renderer_drm_tile_output_t *outputs,
                                          size_t                                output_count,
                                          size_t                                output_index,
                                          uint32_t                              max_width,
                                          uint32_t                              max_height,
                                          ply_renderer_drm_tile_geometry_t     *geometry)
{
        const ply_renderer_drm_tile_output_t *output;
        uint64_t occupied_tiles = 0;
        uint64_t expected_tiles;
        size_t i, count = 0;

        if (outputs == NULL || output_index >= output_count || geometry == NULL)
                return false;

        output = &outputs[output_index];
        if (!output->connected || output->controller_id == 0 ||
            !output->tiled || !output->tile.is_single_monitor ||
            !tile_info_is_valid (&output->tile) ||
            output->uses_hw_rotation || !output->upright_rotation ||
            output->mode.hdisplay != output->tile.h_size ||
            output->mode.vdisplay != output->tile.v_size)
                return false;

        expected_tiles = (uint64_t) output->tile.num_h * output->tile.num_v;
        if (expected_tiles == 0 || expected_tiles > 64 ||
            output->tile.num_h > UINT32_MAX / output->tile.h_size ||
            output->tile.num_v > UINT32_MAX / output->tile.v_size ||
            output->tile.num_h * output->tile.h_size > max_width ||
            output->tile.num_v * output->tile.v_size > max_height)
                return false;

        for (i = 0; i < output_count; i++) {
                uint32_t tile_index;
                size_t j;

                if (!outputs[i].tiled ||
                    outputs[i].tile.group_id != output->tile.group_id)
                        continue;

                if (!tile_layouts_match (&output->tile, &outputs[i].tile) ||
                    !tile_info_is_valid (&outputs[i].tile) ||
                    !outputs[i].connected || outputs[i].controller_id == 0 ||
                    outputs[i].uses_hw_rotation || !outputs[i].upright_rotation ||
                    outputs[i].device_scale != output->device_scale ||
                    outputs[i].mode.hdisplay != output->tile.h_size ||
                    outputs[i].mode.vdisplay != output->tile.v_size ||
                    !ply_renderer_drm_modes_are_equal (&outputs[i].mode, &output->mode))
                        return false;

                for (j = 0; j < i; j++) {
                        if (outputs[j].tiled &&
                            outputs[j].tile.group_id == output->tile.group_id &&
                            outputs[j].controller_id == outputs[i].controller_id)
                                return false;
                }

                tile_index = outputs[i].tile.v_loc * output->tile.num_h +
                             outputs[i].tile.h_loc;
                if (tile_index >= expected_tiles ||
                    occupied_tiles & (UINT64_C (1) << tile_index))
                        return false;

                occupied_tiles |= UINT64_C (1) << tile_index;
                count++;
        }

        if (count != expected_tiles)
                return false;

        geometry->width = output->tile.num_h * output->tile.h_size;
        geometry->height = output->tile.num_v * output->tile.v_size;
        geometry->x = output->tile.h_loc * output->tile.h_size;
        geometry->y = output->tile.v_loc * output->tile.v_size;
        return true;
}
