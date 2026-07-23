/*
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#include "fake-splash.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct _ply_boot_splash_plugin
{
        int marker;
};

static test_splash_plugin_state_t state;

const test_splash_plugin_state_t *
test_splash_plugin_get_state (void)
{
        return &state;
}

static void
copy_string (char       *destination,
             size_t      destination_size,
             const char *source)
{
        if (source == NULL)
                source = "";

        snprintf (destination, destination_size, "%s", source);
}

static ply_boot_splash_plugin_t *
create_plugin (ply_key_file_t *key_file)
{
        ply_boot_splash_plugin_t *plugin;
        char *token;

        memset (&state, 0, sizeof(state));
        state.create_count++;

        token = ply_key_file_get_value (key_file, "Test", "Token");
        copy_string (state.theme_token, sizeof(state.theme_token), token);
        free (token);

        plugin = calloc (1, sizeof(ply_boot_splash_plugin_t));
        plugin->marker = 1;
        return plugin;
}

static void
destroy_plugin (ply_boot_splash_plugin_t *plugin)
{
        state.destroy_count += plugin->marker;
        free (plugin);
}

static void
set_keyboard (ply_boot_splash_plugin_t *plugin,
              ply_keyboard_t           *keyboard)
{
        state.set_keyboard_count += plugin->marker;
        state.keyboard = keyboard;
}

static void
unset_keyboard (ply_boot_splash_plugin_t *plugin,
                ply_keyboard_t           *keyboard)
{
        state.unset_keyboard_count += plugin->marker;
        state.keyboard = keyboard;
}

static void
add_pixel_display (ply_boot_splash_plugin_t *plugin,
                   ply_pixel_display_t      *display)
{
        state.add_pixel_display_count += plugin->marker;
        state.pixel_display = display;
}

static void
remove_pixel_display (ply_boot_splash_plugin_t *plugin,
                      ply_pixel_display_t      *display)
{
        state.remove_pixel_display_count += plugin->marker;
        state.pixel_display = display;
}

static void
add_text_display (ply_boot_splash_plugin_t *plugin,
                  ply_text_display_t       *display)
{
        state.add_text_display_count += plugin->marker;
        state.text_display = display;
}

static void
remove_text_display (ply_boot_splash_plugin_t *plugin,
                     ply_text_display_t       *display)
{
        state.remove_text_display_count += plugin->marker;
        state.text_display = display;
}

static bool
show_splash_screen (ply_boot_splash_plugin_t *plugin,
                    ply_event_loop_t         *loop,
                    ply_buffer_t             *boot_buffer,
                    ply_boot_splash_mode_t    mode)
{
        if (state.show_count < 3)
                state.modes[state.show_count] = mode;
        state.show_count += plugin->marker;
        state.show_loop = loop;
        state.boot_buffer = boot_buffer;
        return true;
}

static void
system_update (ply_boot_splash_plugin_t *plugin,
               int                       progress)
{
        state.system_update_count += plugin->marker;
        state.system_update_progress = progress;
}

static void
update_status (ply_boot_splash_plugin_t *plugin,
               const char               *status)
{
        state.status_count += plugin->marker;
        copy_string (state.status, sizeof(state.status), status);
}

static void
on_boot_output (ply_boot_splash_plugin_t *plugin,
                const char               *output,
                size_t                    size)
{
        state.output_count += plugin->marker;
        state.output_size = size < sizeof(state.output) ? size : sizeof(state.output);
        memcpy (state.output, output, state.output_size);
}

static void
on_boot_progress (ply_boot_splash_plugin_t *plugin,
                  double                    duration,
                  double                    fraction_done)
{
        state.progress_count += plugin->marker;
        state.progress_duration = duration;
        state.progress_fraction = fraction_done;
}

static void
on_root_mounted (ply_boot_splash_plugin_t *plugin)
{
        state.root_mounted_count += plugin->marker;
}

static void
hide_splash_screen (ply_boot_splash_plugin_t *plugin,
                    ply_event_loop_t         *loop)
{
        state.hide_count += plugin->marker;
        state.hide_loop = loop;
}

static void
display_message (ply_boot_splash_plugin_t *plugin,
                 const char               *message)
{
        state.display_message_count += plugin->marker;
        copy_string (state.displayed_message, sizeof(state.displayed_message), message);
}

static void
hide_message (ply_boot_splash_plugin_t *plugin,
              const char               *message)
{
        state.hide_message_count += plugin->marker;
        copy_string (state.hidden_message, sizeof(state.hidden_message), message);
}

static void
display_normal (ply_boot_splash_plugin_t *plugin)
{
        state.normal_count += plugin->marker;
}

static void
display_password (ply_boot_splash_plugin_t *plugin,
                  const char               *prompt,
                  int                       bullets)
{
        state.password_count += plugin->marker;
        copy_string (state.password_prompt, sizeof(state.password_prompt), prompt);
        state.password_bullets = bullets;
}

static void
display_question (ply_boot_splash_plugin_t *plugin,
                  const char               *prompt,
                  const char               *entry_text)
{
        state.question_count += plugin->marker;
        copy_string (state.question_prompt, sizeof(state.question_prompt), prompt);
        copy_string (state.question_entry, sizeof(state.question_entry), entry_text);
}

static void
become_idle (ply_boot_splash_plugin_t *plugin,
             ply_trigger_t            *idle_trigger)
{
        state.idle_count += plugin->marker;
        ply_trigger_pull (idle_trigger, NULL);
}

static void
display_prompt (ply_boot_splash_plugin_t *plugin,
                const char               *prompt,
                const char               *entry_text,
                bool                      is_secret)
{
        state.prompt_count += plugin->marker;
        copy_string (state.prompt, sizeof(state.prompt), prompt);
        copy_string (state.prompt_entry, sizeof(state.prompt_entry), entry_text);
        state.prompt_is_secret = is_secret;
}

static bool
validate_input (ply_boot_splash_plugin_t *plugin,
                const char               *entry_text,
                const char               *add_text)
{
        state.validate_count += plugin->marker;
        copy_string (state.validated_entry, sizeof(state.validated_entry), entry_text);
        copy_string (state.validated_addition, sizeof(state.validated_addition), add_text);
        return strcmp (add_text, "!") != 0;
}

const ply_boot_splash_plugin_interface_t *
ply_boot_splash_plugin_get_interface (void)
{
        static const ply_boot_splash_plugin_interface_t interface = {
                .create_plugin        = create_plugin,
                .destroy_plugin       = destroy_plugin,
                .set_keyboard         = set_keyboard,
                .unset_keyboard       = unset_keyboard,
                .add_pixel_display    = add_pixel_display,
                .remove_pixel_display = remove_pixel_display,
                .add_text_display     = add_text_display,
                .remove_text_display  = remove_text_display,
                .show_splash_screen   = show_splash_screen,
                .system_update        = system_update,
                .update_status        = update_status,
                .on_boot_output       = on_boot_output,
                .on_boot_progress     = on_boot_progress,
                .on_root_mounted      = on_root_mounted,
                .hide_splash_screen   = hide_splash_screen,
                .display_message      = display_message,
                .hide_message         = hide_message,
                .display_normal       = display_normal,
                .display_password     = display_password,
                .display_question     = display_question,
                .become_idle          = become_idle,
                .display_prompt       = display_prompt,
                .validate_input       = validate_input,
        };

        return &interface;
}
