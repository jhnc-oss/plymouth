/*
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#ifndef TEST_FAKE_RENDERER_H
#define TEST_FAKE_RENDERER_H

#include <stdbool.h>

#include "ply-renderer.h"

typedef struct
{
        int                                 create_count;
        int                                 destroy_count;
        int                                 open_count;
        int                                 close_count;
        int                                 query_count;
        int                                 change_count;
        int                                 map_count;
        int                                 unmap_count;
        int                                 activate_count;
        int                                 deactivate_count;
        int                                 flush_count;
        int                                 heads_count;
        int                                 buffer_count;
        int                                 input_source_count;
        int                                 open_input_count;
        int                                 set_input_handler_count;
        int                                 close_input_count;
        int                                 panel_count;
        int                                 capslock_count;
        int                                 keymap_count;
        int                                 add_input_device_count;
        int                                 remove_input_device_count;
        bool                                query_force;
        char                                requested_device[64];
        ply_terminal_t                     *terminal;
        ply_terminal_t                     *local_console_terminal;
        ply_renderer_input_source_handler_t input_handler;
        void                               *input_handler_user_data;
        ply_input_device_t                 *input_device;
} test_renderer_plugin_state_t;

typedef const test_renderer_plugin_state_t *
(*test_renderer_plugin_get_state_function_t) (void);

const test_renderer_plugin_state_t *test_renderer_plugin_get_state (void);

#endif /* TEST_FAKE_RENDERER_H */
