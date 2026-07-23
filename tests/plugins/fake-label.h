/*
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#ifndef TEST_FAKE_LABEL_H
#define TEST_FAKE_LABEL_H

#include "ply-label.h"

typedef struct
{
        int                   create_count;
        int                   destroy_count;
        int                   show_count;
        int                   hide_count;
        int                   draw_count;
        int                   hidden_count;
        int                   text_count;
        int                   rich_text_count;
        int                   font_count;
        int                   color_count;
        int                   width_count;
        int                   height_count;
        int                   alignment_count;
        int                   width_setting_count;
        bool                  hidden;
        char                  text[64];
        char                  font[64];
        ply_rich_text_t      *rich_text;
        ply_rich_text_span_t  span;
        ply_label_alignment_t alignment;
        long                  width;
        float                 red;
        float                 green;
        float                 blue;
        float                 alpha;
        ply_pixel_display_t  *display;
        long                  show_x;
        long                  show_y;
        ply_pixel_buffer_t   *draw_buffer;
        long                  draw_x;
        long                  draw_y;
        unsigned long         draw_width;
        unsigned long         draw_height;
} test_label_plugin_state_t;

typedef const test_label_plugin_state_t *
(*test_label_plugin_get_state_function_t) (void);

const test_label_plugin_state_t *test_label_plugin_get_state (void);

#endif /* TEST_FAKE_LABEL_H */
