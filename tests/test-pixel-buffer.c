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

#include <string.h>

#include "ply-list.h"
#include "ply-pixel-buffer.h"
#include "ply-region.h"

static bool
test_new_buffer_reports_empty_geometry (void)
{
        ply_pixel_buffer_t *buffer;
        ply_rectangle_t size;
        uint32_t *pixels;
        size_t i;

        buffer = ply_pixel_buffer_new (3, 2);
        pixels = ply_pixel_buffer_get_argb32_data (buffer);
        ply_pixel_buffer_get_size (buffer, &size);

        PLY_TEST_ASSERT (size.x == 0);
        PLY_TEST_ASSERT (size.y == 0);
        PLY_TEST_ASSERT (size.width == 3);
        PLY_TEST_ASSERT (size.height == 2);
        PLY_TEST_ASSERT (ply_pixel_buffer_get_width (buffer) == 3);
        PLY_TEST_ASSERT (ply_pixel_buffer_get_height (buffer) == 2);
        PLY_TEST_ASSERT (ply_pixel_buffer_get_device_scale (buffer) == 1);
        PLY_TEST_ASSERT (ply_pixel_buffer_get_device_rotation (buffer) ==
                         PLY_PIXEL_BUFFER_ROTATE_UPRIGHT);
        PLY_TEST_ASSERT (!ply_pixel_buffer_is_opaque (buffer));
        PLY_TEST_ASSERT (ply_list_get_length (ply_region_get_rectangle_list (
                                 ply_pixel_buffer_get_updated_areas (buffer))) == 0);

        for (i = 0; i < 6; i++)
                PLY_TEST_ASSERT (pixels[i] == 0);

        ply_pixel_buffer_free (buffer);
        return true;
}

static bool
test_fill_respects_nested_clip_area (void)
{
        ply_rectangle_t clip = { .x = 1, .y = 1, .width = 2, .height = 1 };
        ply_pixel_buffer_t *buffer;
        ply_region_t *updated_areas;
        uint32_t *pixels;

        buffer = ply_pixel_buffer_new (4, 3);
        ply_pixel_buffer_fill_with_hex_color (buffer, NULL, 0x0000ff);
        pixels = ply_pixel_buffer_get_argb32_data (buffer);

        PLY_TEST_ASSERT (ply_pixel_buffer_is_opaque (buffer));
        PLY_TEST_ASSERT (pixels[0] == UINT32_C (0xff0000ff));
        PLY_TEST_ASSERT (pixels[11] == UINT32_C (0xff0000ff));

        updated_areas = ply_pixel_buffer_get_updated_areas (buffer);
        PLY_TEST_ASSERT (ply_list_get_length (
                                 ply_region_get_rectangle_list (updated_areas)) == 1);
        ply_region_clear (updated_areas);

        ply_pixel_buffer_push_clip_area (buffer, &clip);
        ply_pixel_buffer_fill_with_hex_color (buffer, NULL, 0xff0000);
        ply_pixel_buffer_pop_clip_area (buffer);

        PLY_TEST_ASSERT (pixels[0] == UINT32_C (0xff0000ff));
        PLY_TEST_ASSERT (pixels[5] == UINT32_C (0xffff0000));
        PLY_TEST_ASSERT (pixels[6] == UINT32_C (0xffff0000));
        PLY_TEST_ASSERT (pixels[7] == UINT32_C (0xff0000ff));
        PLY_TEST_ASSERT (ply_list_get_length (
                                 ply_region_get_rectangle_list (updated_areas)) == 1);

        ply_pixel_buffer_free (buffer);
        return true;
}

static bool
test_opacity_blends_premultiplied_color (void)
{
        ply_pixel_buffer_t *buffer;
        uint32_t *pixels;

        buffer = ply_pixel_buffer_new (1, 1);
        ply_pixel_buffer_fill_with_hex_color (buffer, NULL, 0x0000ff);
        ply_pixel_buffer_fill_with_hex_color_at_opacity (buffer,
                                                         NULL,
                                                         0xff0000,
                                                         0.5);
        pixels = ply_pixel_buffer_get_argb32_data (buffer);

        PLY_TEST_ASSERT (pixels[0] == UINT32_C (0xff7f0080));

        ply_pixel_buffer_free (buffer);
        return true;
}

static bool
test_argb_data_honors_source_clip (void)
{
        uint32_t source_pixels[] = {
                UINT32_C (0xff010101), UINT32_C (0xff020202), UINT32_C (0xff030303),
                UINT32_C (0xff040404), UINT32_C (0xff050505), UINT32_C (0xff060606),
        };
        ply_rectangle_t fill_area = { .x = 0, .y = 0, .width = 3, .height = 2 };
        ply_rectangle_t clip_area = { .x = 1, .y = 0, .width = 1, .height = 2 };
        ply_pixel_buffer_t *buffer;
        uint32_t *pixels;

        buffer = ply_pixel_buffer_new (3, 2);
        ply_pixel_buffer_fill_with_argb32_data_with_clip (buffer,
                                                          &fill_area,
                                                          &clip_area,
                                                          source_pixels);
        pixels = ply_pixel_buffer_get_argb32_data (buffer);

        PLY_TEST_ASSERT (pixels[0] == 0);
        PLY_TEST_ASSERT (pixels[1] == source_pixels[1]);
        PLY_TEST_ASSERT (pixels[2] == 0);
        PLY_TEST_ASSERT (pixels[3] == 0);
        PLY_TEST_ASSERT (pixels[4] == source_pixels[4]);
        PLY_TEST_ASSERT (pixels[5] == 0);

        ply_pixel_buffer_free (buffer);
        return true;
}

static bool
test_buffer_composition_applies_offset (void)
{
        uint32_t source_pixels[] = {
                UINT32_C (0xff100000), UINT32_C (0xff200000),
                UINT32_C (0xff300000), UINT32_C (0xff400000),
        };
        ply_rectangle_t source_area = { .x = 0, .y = 0, .width = 2, .height = 2 };
        ply_pixel_buffer_t *canvas;
        ply_pixel_buffer_t *source;
        uint32_t *pixels;

        source = ply_pixel_buffer_new (2, 2);
        ply_pixel_buffer_fill_with_argb32_data (source,
                                                &source_area,
                                                source_pixels);
        ply_pixel_buffer_set_opaque (source, true);
        canvas = ply_pixel_buffer_new (4, 3);

        ply_pixel_buffer_fill_with_buffer (canvas, source, 1, 1);
        pixels = ply_pixel_buffer_get_argb32_data (canvas);

        PLY_TEST_ASSERT (pixels[0] == 0);
        PLY_TEST_ASSERT (pixels[5] == source_pixels[0]);
        PLY_TEST_ASSERT (pixels[6] == source_pixels[1]);
        PLY_TEST_ASSERT (pixels[9] == source_pixels[2]);
        PLY_TEST_ASSERT (pixels[10] == source_pixels[3]);

        ply_pixel_buffer_free (canvas);
        ply_pixel_buffer_free (source);
        return true;
}

static bool
test_device_scale_maps_logical_fill_to_pixels (void)
{
        ply_rectangle_t logical_area = { .x = 1, .y = 1, .width = 1, .height = 1 };
        ply_pixel_buffer_t *buffer;
        uint32_t *pixels;

        buffer = ply_pixel_buffer_new (4, 6);
        ply_pixel_buffer_set_device_scale (buffer, 2);

        PLY_TEST_ASSERT (ply_pixel_buffer_get_width (buffer) == 2);
        PLY_TEST_ASSERT (ply_pixel_buffer_get_height (buffer) == 3);

        ply_pixel_buffer_fill_with_hex_color (buffer, &logical_area, 0x00ff00);
        pixels = ply_pixel_buffer_get_argb32_data (buffer);

        PLY_TEST_ASSERT (pixels[2 + 2 * 4] == UINT32_C (0xff00ff00));
        PLY_TEST_ASSERT (pixels[3 + 2 * 4] == UINT32_C (0xff00ff00));
        PLY_TEST_ASSERT (pixels[2 + 3 * 4] == UINT32_C (0xff00ff00));
        PLY_TEST_ASSERT (pixels[3 + 3 * 4] == UINT32_C (0xff00ff00));
        PLY_TEST_ASSERT (pixels[1 + 2 * 4] == 0);

        ply_pixel_buffer_free (buffer);
        return true;
}

static bool
test_tile_and_resize_preserve_sample_points (void)
{
        uint32_t source_pixels[] = {
                UINT32_C (0xff000001), UINT32_C (0xff000002),
                UINT32_C (0xff000003), UINT32_C (0xff000004),
        };
        ply_rectangle_t source_area = { .x = 0, .y = 0, .width = 2, .height = 2 };
        ply_pixel_buffer_t *source;
        ply_pixel_buffer_t *tiled;
        ply_pixel_buffer_t *resized;
        uint32_t *pixels;

        source = ply_pixel_buffer_new (2, 2);
        ply_pixel_buffer_fill_with_argb32_data (source,
                                                &source_area,
                                                source_pixels);

        tiled = ply_pixel_buffer_tile (source, 3, 3);
        pixels = ply_pixel_buffer_get_argb32_data (tiled);
        PLY_TEST_ASSERT (pixels[0] == source_pixels[0]);
        PLY_TEST_ASSERT (pixels[2] == source_pixels[0]);
        PLY_TEST_ASSERT (pixels[6] == source_pixels[0]);
        PLY_TEST_ASSERT (pixels[8] == source_pixels[0]);
        PLY_TEST_ASSERT (pixels[4] == source_pixels[3]);

        resized = ply_pixel_buffer_resize (source, 3, 3);
        pixels = ply_pixel_buffer_get_argb32_data (resized);
        PLY_TEST_ASSERT (pixels[0] == source_pixels[0]);
        PLY_TEST_ASSERT (pixels[2] == source_pixels[1]);
        PLY_TEST_ASSERT (pixels[6] == source_pixels[2]);
        PLY_TEST_ASSERT (pixels[8] == source_pixels[3]);

        ply_pixel_buffer_free (resized);
        ply_pixel_buffer_free (tiled);
        ply_pixel_buffer_free (source);
        return true;
}

static bool
test_rotation_transitions_preserve_axes (void)
{
        ply_pixel_buffer_t *buffer;

        buffer = ply_pixel_buffer_new (3, 2);

        ply_pixel_buffer_set_device_rotation (buffer,
                                              PLY_PIXEL_BUFFER_ROTATE_CLOCKWISE);
        PLY_TEST_ASSERT (ply_pixel_buffer_get_width (buffer) == 2);
        PLY_TEST_ASSERT (ply_pixel_buffer_get_height (buffer) == 3);

        ply_pixel_buffer_set_device_rotation (
                buffer,
                PLY_PIXEL_BUFFER_ROTATE_COUNTER_CLOCKWISE);
        PLY_TEST_ASSERT (ply_pixel_buffer_get_width (buffer) == 2);
        PLY_TEST_ASSERT (ply_pixel_buffer_get_height (buffer) == 3);

        ply_pixel_buffer_set_device_rotation (buffer,
                                              PLY_PIXEL_BUFFER_ROTATE_UPRIGHT);
        PLY_TEST_ASSERT (ply_pixel_buffer_get_width (buffer) == 3);
        PLY_TEST_ASSERT (ply_pixel_buffer_get_height (buffer) == 2);

        ply_pixel_buffer_set_device_rotation (
                buffer,
                PLY_PIXEL_BUFFER_ROTATE_UPSIDE_DOWN);
        PLY_TEST_ASSERT (ply_pixel_buffer_get_width (buffer) == 3);
        PLY_TEST_ASSERT (ply_pixel_buffer_get_height (buffer) == 2);

        ply_pixel_buffer_free (buffer);
        return true;
}

static bool
test_rotate_upright_maps_clockwise_pixels (void)
{
        ply_pixel_buffer_t *rotated;
        ply_pixel_buffer_t *upright;
        uint32_t *pixels;
        size_t i;

        rotated = ply_pixel_buffer_new (3, 2);
        pixels = ply_pixel_buffer_get_argb32_data (rotated);
        for (i = 0; i < 6; i++)
                pixels[i] = i + 1;

        ply_pixel_buffer_set_device_rotation (rotated,
                                              PLY_PIXEL_BUFFER_ROTATE_CLOCKWISE);
        upright = ply_pixel_buffer_rotate_upright (rotated);
        pixels = ply_pixel_buffer_get_argb32_data (upright);

        PLY_TEST_ASSERT (ply_pixel_buffer_get_width (upright) == 2);
        PLY_TEST_ASSERT (ply_pixel_buffer_get_height (upright) == 3);
        PLY_TEST_ASSERT (pixels[0] == 3);
        PLY_TEST_ASSERT (pixels[1] == 6);
        PLY_TEST_ASSERT (pixels[2] == 2);
        PLY_TEST_ASSERT (pixels[3] == 5);
        PLY_TEST_ASSERT (pixels[4] == 1);
        PLY_TEST_ASSERT (pixels[5] == 4);

        ply_pixel_buffer_free (upright);
        ply_pixel_buffer_free (rotated);
        return true;
}

static const ply_test_case_t test_cases[] =
{
        PLY_TEST_CASE (test_new_buffer_reports_empty_geometry),
        PLY_TEST_CASE (test_fill_respects_nested_clip_area),
        PLY_TEST_CASE (test_opacity_blends_premultiplied_color),
        PLY_TEST_CASE (test_argb_data_honors_source_clip),
        PLY_TEST_CASE (test_buffer_composition_applies_offset),
        PLY_TEST_CASE (test_device_scale_maps_logical_fill_to_pixels),
        PLY_TEST_CASE (test_tile_and_resize_preserve_sample_points),
        PLY_TEST_CASE (test_rotation_transitions_preserve_axes),
        PLY_TEST_CASE (test_rotate_upright_maps_clockwise_pixels),
};

PLY_TEST_MAIN (test_cases)
