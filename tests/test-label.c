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

static bool
test_label_forwards_operations_after_plugin_loads (void)
{
        const test_label_plugin_state_t *state;
        ply_rich_text_character_style_t style;
        ply_rich_text_span_t span = { .offset = 1, .range = 2 };
        ply_pixel_display_t *display;
        ply_pixel_buffer_t *buffer;
        ply_module_handle_t *module;
        ply_rich_text_t *rich_text;
        ply_label_t *label;
        int display_marker;
        int buffer_marker;

        module = ply_open_module (TEST_LABEL_PLUGIN_PATH);
        PLY_TEST_ASSERT (module != NULL);

        display = (ply_pixel_display_t *) &display_marker;
        buffer = (ply_pixel_buffer_t *) &buffer_marker;
        label = ply_label_new_with_plugin_directory (TEST_LABEL_PLUGIN_DIR);
        PLY_TEST_ASSERT (label != NULL);
        PLY_TEST_ASSERT (ply_label_show (label, display, 12, 34));
        PLY_TEST_ASSERT (!ply_label_is_hidden (label));

        ply_label_set_text (label, "ready");
        ply_label_set_font (label, "Monospace 10");
        ply_label_set_alignment (label, PLY_LABEL_ALIGN_CENTER);
        ply_label_set_width (label, 240);
        ply_label_set_color (label, 0.1f, 0.2f, 0.3f, 0.4f);

        ply_rich_text_character_style_initialize (&style);
        rich_text = ply_rich_text_new ();
        ply_rich_text_set_character (rich_text, style, 0, "a", 1);
        ply_rich_text_set_character (rich_text, style, 1, "b", 1);
        ply_rich_text_set_character (rich_text, style, 2, "c", 1);
        ply_label_set_rich_text (label, rich_text, &span);

        ply_label_draw_area (label, buffer, 5, 6, 70, 80);
        ply_label_hide (label);
        PLY_TEST_ASSERT (ply_label_is_hidden (label));

        state = get_plugin_state (module);
        PLY_TEST_ASSERT (state != NULL);
        PLY_TEST_ASSERT (state->create_count == 1);
        PLY_TEST_ASSERT (state->show_count == 1);
        PLY_TEST_ASSERT (state->display == display);
        PLY_TEST_ASSERT (state->show_x == 12);
        PLY_TEST_ASSERT (state->show_y == 34);
        PLY_TEST_ASSERT (state->text_count == 1);
        PLY_TEST_ASSERT (strcmp (state->text, "ready") == 0);
        PLY_TEST_ASSERT (state->font_count == 1);
        PLY_TEST_ASSERT (strcmp (state->font, "Monospace 10") == 0);
        PLY_TEST_ASSERT (state->alignment_count == 2);
        PLY_TEST_ASSERT (state->alignment == PLY_LABEL_ALIGN_CENTER);
        PLY_TEST_ASSERT (state->width_setting_count == 2);
        PLY_TEST_ASSERT (state->width == 240);
        PLY_TEST_ASSERT (state->color_count == 2);
        PLY_TEST_ASSERT (nearly_equal (state->red, 0.1f));
        PLY_TEST_ASSERT (nearly_equal (state->green, 0.2f));
        PLY_TEST_ASSERT (nearly_equal (state->blue, 0.3f));
        PLY_TEST_ASSERT (nearly_equal (state->alpha, 0.4f));
        PLY_TEST_ASSERT (state->rich_text_count == 1);
        PLY_TEST_ASSERT (state->rich_text == rich_text);
        PLY_TEST_ASSERT (state->span.offset == span.offset);
        PLY_TEST_ASSERT (state->span.range == span.range);
        PLY_TEST_ASSERT (state->draw_count == 1);
        PLY_TEST_ASSERT (state->draw_buffer == buffer);
        PLY_TEST_ASSERT (state->draw_x == 5);
        PLY_TEST_ASSERT (state->draw_y == 6);
        PLY_TEST_ASSERT (state->draw_width == 70);
        PLY_TEST_ASSERT (state->draw_height == 80);
        PLY_TEST_ASSERT (state->hide_count == 1);
        PLY_TEST_ASSERT (state->hidden_count == 2);

        ply_label_free (label);
        PLY_TEST_ASSERT (state->destroy_count == 1);
        ply_rich_text_free (rich_text);
        ply_close_module (module);
        return true;
}

static const ply_test_case_t test_cases[] =
{
        PLY_TEST_CASE (test_label_replays_settings_when_plugin_loads),
        PLY_TEST_CASE (test_label_forwards_operations_after_plugin_loads),
};

PLY_TEST_MAIN (test_cases)
