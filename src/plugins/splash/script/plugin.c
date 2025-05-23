/* plugin.c - boot script plugin
 *
 * Copyright (C) 2007, 2008 Red Hat, Inc.
 *               2008, 2009 Charlie Brej <cbrej@cs.man.ac.uk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Written by: Charlie Brej <cbrej@cs.man.ac.uk>
 *             Ray Strode <rstrode@redhat.com>
 */
#include "config.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <values.h>
#include <unistd.h>
#include <wchar.h>

#include "ply-boot-splash-plugin.h"
#include "ply-buffer.h"
#include "ply-entry.h"
#include "ply-event-loop.h"
#include "ply-key-file.h"
#include "ply-list.h"
#include "ply-logger.h"
#include "ply-image.h"
#include "ply-pixel-display.h"
#include "ply-trigger.h"
#include "ply-utils.h"

#include "script.h"
#include "script-parse.h"
#include "script-object.h"
#include "script-execute.h"
#include "script-lib-image.h"
#include "script-lib-sprite.h"
#include "script-lib-plymouth.h"
#include "script-lib-math.h"
#include "script-lib-string.h"

#include <linux/kd.h>

#ifndef FRAMES_PER_SECOND
#define FRAMES_PER_SECOND 50
#endif

typedef enum
{
        PLY_BOOT_SPLASH_DISPLAY_NORMAL,
        PLY_BOOT_SPLASH_DISPLAY_QUESTION_ENTRY,
        PLY_BOOT_SPLASH_DISPLAY_PASSWORD_ENTRY
} ply_boot_splash_display_type_t;

struct _ply_boot_splash_plugin
{
        ply_event_loop_t              *loop;
        ply_boot_splash_mode_t         mode;
        ply_list_t                    *displays;
        ply_keyboard_t                *keyboard;

        ply_boot_splash_display_type_t state;
        char                          *script_filename;
        char                          *image_dir;

        ply_list_t                    *script_env_vars;
        script_op_t                   *script_main_op;

        script_state_t                *script_state;
        script_lib_sprite_data_t      *script_sprite_lib;
        script_lib_image_data_t       *script_image_lib;
        script_lib_plymouth_data_t    *script_plymouth_lib;
        script_lib_math_data_t        *script_math_lib;
        script_lib_string_data_t      *script_string_lib;

        uint32_t                       is_animating : 1;

        char                          *monospace_font;
        uint32_t                       should_show_console_messages : 1;
        ply_buffer_t                  *boot_buffer;
        uint32_t                       console_text_color;
        uint32_t                       console_background_color;
};

typedef struct
{
        char *key;
        char *value;
} script_env_var_t;

static void detach_from_event_loop (ply_boot_splash_plugin_t *plugin);
static void stop_animation (ply_boot_splash_plugin_t *plugin);
ply_boot_splash_plugin_interface_t *ply_boot_splash_plugin_get_interface (void);
static void on_keyboard_input (ply_boot_splash_plugin_t *plugin,
                               const char               *keyboard_input,
                               size_t                    character_size);
static void on_boot_output (ply_boot_splash_plugin_t *plugin,
                            const char               *output,
                            size_t                    size);
static void toggle_console_messages (ply_boot_splash_plugin_t *plugin);
static void display_console_messages (ply_boot_splash_plugin_t *plugin);
static void hide_console_messages (ply_boot_splash_plugin_t *plugin);
static void unhide_console_messages (ply_boot_splash_plugin_t *plugin);

static void
show_prompt_on_console_viewer (ply_boot_splash_plugin_t *plugin,
                               const char               *prompt,
                               const char               *entry_text,
                               int                       number_of_bullets)
{
        if (plugin->state == PLY_BOOT_SPLASH_DISPLAY_NORMAL)
                script_lib_sprite_console_viewer_print (plugin->script_sprite_lib, "\n");

        script_lib_sprite_console_viewer_clear_line (plugin->script_sprite_lib);

        script_lib_sprite_console_viewer_print (plugin->script_sprite_lib, prompt);

        script_lib_sprite_console_viewer_print (plugin->script_sprite_lib, ": ");
        if (entry_text)
                script_lib_sprite_console_viewer_print (plugin->script_sprite_lib, "%s", entry_text);

        for (int i = 0; i < number_of_bullets; i++) {
                script_lib_sprite_console_viewer_print (plugin->script_sprite_lib, " ");
        }

        script_lib_sprite_console_viewer_print (plugin->script_sprite_lib, "_");
}

static void
pause_displays (ply_boot_splash_plugin_t *plugin)
{
        ply_list_node_t *node;

        node = ply_list_get_first_node (plugin->displays);
        while (node != NULL) {
                ply_pixel_display_t *display;
                ply_list_node_t *next_node;

                display = ply_list_node_get_data (node);
                next_node = ply_list_get_next_node (plugin->displays, node);

                ply_pixel_display_pause_updates (display);

                node = next_node;
        }
}

static void
unpause_displays (ply_boot_splash_plugin_t *plugin)
{
        ply_list_node_t *node;

        node = ply_list_get_first_node (plugin->displays);
        while (node != NULL) {
                ply_pixel_display_t *display;
                ply_list_node_t *next_node;

                display = ply_list_node_get_data (node);
                next_node = ply_list_get_next_node (plugin->displays, node);

                ply_pixel_display_unpause_updates (display);

                node = next_node;
        }
}

static void
add_script_env_var (const char *group_name,
                    const char *key,
                    const char *value,
                    void       *user_data)
{
        ply_list_t *script_env_vars;
        script_env_var_t *new_env_var;

        if (strcmp (group_name, "script-env-vars") != 0)
                return;

        script_env_vars = user_data;
        new_env_var = malloc (sizeof(script_env_var_t));
        new_env_var->key = strdup (key);
        new_env_var->value = strdup (value);

        ply_list_append_data (script_env_vars, new_env_var);
}

static ply_boot_splash_plugin_t *
create_plugin (ply_key_file_t *key_file)
{
        ply_boot_splash_plugin_t *plugin;

        plugin = calloc (1, sizeof(ply_boot_splash_plugin_t));
        plugin->image_dir = ply_key_file_get_value (key_file,
                                                    "script",
                                                    "ImageDir");
        plugin->script_filename = ply_key_file_get_value (key_file,
                                                          "script",
                                                          "ScriptFile");

        plugin->state = PLY_BOOT_SPLASH_DISPLAY_NORMAL;

        plugin->script_env_vars = ply_list_new ();
        ply_key_file_foreach_entry (key_file, add_script_env_var, plugin->script_env_vars);

        /* Likely only able to set the font if the font is in the initrd */
        plugin->monospace_font = ply_key_file_get_value (key_file, "script", "MonospaceFont");

        if (plugin->monospace_font == NULL)
                plugin->monospace_font = strdup ("monospace 10");

        plugin->console_text_color =
                ply_key_file_get_ulong (key_file, "script",
                                        "ConsoleLogTextColor",
                                        PLY_CONSOLE_VIEWER_LOG_TEXT_COLOR);

        plugin->console_background_color =
                ply_key_file_get_ulong (key_file, "script",
                                        "ConsoleLogBackgroundColor",
                                        0x00000000);

        plugin->displays = ply_list_new ();

        return plugin;
}

static void
destroy_plugin (ply_boot_splash_plugin_t *plugin)
{
        ply_list_node_t *node;
        script_env_var_t *env_var;

        if (plugin == NULL)
                return;

        if (plugin->loop != NULL) {
                stop_animation (plugin);

                ply_event_loop_stop_watching_for_exit (plugin->loop,
                                                       (ply_event_loop_exit_handler_t)
                                                       detach_from_event_loop,
                                                       plugin);
                detach_from_event_loop (plugin);
        }

        for (node = ply_list_get_first_node (plugin->script_env_vars);
             node != NULL;
             node = ply_list_get_next_node (plugin->script_env_vars, node)) {
                env_var = ply_list_node_get_data (node);
                free (env_var->key);
                free (env_var->value);
                free (env_var);
        }
        ply_list_free (plugin->script_env_vars);
        free (plugin->script_filename);
        free (plugin->image_dir);
        free (plugin->monospace_font);
        free (plugin);
}

static void
on_timeout (ply_boot_splash_plugin_t *plugin)
{
        double sleep_time;

        sleep_time = 1.0 / plugin->script_plymouth_lib->refresh_rate;
        ply_event_loop_watch_for_timeout (plugin->loop,
                                          sleep_time,
                                          (ply_event_loop_timeout_handler_t)
                                          on_timeout, plugin);

        script_lib_plymouth_on_refresh (plugin->script_state,
                                        plugin->script_plymouth_lib);

        pause_displays (plugin);
        script_lib_sprite_set_needs_redraw (plugin->script_sprite_lib);
        script_lib_sprite_refresh (plugin->script_sprite_lib);
        unpause_displays (plugin);
}

static void
on_boot_progress (ply_boot_splash_plugin_t *plugin,
                  double                    duration,
                  double                    fraction_done)
{
        script_lib_plymouth_on_boot_progress (plugin->script_state,
                                              plugin->script_plymouth_lib,
                                              duration,
                                              fraction_done);
}

static bool
start_script_animation (ply_boot_splash_plugin_t *plugin)
{
        ply_list_node_t *node;
        script_obj_t *target_obj;
        script_obj_t *value_obj;
        script_env_var_t *env_var;

        assert (plugin != NULL);

        plugin->script_state = script_state_new (plugin);

        for (node = ply_list_get_first_node (plugin->script_env_vars);
             node != NULL;
             node = ply_list_get_next_node (plugin->script_env_vars, node)) {
                env_var = ply_list_node_get_data (node);
                target_obj = script_obj_hash_get_element (plugin->script_state->global,
                                                          env_var->key);
                value_obj = script_obj_new_string (env_var->value);
                script_obj_assign (target_obj, value_obj);
        }

        plugin->script_image_lib = script_lib_image_setup (plugin->script_state,
                                                           plugin->image_dir);
        plugin->script_sprite_lib = script_lib_sprite_setup (plugin->script_state,
                                                             plugin->displays,
                                                             plugin->boot_buffer,
                                                             plugin->monospace_font,
                                                             plugin->console_text_color,
                                                             plugin->console_background_color);
        plugin->script_plymouth_lib = script_lib_plymouth_setup (plugin->script_state,
                                                                 plugin->mode,
                                                                 FRAMES_PER_SECOND,
                                                                 plugin->keyboard);
        plugin->script_math_lib = script_lib_math_setup (plugin->script_state);
        plugin->script_string_lib = script_lib_string_setup (plugin->script_state);

        ply_trace ("executing script file");
        script_return_t ret = script_execute (plugin->script_state,
                                              plugin->script_main_op);

        script_obj_unref (ret.object);
        if (plugin->keyboard != NULL)
                ply_keyboard_add_input_handler (plugin->keyboard,
                                                (ply_keyboard_input_handler_t)
                                                on_keyboard_input, plugin);
        on_timeout (plugin);

        return true;
}

static bool
start_animation (ply_boot_splash_plugin_t *plugin)
{
        assert (plugin != NULL);
        assert (plugin->loop != NULL);

        if (plugin->is_animating)
                return true;

        ply_trace ("parsing script file");
        plugin->script_main_op = script_parse_file (plugin->script_filename);

        start_script_animation (plugin);

        plugin->is_animating = true;
        return true;
}

static void
stop_script_animation (ply_boot_splash_plugin_t *plugin)
{
        script_lib_plymouth_on_quit (plugin->script_state,
                                     plugin->script_plymouth_lib);
        script_lib_sprite_refresh (plugin->script_sprite_lib);

        if (plugin->loop != NULL)
                ply_event_loop_stop_watching_for_timeout (plugin->loop,
                                                          (ply_event_loop_timeout_handler_t)
                                                          on_timeout, plugin);

        if (plugin->keyboard != NULL) {
                ply_keyboard_remove_input_handler (plugin->keyboard,
                                                   (ply_keyboard_input_handler_t)
                                                   on_keyboard_input);
                plugin->keyboard = NULL;
        }

        script_state_destroy (plugin->script_state);
        script_lib_sprite_destroy (plugin->script_sprite_lib);
        plugin->script_sprite_lib = NULL;
        script_lib_image_destroy (plugin->script_image_lib);
        script_lib_plymouth_destroy (plugin->script_plymouth_lib);
        script_lib_math_destroy (plugin->script_math_lib);
        script_lib_string_destroy (plugin->script_string_lib);
}

static void
stop_animation (ply_boot_splash_plugin_t *plugin)
{
        assert (plugin != NULL);
        assert (plugin->loop != NULL);

        if (!plugin->is_animating)
                return;
        plugin->is_animating = false;

        stop_script_animation (plugin);

        script_parse_op_free (plugin->script_main_op);
}

static void
detach_from_event_loop (ply_boot_splash_plugin_t *plugin)
{
        plugin->loop = NULL;
}

static void
on_keyboard_input (ply_boot_splash_plugin_t *plugin,
                   const char               *keyboard_input,
                   size_t                    character_size)
{
        char keyboard_string[character_size + 1];

        memcpy (keyboard_string, keyboard_input, character_size);
        keyboard_string[character_size] = '\0';

        script_lib_plymouth_on_keyboard_input (plugin->script_state,
                                               plugin->script_plymouth_lib,
                                               keyboard_string);
}

static void
set_keyboard (ply_boot_splash_plugin_t *plugin,
              ply_keyboard_t           *keyboard)
{
        plugin->keyboard = keyboard;
}

static void
unset_keyboard (ply_boot_splash_plugin_t *plugin,
                ply_keyboard_t           *keyboard)
{
        plugin->keyboard = NULL;
}

static void
add_pixel_display (ply_boot_splash_plugin_t *plugin,
                   ply_pixel_display_t      *display)
{
        ply_list_append_data (plugin->displays, display);

        if (plugin->script_sprite_lib != NULL) {
                script_lib_sprite_pixel_display_added (plugin->script_sprite_lib, display);
                script_lib_plymouth_on_display_hotplug (plugin->script_state, plugin->script_plymouth_lib);
        }
}

static void
remove_pixel_display (ply_boot_splash_plugin_t *plugin,
                      ply_pixel_display_t      *display)
{
        if (plugin->script_sprite_lib != NULL) {
                script_lib_sprite_pixel_display_removed (plugin->script_sprite_lib, display);
                script_lib_plymouth_on_display_hotplug (plugin->script_state, plugin->script_plymouth_lib);
        }

        ply_list_remove_data (plugin->displays, display);
}

static bool
show_splash_screen (ply_boot_splash_plugin_t *plugin,
                    ply_event_loop_t         *loop,
                    ply_buffer_t             *boot_buffer,
                    ply_boot_splash_mode_t    mode)
{
        assert (plugin != NULL);

        if (ply_list_get_length (plugin->displays) == 0) {
                ply_trace ("no pixel displays");
                return false;
        }

        plugin->loop = loop;
        plugin->mode = mode;

        if (boot_buffer && ply_console_viewer_preferred ())
                plugin->boot_buffer = boot_buffer;

        ply_event_loop_watch_for_exit (loop, (ply_event_loop_exit_handler_t)
                                       detach_from_event_loop,
                                       plugin);

        plugin->should_show_console_messages = false;

        ply_trace ("starting boot animation");
        return start_animation (plugin);
}

static void
system_update (ply_boot_splash_plugin_t *plugin,
               int                       progress)
{
        script_lib_plymouth_on_system_update (plugin->script_state,
                                              plugin->script_plymouth_lib,
                                              progress);
}

static void
update_status (ply_boot_splash_plugin_t *plugin,
               const char               *status)
{
        script_lib_plymouth_on_update_status (plugin->script_state,
                                              plugin->script_plymouth_lib,
                                              status);
}

static void
hide_splash_screen (ply_boot_splash_plugin_t *plugin,
                    ply_event_loop_t         *loop)
{
        assert (plugin != NULL);

        if (plugin->loop != NULL) {
                stop_animation (plugin);

                ply_event_loop_stop_watching_for_exit (plugin->loop,
                                                       (ply_event_loop_exit_handler_t)
                                                       detach_from_event_loop,
                                                       plugin);
                detach_from_event_loop (plugin);
        }
}

static void
on_root_mounted (ply_boot_splash_plugin_t *plugin)
{
        script_lib_plymouth_on_root_mounted (plugin->script_state,
                                             plugin->script_plymouth_lib);
}

static void
become_idle (ply_boot_splash_plugin_t *plugin,
             ply_trigger_t            *idle_trigger)
{
        ply_trigger_pull (idle_trigger, NULL);
}

static void
display_normal (ply_boot_splash_plugin_t *plugin)
{
        pause_displays (plugin);

        if (plugin->state != PLY_BOOT_SPLASH_DISPLAY_NORMAL) {
                if (plugin->state == PLY_BOOT_SPLASH_DISPLAY_PASSWORD_ENTRY)
                        script_lib_sprite_console_viewer_clear_line (plugin->script_sprite_lib);

                script_lib_sprite_console_viewer_print (plugin->script_sprite_lib, "\n");
        }

        script_lib_plymouth_on_display_normal (plugin->script_state,
                                               plugin->script_plymouth_lib);

        script_lib_update_displays (plugin->script_sprite_lib);
        script_lib_sprite_refresh (plugin->script_sprite_lib);
        plugin->state = PLY_BOOT_SPLASH_DISPLAY_NORMAL;
        unpause_displays (plugin);
}

static void
display_password (ply_boot_splash_plugin_t *plugin,
                  const char               *prompt,
                  int                       bullets)
{
        plugin->state = PLY_BOOT_SPLASH_DISPLAY_PASSWORD_ENTRY;
        pause_displays (plugin);
        show_prompt_on_console_viewer (plugin, prompt, NULL, bullets);
        script_lib_plymouth_on_display_password (plugin->script_state,
                                                 plugin->script_plymouth_lib,
                                                 prompt,
                                                 bullets);
        script_lib_update_displays (plugin->script_sprite_lib);
        script_lib_sprite_refresh (plugin->script_sprite_lib);
        unpause_displays (plugin);
}

static void
display_question (ply_boot_splash_plugin_t *plugin,
                  const char               *prompt,
                  const char               *entry_text)
{
        plugin->state = PLY_BOOT_SPLASH_DISPLAY_QUESTION_ENTRY;
        pause_displays (plugin);
        show_prompt_on_console_viewer (plugin, prompt, entry_text, -1);
        script_lib_plymouth_on_display_question (plugin->script_state,
                                                 plugin->script_plymouth_lib,
                                                 prompt,
                                                 entry_text);
        script_lib_update_displays (plugin->script_sprite_lib);
        script_lib_sprite_refresh (plugin->script_sprite_lib);
        unpause_displays (plugin);
}

static bool
validate_input (ply_boot_splash_plugin_t *plugin,
                const char               *entry_text,
                const char               *add_text)
{
        bool ret = script_lib_plymouth_on_validate_input (plugin->script_state,
                                                          plugin->script_plymouth_lib,
                                                          entry_text,
                                                          add_text);
        if (!ply_console_viewer_preferred ())
                return true;

        if (strcmp (add_text, "\e") == 0) {
                toggle_console_messages (plugin);
                return false;
        }

        return ret;
}

static void
display_prompt (ply_boot_splash_plugin_t *plugin,
                const char               *prompt,
                const char               *entry_text,
                bool                      is_secret)
{
        pause_displays (plugin);
        script_lib_plymouth_on_display_prompt (plugin->script_state,
                                               plugin->script_plymouth_lib,
                                               prompt,
                                               entry_text,
                                               is_secret);
        script_lib_update_displays (plugin->script_sprite_lib);
        script_lib_sprite_refresh (plugin->script_sprite_lib);
        unpause_displays (plugin);
}

static void
display_message (ply_boot_splash_plugin_t *plugin,
                 const char               *message)
{
        pause_displays (plugin);
        script_lib_plymouth_on_display_message (plugin->script_state,
                                                plugin->script_plymouth_lib,
                                                message);
        script_lib_sprite_console_viewer_print (plugin->script_sprite_lib,
                                                message);
        script_lib_update_displays (plugin->script_sprite_lib);
        script_lib_sprite_refresh (plugin->script_sprite_lib);
        unpause_displays (plugin);
}

static void
hide_message (ply_boot_splash_plugin_t *plugin,
              const char               *message)
{
        pause_displays (plugin);
        script_lib_plymouth_on_hide_message (plugin->script_state,
                                             plugin->script_plymouth_lib,
                                             message);
        script_lib_update_displays (plugin->script_sprite_lib);
        script_lib_sprite_refresh (plugin->script_sprite_lib);
        unpause_displays (plugin);
}

static void
toggle_console_messages (ply_boot_splash_plugin_t *plugin)
{
        if (plugin->should_show_console_messages) {
                plugin->should_show_console_messages = false;
                hide_console_messages (plugin);
        } else {
                unhide_console_messages (plugin);
        }
}

static void
display_console_messages (ply_boot_splash_plugin_t *plugin)
{
        pause_displays (plugin);
        script_lib_sprite_console_viewer_show (plugin->script_sprite_lib);
        unpause_displays (plugin);
}

static void
unhide_console_messages (ply_boot_splash_plugin_t *plugin)
{
        plugin->should_show_console_messages = true;
        display_console_messages (plugin);
}

static void
hide_console_messages (ply_boot_splash_plugin_t *plugin)
{
        plugin->should_show_console_messages = false;
        script_lib_sprite_console_viewer_hide (plugin->script_sprite_lib);
}

static void
on_boot_output (ply_boot_splash_plugin_t *plugin,
                const char               *output,
                size_t                    size)
{
        ply_list_t *displays;
        ply_list_node_t *node;

        if (!ply_console_viewer_preferred ())
                return;

        displays = script_lib_get_displays (plugin->script_sprite_lib);
        node = ply_list_get_first_node (displays);
        while (node != NULL) {
                script_lib_display_t *display;
                ply_list_node_t *next_node;

                display = ply_list_node_get_data (node);
                next_node = ply_list_get_next_node (displays, node);

                if (display->console_viewer != NULL)
                        ply_console_viewer_write (display->console_viewer, output, size);

                node = next_node;
        }
}

ply_boot_splash_plugin_interface_t *
ply_boot_splash_plugin_get_interface (void)
{
        static ply_boot_splash_plugin_interface_t plugin_interface =
        {
                .create_plugin        = create_plugin,
                .destroy_plugin       = destroy_plugin,
                .set_keyboard         = set_keyboard,
                .unset_keyboard       = unset_keyboard,
                .add_pixel_display    = add_pixel_display,
                .remove_pixel_display = remove_pixel_display,
                .show_splash_screen   = show_splash_screen,
                .system_update        = system_update,
                .update_status        = update_status,
                .on_boot_progress     = on_boot_progress,
                .hide_splash_screen   = hide_splash_screen,
                .on_root_mounted      = on_root_mounted,
                .become_idle          = become_idle,
                .display_normal       = display_normal,
                .display_password     = display_password,
                .display_question     = display_question,
                .display_prompt       = display_prompt,
                .display_message      = display_message,
                .hide_message         = hide_message,
                .on_boot_output       = on_boot_output,
                .validate_input       = validate_input,
        };

        return &plugin_interface;
}

