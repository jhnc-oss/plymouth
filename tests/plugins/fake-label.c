/*
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#include "fake-label.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ply-label-plugin.h"

struct _ply_label_plugin_control
{
        bool hidden;
};

static test_label_plugin_state_t state;

const test_label_plugin_state_t *
test_label_plugin_get_state (void)
{
        return &state;
}

static ply_label_plugin_control_t *
create_control (void)
{
        ply_label_plugin_control_t *control;

        memset (&state, 0, sizeof(state));
        state.create_count++;
        state.hidden = true;

        control = calloc (1, sizeof(ply_label_plugin_control_t));
        control->hidden = true;
        return control;
}

static void
destroy_control (ply_label_plugin_control_t *control)
{
        state.destroy_count++;
        free (control);
}

static bool
show_control (ply_label_plugin_control_t *control,
              ply_pixel_display_t        *display,
              long                        x,
              long                        y)
{
        state.show_count++;
        state.display = display;
        state.show_x = x;
        state.show_y = y;
        state.hidden = false;
        control->hidden = false;
        return true;
}

static void
hide_control (ply_label_plugin_control_t *control)
{
        state.hide_count++;
        state.hidden = true;
        control->hidden = true;
}

static void
draw_control (ply_label_plugin_control_t *control,
              ply_pixel_buffer_t         *pixel_buffer,
              long                        x,
              long                        y,
              unsigned long               width,
              unsigned long               height)
{
        if (control != NULL)
                state.draw_count++;
        state.draw_buffer = pixel_buffer;
        state.draw_x = x;
        state.draw_y = y;
        state.draw_width = width;
        state.draw_height = height;
}

static bool
is_control_hidden (ply_label_plugin_control_t *control)
{
        state.hidden_count++;
        return control->hidden;
}

static void
set_text_for_control (ply_label_plugin_control_t *control,
                      const char                 *text)
{
        if (control != NULL)
                state.text_count++;
        snprintf (state.text, sizeof(state.text), "%s", text);
}

static void
set_rich_text_for_control (ply_label_plugin_control_t *control,
                           ply_rich_text_t            *rich_text,
                           ply_rich_text_span_t       *span)
{
        if (control != NULL)
                state.rich_text_count++;
        state.rich_text = rich_text;
        state.span = *span;
}

static void
set_font_for_control (ply_label_plugin_control_t *control,
                      const char                 *font)
{
        if (control != NULL)
                state.font_count++;
        snprintf (state.font, sizeof(state.font), "%s", font == NULL ? "" : font);
}

static void
set_color_for_control (ply_label_plugin_control_t *control,
                       float                       red,
                       float                       green,
                       float                       blue,
                       float                       alpha)
{
        if (control != NULL)
                state.color_count++;
        state.red = red;
        state.green = green;
        state.blue = blue;
        state.alpha = alpha;
}

static long
get_width_of_control (ply_label_plugin_control_t *control)
{
        if (control != NULL)
                state.width_count++;
        return 123;
}

static long
get_height_of_control (ply_label_plugin_control_t *control)
{
        if (control != NULL)
                state.height_count++;
        return 45;
}

static void
set_alignment_for_control (ply_label_plugin_control_t *control,
                           ply_label_alignment_t       alignment)
{
        if (control != NULL)
                state.alignment_count++;
        state.alignment = alignment;
}

static void
set_width_for_control (ply_label_plugin_control_t *control,
                       long                        width)
{
        if (control != NULL)
                state.width_setting_count++;
        state.width = width;
}

const ply_label_plugin_interface_t *
ply_label_plugin_get_interface (void)
{
        static const ply_label_plugin_interface_t interface = {
                .create_control            = create_control,
                .destroy_control           = destroy_control,
                .show_control              = show_control,
                .hide_control              = hide_control,
                .draw_control              = draw_control,
                .is_control_hidden         = is_control_hidden,
                .set_text_for_control      = set_text_for_control,
                .set_rich_text_for_control = set_rich_text_for_control,
                .set_font_for_control      = set_font_for_control,
                .set_color_for_control     = set_color_for_control,
                .get_width_of_control      = get_width_of_control,
                .get_height_of_control     = get_height_of_control,
                .set_alignment_for_control = set_alignment_for_control,
                .set_width_for_control     = set_width_for_control,
        };

        return &interface;
}
