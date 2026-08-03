/*
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "ply-test.h"

#include <stdint.h>

#include "ply-boot-splash-plugin.h"
#include "ply-event-loop.h"
#include "ply-key-file.h"
#include "ply-list.h"
#include "ply-pixel-buffer.h"
#include "ply-pixel-display.h"
#include "ply-renderer-private.h"
#include "ply-utils.h"

typedef ply_boot_splash_plugin_interface_t *
(*get_plugin_interface_function_t) (void);

static bool
test_update_mode_starts_with_empty_progress (void)
{
        const ply_boot_splash_plugin_interface_t *interface;
        get_plugin_interface_function_t get_interface;
        ply_boot_splash_plugin_t *plugin;
        ply_renderer_head_t *head;
        ply_pixel_display_t *display;
        ply_pixel_buffer_t *buffer;
        ply_module_handle_t *module;
        ply_event_loop_t *loop;
        ply_key_file_t *key_file;
        ply_renderer_t *renderer;
        ply_list_node_t *node;
        uint32_t *pixels;

        module = ply_open_module (TEST_TWO_STEP_PLUGIN_PATH);
        PLY_TEST_ASSERT (module != NULL);
        get_interface = (get_plugin_interface_function_t)
                        ply_module_look_up_function (module,
                                                     "ply_boot_splash_plugin_get_interface");
        PLY_TEST_ASSERT (get_interface != NULL);
        interface = get_interface ();
        PLY_TEST_ASSERT (interface != NULL);

        key_file = ply_key_file_new (TEST_TWO_STEP_THEME_PATH);
        PLY_TEST_ASSERT (key_file != NULL);
        PLY_TEST_ASSERT (ply_key_file_load (key_file));
        plugin = interface->create_plugin (key_file);
        PLY_TEST_ASSERT (plugin != NULL);

        loop = ply_event_loop_new ();
        PLY_TEST_ASSERT (loop != NULL);
        renderer = ply_renderer_new_with_plugin_directory (
                PLY_RENDERER_TYPE_FRAME_BUFFER,
                TEST_RENDERER_PLUGIN_DIR,
                NULL,
                NULL,
                NULL);
        PLY_TEST_ASSERT (renderer != NULL);
        PLY_TEST_ASSERT (ply_renderer_open (renderer, false));
        node = ply_list_get_first_node (ply_renderer_get_heads (renderer));
        PLY_TEST_ASSERT (node != NULL);
        head = ply_list_node_get_data (node);
        display = ply_pixel_display_new (renderer, head);
        PLY_TEST_ASSERT (display != NULL);
        interface->add_pixel_display (plugin, display);

        PLY_TEST_ASSERT (interface->show_splash_screen (
                                 plugin,
                                 loop,
                                 NULL,
                                 PLY_BOOT_SPLASH_MODE_BOOT_UP));
        interface->on_boot_progress (plugin, 1.0, 0.75);
        interface->hide_splash_screen (plugin, loop);

        PLY_TEST_ASSERT (interface->show_splash_screen (
                                 plugin,
                                 loop,
                                 NULL,
                                 PLY_BOOT_SPLASH_MODE_UPDATES));
        buffer = ply_renderer_get_buffer_for_head (renderer, head);
        pixels = ply_pixel_buffer_get_argb32_data (buffer);
        PLY_TEST_ASSERT (pixels[0] == UINT32_C (0xffff0000));
        PLY_TEST_ASSERT (pixels[79] == UINT32_C (0xffff0000));

        interface->system_update (plugin, 50);
        PLY_TEST_ASSERT (pixels[0] == UINT32_C (0xff00ff00));
        PLY_TEST_ASSERT (pixels[39] == UINT32_C (0xff00ff00));
        PLY_TEST_ASSERT (pixels[40] == UINT32_C (0xffff0000));
        PLY_TEST_ASSERT (pixels[79] == UINT32_C (0xffff0000));

        interface->hide_splash_screen (plugin, loop);
        interface->remove_pixel_display (plugin, display);
        interface->destroy_plugin (plugin);
        ply_pixel_display_free (display);
        ply_renderer_close (renderer);
        ply_renderer_free (renderer);
        ply_event_loop_free (loop);
        ply_key_file_free (key_file);
        ply_close_module (module);
        return true;
}

static const ply_test_case_t test_cases[] =
{
        PLY_TEST_CASE (test_update_mode_starts_with_empty_progress),
};

PLY_TEST_MAIN (test_cases)
