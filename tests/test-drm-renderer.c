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
        PLY_TEST_CASE (test_close_device_preserves_backend),
};

PLY_TEST_MAIN (test_cases)
