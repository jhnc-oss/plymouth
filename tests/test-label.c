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

#include "plugins/fake-label.h"
#include "ply-label-private.h"
#include "ply-utils.h"

static const test_label_plugin_state_t *
get_plugin_state (ply_module_handle_t *module)
{
        test_label_plugin_get_state_function_t get_state;

        get_state = (test_label_plugin_get_state_function_t)
                    ply_module_look_up_function (module,
                                                 "test_label_plugin_get_state");
        if (get_state == NULL)
                return NULL;

        return get_state ();
}

static bool
nearly_equal (float left,
              float right)
{
        return left > right - 0.001f && left < right + 0.001f;
}

static bool
test_label_replays_settings_when_plugin_loads (void)
{
        const test_label_plugin_state_t *state;
        ply_module_handle_t *module;
        ply_label_t *label;

        module = ply_open_module (TEST_LABEL_PLUGIN_PATH);
        PLY_TEST_ASSERT (module != NULL);

        label = ply_label_new_with_plugin_directory (TEST_LABEL_PLUGIN_DIR);
        PLY_TEST_ASSERT (label != NULL);
        PLY_TEST_ASSERT (ply_label_is_hidden (label));

        ply_label_set_text (label, "waiting for disk");
        ply_label_set_font (label, "Sans 12");
        ply_label_set_alignment (label, PLY_LABEL_ALIGN_RIGHT);
        ply_label_set_width (label, 320);
        ply_label_set_hex_color (label, 0x336699cc);

        PLY_TEST_ASSERT (ply_label_get_width (label) == 123);
        PLY_TEST_ASSERT (ply_label_get_height (label) == 45);

        state = get_plugin_state (module);
        PLY_TEST_ASSERT (state != NULL);
        PLY_TEST_ASSERT (state->create_count == 1);
        PLY_TEST_ASSERT (state->font_count == 1);
        PLY_TEST_ASSERT (strcmp (state->font, "Sans 12") == 0);
        PLY_TEST_ASSERT (state->text_count == 1);
        PLY_TEST_ASSERT (strcmp (state->text, "waiting for disk") == 0);
        PLY_TEST_ASSERT (state->alignment_count == 1);
        PLY_TEST_ASSERT (state->alignment == PLY_LABEL_ALIGN_RIGHT);
        PLY_TEST_ASSERT (state->width_setting_count == 1);
        PLY_TEST_ASSERT (state->width == 320);
        PLY_TEST_ASSERT (state->color_count == 1);
        PLY_TEST_ASSERT (nearly_equal (state->red, 0.2f));
        PLY_TEST_ASSERT (nearly_equal (state->green, 0.4f));
        PLY_TEST_ASSERT (nearly_equal (state->blue, 0.6f));
        PLY_TEST_ASSERT (nearly_equal (state->alpha, 0.8f));
        PLY_TEST_ASSERT (state->width_count == 1);
        PLY_TEST_ASSERT (state->height_count == 1);

        ply_label_free (label);
        ply_close_module (module);
        return true;
}

static const ply_test_case_t test_cases[] =
{
        PLY_TEST_CASE (test_label_replays_settings_when_plugin_loads),
};

PLY_TEST_MAIN (test_cases)
