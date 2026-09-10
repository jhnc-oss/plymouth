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
