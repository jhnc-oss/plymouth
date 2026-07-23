/* ply-bgrt.c - internal firmware logo metadata reader
 *
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#include "ply-bgrt-private.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#define BGRT_STATUS_ORIENTATION_OFFSET_0    (0 << 1)
#define BGRT_STATUS_ORIENTATION_OFFSET_90   (1 << 1)
#define BGRT_STATUS_ORIENTATION_OFFSET_180  (2 << 1)
#define BGRT_STATUS_ORIENTATION_OFFSET_270  (3 << 1)
#define BGRT_STATUS_ORIENTATION_OFFSET_MASK (3 << 1)

static bool
read_integer_file (const char *directory,
                   const char *name,
                   int        *value)
{
        char buffer[64];
        char *end;
        char *path = NULL;
        long parsed_value;
        FILE *file;

        asprintf (&path, "%s/%s", directory, name);
        file = fopen (path, "r");
        free (path);

        if (file == NULL)
                return false;

        if (fgets (buffer, sizeof(buffer), file) == NULL) {
                fclose (file);
                return false;
        }

        fclose (file);

        errno = 0;
        parsed_value = strtol (buffer, &end, 10);
        if (errno != 0 || end == buffer ||
            parsed_value < INT_MIN || parsed_value > INT_MAX)
                return false;

        while (isspace ((unsigned char) *end)) {
                end++;
        }

        if (*end != '\0')
                return false;

        *value = (int) parsed_value;
        return true;
}

bool
ply_bgrt_info_load (const char      *directory,
                    ply_bgrt_info_t *info)
{
        ply_pixel_buffer_rotation_t rotation;
        int status;
        int x_offset;
        int y_offset;

        if (!read_integer_file (directory, "status", &status) ||
            !read_integer_file (directory, "xoffset", &x_offset) ||
            !read_integer_file (directory, "yoffset", &y_offset))
                return false;

        switch (status & BGRT_STATUS_ORIENTATION_OFFSET_MASK) {
        case BGRT_STATUS_ORIENTATION_OFFSET_0:
                rotation = PLY_PIXEL_BUFFER_ROTATE_UPRIGHT;
                break;
        case BGRT_STATUS_ORIENTATION_OFFSET_90:
                rotation = PLY_PIXEL_BUFFER_ROTATE_COUNTER_CLOCKWISE;
                break;
        case BGRT_STATUS_ORIENTATION_OFFSET_180:
                rotation = PLY_PIXEL_BUFFER_ROTATE_UPSIDE_DOWN;
                break;
        case BGRT_STATUS_ORIENTATION_OFFSET_270:
                rotation = PLY_PIXEL_BUFFER_ROTATE_CLOCKWISE;
                break;
        }

        info->x_offset = x_offset;
        info->y_offset = y_offset;
        info->rotation = rotation;
        return true;
}
