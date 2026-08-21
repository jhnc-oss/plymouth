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
#include "ply-utils.h"

typedef ply_renderer_plugin_interface_t *
(*get_backend_interface_function_t) (void);

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
        PLY_TEST_CASE (test_close_device_preserves_backend),
};

PLY_TEST_MAIN (test_cases)
