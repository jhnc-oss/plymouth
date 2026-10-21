/*
 * Copyright (C) 2026 Ilya Kitaev
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#include "ply-test.h"

#include <stdlib.h>
#include <string.h>

#include "ply-keyboard.h"
#include "ply-renderer-private.h"
#include "script-execute.h"
#include "script-lib-plymouth.h"
#include "script-object.h"
#include "script-parse.h"
#include "script.h"

static bool
test_get_keyboard_layout_reports_renderer_layout (void)
{
        script_lib_plymouth_data_t *plymouth_data;
        script_return_t result;
        ply_keyboard_t *keyboard;
        ply_renderer_t *renderer;
        script_state_t *state;
        script_op_t *op;
        int terminal_marker;
        int local_console_marker;
        char *layout;

        renderer = ply_renderer_new_with_plugin_directory (
                PLY_RENDERER_TYPE_FRAME_BUFFER,
                TEST_RENDERER_PLUGIN_DIR,
                "/dev/requested",
                (ply_terminal_t *) &terminal_marker,
                (ply_terminal_t *) &local_console_marker);
        PLY_TEST_ASSERT (renderer != NULL);
        PLY_TEST_ASSERT (ply_renderer_open (renderer, true));
        keyboard = ply_keyboard_new_for_renderer (renderer);
        PLY_TEST_ASSERT (keyboard != NULL);

        state = script_state_new (NULL);
        plymouth_data = script_lib_plymouth_setup (state,
                                                   PLY_BOOT_SPLASH_MODE_BOOT_UP,
                                                   50,
                                                   keyboard);
        op = script_parse_string ("layout = Plymouth.GetKeyboardLayout ();",
                                  "test.script");
        PLY_TEST_ASSERT (op != NULL);

        result = script_execute (state, op);
        script_obj_unref (result.object);

        layout = script_obj_hash_get_string (state->global, "layout");
        PLY_TEST_ASSERT (layout != NULL);
        PLY_TEST_ASSERT (strcmp (layout, "Russian") == 0);

        free (layout);
        script_parse_op_free (op);
        script_lib_plymouth_destroy (plymouth_data);
        script_state_destroy (state);
        ply_keyboard_free (keyboard);
        ply_renderer_close (renderer);
        ply_renderer_free (renderer);
        return true;
}

static const ply_test_case_t test_cases[] =
{
        PLY_TEST_CASE (test_get_keyboard_layout_reports_renderer_layout),
};

PLY_TEST_MAIN (test_cases)
