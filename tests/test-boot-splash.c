/*
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#include "ply-test.h"

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "plugins/fake-splash.h"
#include "ply-boot-splash.h"
#include "ply-keyboard.h"
#include "ply-list.h"
#include "ply-pixel-display.h"
#include "ply-renderer-private.h"
#include "ply-terminal.h"
#include "ply-text-display.h"
#include "ply-utils.h"

static const test_splash_plugin_state_t *
get_plugin_state (ply_module_handle_t *module)
{
        test_splash_plugin_get_state_function_t get_state;

        get_state = (test_splash_plugin_get_state_function_t)
                    ply_module_look_up_function (module,
                                                 "test_splash_plugin_get_state");
        if (get_state == NULL)
                return NULL;

        return get_state ();
}

static ply_boot_splash_t *
load_splash (ply_buffer_t *boot_buffer)
{
        ply_boot_splash_t *splash;

        splash = ply_boot_splash_new (TEST_SPLASH_THEME_PATH,
                                      TEST_SPLASH_PLUGIN_DIR,
                                      boot_buffer);
        if (!ply_boot_splash_load (splash)) {
                ply_boot_splash_free (splash);
                return NULL;
        }

        return splash;
}

static bool
test_theme_loads_and_attached_devices_are_removed (void)
{
        const test_splash_plugin_state_t *state;
        ply_renderer_head_t *head;
        ply_pixel_display_t *pixel_display;
        ply_text_display_t *text_display;
        ply_module_handle_t *module;
        ply_boot_splash_t *splash;
        ply_renderer_t *renderer;
        ply_list_node_t *node;
        ply_terminal_t *terminal;
        ply_keyboard_t *keyboard;
        ply_buffer_t *boot_buffer;
        char *terminal_name;
        int terminal_fd;

        module = ply_open_module (TEST_SPLASH_PLUGIN_PATH);
        PLY_TEST_ASSERT (module != NULL);

        boot_buffer = ply_buffer_new ();
        ply_buffer_append_bytes (boot_buffer, "boot output", 11);
        splash = load_splash (boot_buffer);
        PLY_TEST_ASSERT (splash != NULL);
        PLY_TEST_ASSERT (ply_boot_splash_uses_pixel_displays (splash));

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
        pixel_display = ply_pixel_display_new (renderer, head);
        PLY_TEST_ASSERT (pixel_display != NULL);

        terminal_fd = posix_openpt (O_RDWR | O_NOCTTY | O_CLOEXEC);
        PLY_TEST_ASSERT (terminal_fd >= 0);
        PLY_TEST_ASSERT (grantpt (terminal_fd) == 0);
        PLY_TEST_ASSERT (unlockpt (terminal_fd) == 0);
        terminal_name = ptsname (terminal_fd);
        PLY_TEST_ASSERT (terminal_name != NULL);
        terminal = ply_terminal_new (terminal_name, NULL);
        PLY_TEST_ASSERT (terminal != NULL);
        PLY_TEST_ASSERT (ply_terminal_open (terminal));
        text_display = ply_text_display_new (terminal);
        keyboard = ply_keyboard_new_for_terminal (terminal);
        PLY_TEST_ASSERT (text_display != NULL);
        PLY_TEST_ASSERT (keyboard != NULL);

        ply_boot_splash_set_keyboard (splash, keyboard);
        ply_boot_splash_add_pixel_display (splash, pixel_display);
        ply_boot_splash_add_text_display (splash, text_display);

        state = get_plugin_state (module);
        PLY_TEST_ASSERT (state != NULL);
        PLY_TEST_ASSERT (state->create_count == 1);
        PLY_TEST_ASSERT (strcmp (state->theme_token,
                                 "theme settings loaded") == 0);
        PLY_TEST_ASSERT (state->set_keyboard_count == 1);
        PLY_TEST_ASSERT (state->keyboard == keyboard);
        PLY_TEST_ASSERT (state->add_pixel_display_count == 1);
        PLY_TEST_ASSERT (state->pixel_display == pixel_display);
        PLY_TEST_ASSERT (state->add_text_display_count == 1);
        PLY_TEST_ASSERT (state->text_display == text_display);

        ply_boot_splash_free (splash);
        PLY_TEST_ASSERT (state->unset_keyboard_count == 1);
        PLY_TEST_ASSERT (state->remove_pixel_display_count == 1);
        PLY_TEST_ASSERT (state->remove_text_display_count == 1);
        PLY_TEST_ASSERT (state->destroy_count == 1);

        ply_keyboard_free (keyboard);
        ply_text_display_free (text_display);
        ply_terminal_free (terminal);
        close (terminal_fd);
        ply_pixel_display_free (pixel_display);
        ply_renderer_close (renderer);
        ply_renderer_free (renderer);
        ply_buffer_free (boot_buffer);
        ply_close_module (module);
        return true;
}

static const ply_test_case_t test_cases[] =
{
        PLY_TEST_CASE (test_theme_loads_and_attached_devices_are_removed),
};

PLY_TEST_MAIN (test_cases)
