/*
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#include "ply-test.h"

#include <string.h>

#include "ply-renderer-plugin.h"
#include "ply-renderer-drm-tile.h"
#include "ply-utils.h"

typedef ply_renderer_plugin_interface_t *
(*get_backend_interface_function_t) (void);

static ply_renderer_drm_tile_output_t
make_tile_output (uint32_t group_id,
                  uint32_t num_h,
                  uint32_t num_v,
                  uint32_t h_loc,
                  uint32_t v_loc,
                  uint32_t h_size,
                  uint32_t v_size)
{
        ply_renderer_drm_tile_output_t output = { 0 };

        output.tile.group_id = group_id;
        output.tile.is_single_monitor = true;
        output.tile.num_h = num_h;
        output.tile.num_v = num_v;
        output.tile.h_loc = h_loc;
        output.tile.v_loc = v_loc;
        output.tile.h_size = h_size;
        output.tile.v_size = v_size;
        output.mode.clock = 1058800;
        output.mode.hdisplay = h_size;
        output.mode.vdisplay = v_size;
        output.mode.htotal = h_size + 200;
        output.mode.vtotal = v_size + 48;
        output.controller_id = h_loc + v_loc * num_h + 1;
        output.device_scale = 2;
        output.tiled = true;
        output.connected = true;
        output.upright_rotation = true;
        return output;
}

static bool
test_tile_info_parser_accepts_valid_layout (void)
{
        static const char tile_data[] = "1:1:2:1:1:0:3840:4320";
        ply_renderer_drm_tile_info_t tile_info;

        PLY_TEST_ASSERT (ply_renderer_drm_tile_info_parse (tile_data,
                                                           sizeof(tile_data),
                                                           &tile_info));
        PLY_TEST_ASSERT (tile_info.group_id == 1);
        PLY_TEST_ASSERT (tile_info.is_single_monitor);
        PLY_TEST_ASSERT (tile_info.num_h == 2);
        PLY_TEST_ASSERT (tile_info.num_v == 1);
        PLY_TEST_ASSERT (tile_info.h_loc == 1);
        PLY_TEST_ASSERT (tile_info.v_loc == 0);
        PLY_TEST_ASSERT (tile_info.h_size == 3840);
        PLY_TEST_ASSERT (tile_info.v_size == 4320);
        return true;
}

static bool
test_tile_info_parser_rejects_invalid_layouts (void)
{
        static const char embedded_nul[] = "1:1:2:1:0:0:3840:4320\0extra";
        ply_renderer_drm_tile_info_t tile_info;

        PLY_TEST_ASSERT (!ply_renderer_drm_tile_info_parse ("1:1:2:1:2:0:3840:4320",
                                                            sizeof("1:1:2:1:2:0:3840:4320"),
                                                            &tile_info));
        PLY_TEST_ASSERT (!ply_renderer_drm_tile_info_parse ("1:1:2:1:0:0:0:4320",
                                                            sizeof("1:1:2:1:0:0:0:4320"),
                                                            &tile_info));
        PLY_TEST_ASSERT (!ply_renderer_drm_tile_info_parse ("1:1:2:1:0:0:3840:4320:extra",
                                                            sizeof("1:1:2:1:0:0:3840:4320:extra"),
                                                            &tile_info));
        PLY_TEST_ASSERT (!ply_renderer_drm_tile_info_parse (embedded_nul,
                                                            sizeof(embedded_nul),
                                                            &tile_info));
        return true;
}

static bool
test_tile_mode_selection_ignores_full_monitor_mode (void)
{
        drmModeModeInfo modes[] = {
                { .hdisplay = 7680, .vdisplay = 4320, .type = DRM_MODE_TYPE_PREFERRED },
                { .hdisplay = 3840, .vdisplay = 2160, .type = DRM_MODE_TYPE_PREFERRED },
                { .hdisplay = 3840, .vdisplay = 4320 },
        };
        ply_renderer_drm_tile_info_t tile_info = {
                .h_size = 3840,
                .v_size = 4320,
        };

        PLY_TEST_ASSERT (ply_renderer_drm_find_tile_mode (modes, 3, &tile_info) == &modes[2]);

        tile_info.h_size = 5120;
        PLY_TEST_ASSERT (ply_renderer_drm_find_tile_mode (modes, 3, &tile_info) == NULL);
        return true;
}

static bool
test_reported_tiled_layout_geometry (void)
{
        ply_renderer_drm_tile_output_t outputs[] = {
                make_tile_output (1, 2, 1, 1, 0, 3840, 4320),
                make_tile_output (1, 2, 1, 0, 0, 3840, 4320),
        };
        ply_renderer_drm_tile_geometry_t geometry;

        /* The reporter's right tile is enumerated before the left tile. */
        PLY_TEST_ASSERT (ply_renderer_drm_tile_group_get_geometry (outputs, 2, 0,
                                                                   16384, 16384,
                                                                   &geometry));
        PLY_TEST_ASSERT (geometry.width == 7680);
        PLY_TEST_ASSERT (geometry.height == 4320);
        PLY_TEST_ASSERT (geometry.x == 3840);
        PLY_TEST_ASSERT (geometry.y == 0);

        PLY_TEST_ASSERT (ply_renderer_drm_tile_group_get_geometry (outputs, 2, 1,
                                                                   16384, 16384,
                                                                   &geometry));
        PLY_TEST_ASSERT (geometry.x == 0);
        PLY_TEST_ASSERT (geometry.y == 0);
        return true;
}

static bool
test_vertical_tiled_layout_geometry (void)
{
        ply_renderer_drm_tile_output_t outputs[] = {
                make_tile_output (7, 1, 2, 0, 0, 1920, 2160),
                make_tile_output (7, 1, 2, 0, 1, 1920, 2160),
        };
        ply_renderer_drm_tile_geometry_t geometry;

        PLY_TEST_ASSERT (ply_renderer_drm_tile_group_get_geometry (outputs, 2, 1,
                                                                   8192, 8192,
                                                                   &geometry));
        PLY_TEST_ASSERT (geometry.width == 1920);
        PLY_TEST_ASSERT (geometry.height == 4320);
        PLY_TEST_ASSERT (geometry.x == 0);
        PLY_TEST_ASSERT (geometry.y == 2160);
        return true;
}

static bool
test_tile_group_arrival_and_removal (void)
{
        ply_renderer_drm_tile_output_t outputs[] = {
                make_tile_output (1, 2, 1, 0, 0, 3840, 4320),
                make_tile_output (1, 2, 1, 1, 0, 3840, 4320),
        };
        ply_renderer_drm_tile_geometry_t geometry;

        outputs[1].connected = false;
        outputs[1].controller_id = 0;
        PLY_TEST_ASSERT (!ply_renderer_drm_tile_group_get_geometry (outputs, 2, 0,
                                                                    16384, 16384,
                                                                    &geometry));

        outputs[1].connected = true;
        outputs[1].controller_id = 2;
        PLY_TEST_ASSERT (ply_renderer_drm_tile_group_get_geometry (outputs, 2, 0,
                                                                   16384, 16384,
                                                                   &geometry));

        outputs[0].connected = false;
        outputs[0].controller_id = 0;
        PLY_TEST_ASSERT (!ply_renderer_drm_tile_group_get_geometry (outputs, 2, 1,
                                                                    16384, 16384,
                                                                    &geometry));
        return true;
}

static bool
test_tile_group_rejects_duplicate_and_incompatible_tiles (void)
{
        ply_renderer_drm_tile_output_t outputs[] = {
                make_tile_output (1, 2, 1, 0, 0, 3840, 4320),
                make_tile_output (1, 2, 1, 0, 0, 3840, 4320),
        };
        ply_renderer_drm_tile_geometry_t geometry;

        PLY_TEST_ASSERT (!ply_renderer_drm_tile_group_get_geometry (outputs, 2, 0,
                                                                    16384, 16384,
                                                                    &geometry));

        outputs[1].tile.h_loc = 1;
        outputs[1].device_scale = 1;
        PLY_TEST_ASSERT (!ply_renderer_drm_tile_group_get_geometry (outputs, 2, 0,
                                                                    16384, 16384,
                                                                    &geometry));

        outputs[1].device_scale = 2;
        outputs[1].mode.clock++;
        PLY_TEST_ASSERT (!ply_renderer_drm_tile_group_get_geometry (outputs, 2, 0,
                                                                    16384, 16384,
                                                                    &geometry));

        outputs[1].mode.clock--;
        outputs[1].uses_hw_rotation = true;
        PLY_TEST_ASSERT (!ply_renderer_drm_tile_group_get_geometry (outputs, 2, 0,
                                                                    16384, 16384,
                                                                    &geometry));

        outputs[1].uses_hw_rotation = false;
        outputs[1].controller_id = outputs[0].controller_id;
        PLY_TEST_ASSERT (!ply_renderer_drm_tile_group_get_geometry (outputs, 2, 0,
                                                                    16384, 16384,
                                                                    &geometry));
        return true;
}

static bool
test_tile_group_respects_framebuffer_limits (void)
{
        ply_renderer_drm_tile_output_t outputs[] = {
                make_tile_output (1, 2, 1, 0, 0, 3840, 4320),
                make_tile_output (1, 2, 1, 1, 0, 3840, 4320),
        };
        ply_renderer_drm_tile_geometry_t geometry;

        PLY_TEST_ASSERT (!ply_renderer_drm_tile_group_get_geometry (outputs, 2, 0,
                                                                    4096, 8192,
                                                                    &geometry));
        return true;
}

static bool
test_close_device_preserves_backend (void)
{
        get_backend_interface_function_t get_backend_interface;
        ply_renderer_plugin_interface_t *plugin_interface;
        ply_renderer_backend_t *backend;
        ply_module_handle_t *module;

        module = ply_open_module (TEST_DRM_RENDERER_PLUGIN_PATH);
        PLY_TEST_ASSERT (module != NULL);

        get_backend_interface = (get_backend_interface_function_t)
                                ply_module_look_up_function (module,
                                                             "ply_renderer_backend_get_interface");
        PLY_TEST_ASSERT (get_backend_interface != NULL);

        plugin_interface = get_backend_interface ();
        PLY_TEST_ASSERT (plugin_interface != NULL);

        backend = plugin_interface->create_backend ("/dev/null", NULL, NULL);
        PLY_TEST_ASSERT (backend != NULL);
        PLY_TEST_ASSERT (plugin_interface->open_device (backend));
        PLY_TEST_ASSERT (!plugin_interface->query_device (backend, false));

        plugin_interface->close_device (backend);
        PLY_TEST_ASSERT (strcmp (plugin_interface->get_device_name (backend),
                                 "/dev/null") == 0);
        plugin_interface->destroy_backend (backend);

        ply_close_module (module);
        return true;
}

static const ply_test_case_t test_cases[] =
{
        PLY_TEST_CASE (test_tile_info_parser_accepts_valid_layout),
        PLY_TEST_CASE (test_tile_info_parser_rejects_invalid_layouts),
        PLY_TEST_CASE (test_tile_mode_selection_ignores_full_monitor_mode),
        PLY_TEST_CASE (test_reported_tiled_layout_geometry),
        PLY_TEST_CASE (test_vertical_tiled_layout_geometry),
        PLY_TEST_CASE (test_tile_group_arrival_and_removal),
        PLY_TEST_CASE (test_tile_group_rejects_duplicate_and_incompatible_tiles),
        PLY_TEST_CASE (test_tile_group_respects_framebuffer_limits),
        PLY_TEST_CASE (test_close_device_preserves_backend),
};

PLY_TEST_MAIN (test_cases)
