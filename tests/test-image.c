/*
 * Copyright (C) 2026 Red Hat, Inc.
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
 */

#include "ply-test.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ply-image.h"
#include "ply-pixel-buffer.h"

typedef bool (*fixture_test_function_t) (const char *path);

static const uint8_t rgba_png[] = {
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d,
        0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01,
        0x08, 0x06, 0x00, 0x00, 0x00, 0xf4, 0x22, 0x7f, 0x8a, 0x00, 0x00, 0x00,
        0x11, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9c, 0x63, 0x10, 0x54, 0x32, 0xfe,
        0xff, 0xbf, 0x81, 0xa1, 0x01, 0x00, 0x0d, 0xa8, 0x03, 0x65, 0xc1, 0xf2,
        0xb1, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42,
        0x60, 0x82,
};

static const uint8_t bottom_up_bmp[] = {
        0x42, 0x4d, 0x46, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x36, 0x00,
        0x00, 0x00, 0x28, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02, 0x00,
        0x00, 0x00, 0x01, 0x00, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0xff, 0x00, 0x00, 0xff, 0xff, 0xff, 0x00, 0x00,
        0x00, 0x00, 0xff, 0x00, 0xff, 0x00, 0x00, 0x00,
};

static bool
write_all (int            fd,
           const uint8_t *data,
           size_t         size)
{
        size_t offset = 0;

        while (offset < size) {
                ssize_t bytes_written;

                bytes_written = write (fd, data + offset, size - offset);
                if (bytes_written < 0) {
                        if (errno == EINTR)
                                continue;

                        return false;
                }

                if (bytes_written == 0)
                        return false;

                offset += (size_t) bytes_written;
        }

        return true;
}

static bool
with_fixture (const uint8_t          *data,
              size_t                  size,
              fixture_test_function_t test_function)
{
        char path[] = "/tmp/plymouth-image-test-XXXXXX";
        bool passed;
        int fd;

        fd = mkstemp (path);
        if (fd < 0)
                return false;

        if (!write_all (fd, data, size) || close (fd) < 0) {
                unlink (path);
                return false;
        }

        passed = test_function (path);
        if (unlink (path) < 0)
                passed = false;

        return passed;
}

static void
set_uint32_le (uint8_t  *bytes,
               uint32_t value)
{
        bytes[0] = (uint8_t) value;
        bytes[1] = (uint8_t) (value >> 8);
        bytes[2] = (uint8_t) (value >> 16);
        bytes[3] = (uint8_t) (value >> 24);
}

static bool
fixture_decodes_rgba_png (const char *path)
{
        ply_image_t *image;
        uint32_t *pixels;

        image = ply_image_new (path);
        PLY_TEST_ASSERT (ply_image_load (image));
        PLY_TEST_ASSERT (ply_image_get_width (image) == 2);
        PLY_TEST_ASSERT (ply_image_get_height (image) == 1);

        pixels = ply_image_get_data (image);
        PLY_TEST_ASSERT (pixels[0] == UINT32_C (0xff112233));
        PLY_TEST_ASSERT (pixels[1] == UINT32_C (0x80804000));

        ply_image_free (image);
        return true;
}

static bool
test_png_decodes_rgba_pixels (void)
{
        return with_fixture (rgba_png,
                             sizeof(rgba_png),
                             fixture_decodes_rgba_png);
}

static bool
fixture_is_rejected (const char *path)
{
        ply_image_t *image;
        bool rejected;

        image = ply_image_new (path);
        rejected = !ply_image_load (image);
        ply_image_free (image);

        return rejected;
}

static bool
test_truncated_png_is_rejected (void)
{
        return with_fixture (rgba_png,
                             sizeof(rgba_png) - 20,
                             fixture_is_rejected);
}

static bool
fixture_decodes_bmp_pixels (const char *path)
{
        ply_image_t *image;
        ply_pixel_buffer_t *buffer;
        uint32_t *pixels;

        image = ply_image_new (path);
        PLY_TEST_ASSERT (ply_image_load (image));
        PLY_TEST_ASSERT (ply_image_get_width (image) == 2);
        PLY_TEST_ASSERT (ply_image_get_height (image) == 2);

        buffer = ply_image_get_buffer (image);
        pixels = ply_image_get_data (image);
        PLY_TEST_ASSERT (ply_pixel_buffer_is_opaque (buffer));
        PLY_TEST_ASSERT (pixels[0] == UINT32_C (0xffff0000));
        PLY_TEST_ASSERT (pixels[1] == UINT32_C (0xff00ff00));
        PLY_TEST_ASSERT (pixels[2] == UINT32_C (0xff0000ff));
        PLY_TEST_ASSERT (pixels[3] == UINT32_C (0xffffffff));

        ply_image_free (image);
        return true;
}

static bool
test_bmp_decodes_bottom_up_rows (void)
{
        return with_fixture (bottom_up_bmp,
                             sizeof(bottom_up_bmp),
                             fixture_decodes_bmp_pixels);
}

static bool
test_bmp_decodes_top_down_rows (void)
{
        uint8_t top_down_bmp[sizeof(bottom_up_bmp)];
        uint8_t row[8];

        memcpy (top_down_bmp, bottom_up_bmp, sizeof(top_down_bmp));
        set_uint32_le (&top_down_bmp[22], (uint32_t) -2);

        memcpy (row, &top_down_bmp[54], sizeof(row));
        memcpy (&top_down_bmp[54], &top_down_bmp[62], sizeof(row));
        memcpy (&top_down_bmp[62], row, sizeof(row));

        return with_fixture (top_down_bmp,
                             sizeof(top_down_bmp),
                             fixture_decodes_bmp_pixels);
}

static bool
test_truncated_bmp_is_rejected (void)
{
        return with_fixture (bottom_up_bmp,
                             sizeof(bottom_up_bmp) - 1,
                             fixture_is_rejected);
}

static bool
test_bmp_offset_inside_headers_is_rejected (void)
{
        uint8_t malformed_bmp[sizeof(bottom_up_bmp)];

        memcpy (malformed_bmp, bottom_up_bmp, sizeof(malformed_bmp));
        set_uint32_le (&malformed_bmp[10], 53);

        return with_fixture (malformed_bmp,
                             sizeof(malformed_bmp),
                             fixture_is_rejected);
}

static bool
test_invalid_bmp_headers_are_rejected (void)
{
        uint8_t malformed_bmp[sizeof(bottom_up_bmp)];

        memcpy (malformed_bmp, bottom_up_bmp, sizeof(malformed_bmp));
        malformed_bmp[28] = 32;
        PLY_TEST_ASSERT (with_fixture (malformed_bmp,
                                      sizeof(malformed_bmp),
                                      fixture_is_rejected));

        memcpy (malformed_bmp, bottom_up_bmp, sizeof(malformed_bmp));
        set_uint32_le (&malformed_bmp[18], 0);
        PLY_TEST_ASSERT (with_fixture (malformed_bmp,
                                      sizeof(malformed_bmp),
                                      fixture_is_rejected));

        memcpy (malformed_bmp, bottom_up_bmp, sizeof(malformed_bmp));
        set_uint32_le (&malformed_bmp[22], UINT32_C (0x80000000));
        PLY_TEST_ASSERT (with_fixture (malformed_bmp,
                                      sizeof(malformed_bmp),
                                      fixture_is_rejected));

        memcpy (malformed_bmp, bottom_up_bmp, sizeof(malformed_bmp));
        set_uint32_le (&malformed_bmp[18], UINT32_C (65536));
        PLY_TEST_ASSERT (with_fixture (malformed_bmp,
                                      sizeof(malformed_bmp),
                                      fixture_is_rejected));

        return true;
}

static bool
fixture_transforms_and_transfers_image (const char *path)
{
        ply_image_t *image;
        ply_image_t *resized;
        ply_image_t *rotated;
        ply_image_t *tiled;
        ply_pixel_buffer_t *buffer;
        uint32_t *pixels;

        image = ply_image_new (path);
        PLY_TEST_ASSERT (ply_image_load (image));

        tiled = ply_image_tile (image, 3, 3);
        PLY_TEST_ASSERT (ply_image_get_width (tiled) == 3);
        PLY_TEST_ASSERT (ply_image_get_height (tiled) == 3);
        pixels = ply_image_get_data (tiled);
        PLY_TEST_ASSERT (pixels[0] == UINT32_C (0xffff0000));
        PLY_TEST_ASSERT (pixels[4] == UINT32_C (0xffffffff));
        PLY_TEST_ASSERT (pixels[8] == UINT32_C (0xffff0000));

        resized = ply_image_resize (image, 3, 3);
        pixels = ply_image_get_data (resized);
        PLY_TEST_ASSERT (pixels[0] == UINT32_C (0xffff0000));
        PLY_TEST_ASSERT (pixels[2] == UINT32_C (0xff00ff00));
        PLY_TEST_ASSERT (pixels[6] == UINT32_C (0xff0000ff));
        PLY_TEST_ASSERT (pixels[8] == UINT32_C (0xffffffff));

        rotated = ply_image_rotate (image, 0, 0, 0.0);
        PLY_TEST_ASSERT (memcmp (ply_image_get_data (rotated),
                                 ply_image_get_data (image),
                                 4 * sizeof(uint32_t)) == 0);

        buffer = ply_image_convert_to_pixel_buffer (image);
        PLY_TEST_ASSERT (ply_pixel_buffer_get_width (buffer) == 2);
        PLY_TEST_ASSERT (ply_pixel_buffer_get_height (buffer) == 2);
        PLY_TEST_ASSERT (ply_pixel_buffer_get_argb32_data (buffer)[3] ==
                         UINT32_C (0xffffffff));

        ply_pixel_buffer_free (buffer);
        ply_image_free (rotated);
        ply_image_free (resized);
        ply_image_free (tiled);
        return true;
}

static bool
test_image_wrappers_transform_and_transfer (void)
{
        return with_fixture (bottom_up_bmp,
                             sizeof(bottom_up_bmp),
                             fixture_transforms_and_transfers_image);
}

static const ply_test_case_t test_cases[] =
{
        PLY_TEST_CASE (test_png_decodes_rgba_pixels),
        PLY_TEST_CASE (test_truncated_png_is_rejected),
        PLY_TEST_CASE (test_bmp_decodes_bottom_up_rows),
        PLY_TEST_CASE (test_bmp_decodes_top_down_rows),
        PLY_TEST_CASE (test_truncated_bmp_is_rejected),
        PLY_TEST_CASE (test_bmp_offset_inside_headers_is_rejected),
        PLY_TEST_CASE (test_invalid_bmp_headers_are_rejected),
        PLY_TEST_CASE (test_image_wrappers_transform_and_transfer),
};

PLY_TEST_MAIN (test_cases)
