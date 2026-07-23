/*
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#ifndef TEST_FAKE_SPLASH_H
#define TEST_FAKE_SPLASH_H

#include "ply-boot-splash-plugin.h"

typedef struct
{
        int                    create_count;
        int                    destroy_count;
        int                    set_keyboard_count;
        int                    unset_keyboard_count;
        int                    add_pixel_display_count;
        int                    remove_pixel_display_count;
        int                    add_text_display_count;
        int                    remove_text_display_count;
        int                    show_count;
        int                    system_update_count;
        int                    status_count;
        int                    output_count;
        int                    progress_count;
        int                    root_mounted_count;
        int                    hide_count;
        int                    display_message_count;
        int                    hide_message_count;
        int                    normal_count;
        int                    password_count;
        int                    question_count;
        int                    idle_count;
        int                    prompt_count;
        int                    validate_count;
        char                   theme_token[64];
        ply_keyboard_t        *keyboard;
        ply_pixel_display_t   *pixel_display;
        ply_text_display_t    *text_display;
        ply_event_loop_t      *show_loop;
        ply_event_loop_t      *hide_loop;
        ply_buffer_t          *boot_buffer;
        ply_boot_splash_mode_t modes[3];
        int                    system_update_progress;
        char                   status[64];
        char                   output[64];
        size_t                 output_size;
        double                 progress_duration;
        double                 progress_fraction;
        char                   displayed_message[64];
        char                   hidden_message[64];
        char                   password_prompt[64];
        int                    password_bullets;
        char                   question_prompt[64];
        char                   question_entry[64];
        char                   prompt[64];
        char                   prompt_entry[64];
        bool                   prompt_is_secret;
        char                   validated_entry[64];
        char                   validated_addition[64];
} test_splash_plugin_state_t;

typedef const test_splash_plugin_state_t *
(*test_splash_plugin_get_state_function_t) (void);

const test_splash_plugin_state_t *test_splash_plugin_get_state (void);

#endif /* TEST_FAKE_SPLASH_H */
