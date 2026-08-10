/* plymouthd-input.c - internal keyboard input routing
 *
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#include "plymouthd-input-private.h"

#include "ply-boot-splash.h"
#include "ply-keyboard.h"
#include "ply-logger.h"
#include "plymouthd-display-private.h"
#include "plymouthd-interaction-private.h"
#include "plymouthd-state-private.h"

static void
on_keyboard_input (plymouthd_t *daemon,
                   const char  *keyboard_input,
                   size_t       character_size)
{
        plymouthd_interaction_handle_input (daemon->interaction,
                                            daemon->boot_splash,
                                            keyboard_input,
                                            character_size);
}

static void
on_backspace (plymouthd_t *daemon)
{
        plymouthd_interaction_handle_backspace (daemon->interaction,
                                                daemon->boot_splash);
}

static void
on_enter (plymouthd_t *daemon,
          const char  *line)
{
        plymouthd_interaction_handle_enter (daemon->interaction,
                                            daemon->boot_splash,
                                            line);
}

void
plymouthd_handle_escape (plymouthd_t *daemon,
                         bool         has_vt_console)
{
        ply_trace ("escape key pressed");

        if (plymouthd_validate_prompt_input (daemon->boot_splash,
                                             "",
                                             "\e") &&
            has_vt_console)
                plymouthd_toggle_details (daemon);
}

static void
attach_keyboard (plymouthd_t                     *daemon,
                 ply_keyboard_t                  *keyboard,
                 plymouthd_input_escape_handler_t escape_handler)
{
        ply_trace ("listening for keystrokes");
        ply_keyboard_add_input_handler (
                keyboard,
                (ply_keyboard_input_handler_t) on_keyboard_input,
                daemon);
        ply_trace ("listening for escape");
        ply_keyboard_add_escape_handler (
                keyboard,
                (ply_keyboard_escape_handler_t) escape_handler,
                daemon);
        ply_trace ("listening for backspace");
        ply_keyboard_add_backspace_handler (
                keyboard,
                (ply_keyboard_backspace_handler_t) on_backspace,
                daemon);
        ply_trace ("listening for enter");
        ply_keyboard_add_enter_handler (
                keyboard,
                (ply_keyboard_enter_handler_t) on_enter,
                daemon);
}

void
plymouthd_handle_keyboard_added (plymouthd_t                     *daemon,
                                 ply_keyboard_t                  *keyboard,
                                 plymouthd_input_escape_handler_t escape_handler)
{
        attach_keyboard (daemon, keyboard, escape_handler);

        if (daemon->boot_splash != NULL) {
                ply_trace ("keyboard set after splash loaded, so attaching to splash");
                ply_boot_splash_set_keyboard (daemon->boot_splash, keyboard);
        }
}

static void
detach_keyboard (ply_keyboard_t                  *keyboard,
                 plymouthd_input_escape_handler_t escape_handler)
{
        ply_trace ("no longer listening for keystrokes");
        ply_keyboard_remove_input_handler (
                keyboard,
                (ply_keyboard_input_handler_t) on_keyboard_input);
        ply_trace ("no longer listening for escape");
        ply_keyboard_remove_escape_handler (
                keyboard,
                (ply_keyboard_escape_handler_t) escape_handler);
        ply_trace ("no longer listening for backspace");
        ply_keyboard_remove_backspace_handler (
                keyboard,
                (ply_keyboard_backspace_handler_t) on_backspace);
        ply_trace ("no longer listening for enter");
        ply_keyboard_remove_enter_handler (
                keyboard,
                (ply_keyboard_enter_handler_t) on_enter);
}

void
plymouthd_handle_keyboard_removed (plymouthd_t                     *daemon,
                                   ply_keyboard_t                  *keyboard,
                                   plymouthd_input_escape_handler_t escape_handler)
{
        detach_keyboard (keyboard, escape_handler);

        if (daemon->boot_splash != NULL)
                ply_boot_splash_unset_keyboard (daemon->boot_splash);
}
