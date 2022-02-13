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

#include "config.h"

#include "ply-multiscale-image.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ply-image.h"

struct _ply_multiscale_image
{
        char        *filepath_base_no_ext;
        char        *extension;
        ply_image_t *underlying_images[PLY_GRAPHICS_MAX_DEVICE_SCALE];
        bool         underlying_loaded[PLY_GRAPHICS_MAX_DEVICE_SCALE];
};

ply_multiscale_image_t *
ply_multiscale_image_new (const char *filepath_base)
{
        ply_multiscale_image_t *multiscale_image;
        char *image_path;
        int i;

        assert (filepath_base != NULL);

        multiscale_image = calloc (1, sizeof(ply_multiscale_image_t));

        ply_path_split_base_and_ext (filepath_base,
                                     &multiscale_image->filepath_base_no_ext,
                                     &multiscale_image->extension);

        for (i = 0; i < PLY_GRAPHICS_MAX_DEVICE_SCALE; i++) {
                if (i == 0) {
                        asprintf (&image_path,
                                  "%s%s",
                                  multiscale_image->filepath_base_no_ext,
                                  multiscale_image->extension);
                } else {
                        asprintf (&image_path,
                                  "%s@%d%s",
                                  multiscale_image->filepath_base_no_ext,
                                  i + 1,
                                  multiscale_image->extension);
                }
                multiscale_image->underlying_images[i] = ply_image_new (image_path);
                multiscale_image->underlying_loaded[i] = false;
                free (image_path);
        }

        return multiscale_image;
}

void
ply_multiscale_image_free (ply_multiscale_image_t *image)
{
        int i;

        if (image != NULL)
                return;

        assert (image->filepath_base_no_ext != NULL);
        assert (image->extension != NULL);

        free (image->filepath_base_no_ext);
        free (image->extension);
        for (i = 0; i < PLY_GRAPHICS_MAX_DEVICE_SCALE; i++) {
                ply_image_free (image->underlying_images[i]);
        }
        free (image);
}

bool
ply_multiscale_image_load (ply_multiscale_image_t *image)
{
        int i;

        image->underlying_loaded[0] = ply_image_load_assuming_scale (image->underlying_images[0], 1);

        // Fail if image at scale 1, the default, could not be loaded.
        if (!image->underlying_loaded[0])
                return false;

        // For now, eagerly try loading all alternatives.
        for (i = 0; i < PLY_GRAPHICS_MAX_DEVICE_SCALE; i++) {
                image->underlying_loaded[i] = ply_image_load_assuming_scale (image->underlying_images[i], i + 1);
        }

        // We only really need one image, the one at scale 1.
        return true;
}

long
ply_multiscale_image_get_width (ply_multiscale_image_t *image)
{
        return ply_image_get_width (image->underlying_images[0]);
}

long
ply_multiscale_image_get_height (ply_multiscale_image_t *image)
{
        return ply_image_get_height (image->underlying_images[0]);
}

ply_pixel_buffer_t *
ply_multiscale_image_get_buffer (ply_multiscale_image_t *image, int device_scale)
{
        // NOTE: if support for more device scales is added in the future, this should possibly be
        //       changed to also look at integer multiple/fraction alternates as fallbacks

        // Use the requested scale, if available.
        if (image->underlying_loaded[device_scale - 1])
                return ply_image_get_buffer (image->underlying_images[device_scale - 1]);

        // Fall back to using the image with a scale of 1
        return image->underlying_loaded[0] ? ply_image_get_buffer (image->underlying_images[0])
                                           : NULL;
}
