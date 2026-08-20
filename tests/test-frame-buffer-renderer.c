/*
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#include "ply-test.h"

#include <dirent.h>

#include "ply-renderer-plugin.h"
#include "ply-utils.h"

typedef ply_renderer_plugin_interface_t *
(*get_backend_interface_function_t) (void);

static int
count_open_file_descriptors (void)
{
        struct dirent *entry;
        DIR *directory;
        int count = 0;

        directory = opendir ("/proc/self/fd");
        if (directory == NULL)
                return -1;

        while ((entry = readdir (directory)) != NULL) {
                if (entry->d_name[0] == '.')
                        continue;

                count++;
        }

        closedir (directory);
        return count;
}

static bool
test_destroy_backend_closes_device (void)
{
        get_backend_interface_function_t get_backend_interface;
        ply_renderer_plugin_interface_t *plugin_interface;
        ply_renderer_backend_t *backend;
        ply_module_handle_t *module;
        int open_descriptor_count;

        module = ply_open_module (TEST_FRAME_BUFFER_RENDERER_PLUGIN_PATH);
        PLY_TEST_ASSERT (module != NULL);

        get_backend_interface = (get_backend_interface_function_t)
                                ply_module_look_up_function (module,
                                                             "ply_renderer_backend_get_interface");
        PLY_TEST_ASSERT (get_backend_interface != NULL);

        plugin_interface = get_backend_interface ();
        PLY_TEST_ASSERT (plugin_interface != NULL);

        backend = plugin_interface->create_backend ("/dev/null", NULL, NULL);
        PLY_TEST_ASSERT (backend != NULL);

        open_descriptor_count = count_open_file_descriptors ();
        PLY_TEST_ASSERT (open_descriptor_count >= 0);

        PLY_TEST_ASSERT (plugin_interface->open_device (backend));
        PLY_TEST_ASSERT (count_open_file_descriptors () == open_descriptor_count + 1);

        plugin_interface->destroy_backend (backend);
        PLY_TEST_ASSERT (count_open_file_descriptors () == open_descriptor_count);

        ply_close_module (module);
        return true;
}

static const ply_test_case_t test_cases[] =
{
        PLY_TEST_CASE (test_destroy_backend_closes_device),
};

PLY_TEST_MAIN (test_cases)
