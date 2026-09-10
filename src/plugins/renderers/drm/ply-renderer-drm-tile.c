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
