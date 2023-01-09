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

#define KMSG_POLLS_PER_SECOND 4

#ifndef FRAMES_PER_SECOND
#define FRAMES_PER_SECOND 50
#endif

struct _ply_boot_splash_plugin
{
        ply_event_loop_t           *loop;
        ply_boot_splash_mode_t      mode;
        ply_list_t                 *displays;
        ply_keyboard_t             *keyboard;

        char                       *script_filename;
        char                       *image_dir;

        ply_list_t                 *script_env_vars;
        script_op_t                *script_main_op;

        script_state_t             *script_state;
        script_lib_sprite_data_t   *script_sprite_lib;
        script_lib_image_data_t    *script_image_lib;
        script_lib_plymouth_data_t *script_plymouth_lib;
        script_lib_math_data_t     *script_math_lib;
        script_lib_string_data_t   *script_string_lib;

        uint32_t                    is_animating : 1;

        char                       *monospace_font;
        uint32_t                    has_new_kmsgs : 1;
        uint32_t                    should_show_kmsg_messages : 1;
        ply_list_t                 *kmsg_messages;
        uint32_t                    kmsg_colors[LOG_PRIMASK + 1];
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
static void attach_kmsg_messages (ply_boot_splash_plugin_t *plugin,
                                  ply_list_t               *kmsg_messages);
static void display_kmsg_messages (ply_boot_splash_plugin_t *plugin);
static void on_update_kmsg_messages (ply_boot_splash_plugin_t *plugin);
static void hide_kmsg_messages (ply_boot_splash_plugin_t *plugin);
static void unhide_kmsg_messages (ply_boot_splash_plugin_t *plugin);

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

        plugin->script_env_vars = ply_list_new ();
        ply_key_file_foreach_entry (key_file, add_script_env_var, plugin->script_env_vars);

        if (plugin->monospace_font == NULL)
                plugin->monospace_font = strdup ("DejaVu Sans Mono Book 9");

        plugin->kmsg_colors[LOG_EMERG] =
                ply_key_file_get_long (key_file, "script",
                                       "LogLevelEmergColor",
                                       PLY_KMSG_VIEWER_LOG_EMERG_COLOR);

        plugin->kmsg_colors[LOG_ALERT] =
                ply_key_file_get_long (key_file, "script",
                                       "LogLevelAlertColor",
                                       PLY_KMSG_VIEWER_LOG_ALERT_COLOR);

        plugin->kmsg_colors[LOG_CRIT] =
                ply_key_file_get_long (key_file, "script",
                                       "LogLevelCritColor",
                                       PLY_KMSG_VIEWER_LOG_CRIT_COLOR);

        plugin->kmsg_colors[LOG_ERR] =
                ply_key_file_get_long (key_file, "script",
                                       "LogLevelErrColor",
                                       PLY_KMSG_VIEWER_LOG_ERR_COLOR);

        plugin->kmsg_colors[LOG_WARNING] =
                ply_key_file_get_long (key_file, "script",
                                       "LogLevelWarningColor",
                                       PLY_KMSG_VIEWER_LOG_WARNING_COLOR);

        plugin->kmsg_colors[LOG_NOTICE] =
                ply_key_file_get_long (key_file, "script",
                                       "LogLevelNoticeColor",
                                       PLY_KMSG_VIEWER_LOG_NOTICE_COLOR);

        plugin->kmsg_colors[LOG_INFO] =
                ply_key_file_get_long (key_file, "script",
                                       "LogLevelInfoColor",
                                       PLY_KMSG_VIEWER_LOG_INFO_COLOR);

        plugin->kmsg_colors[LOG_DEBUG] =
                ply_key_file_get_long (key_file, "script",
                                       "LogLevelDebugColor",
                                       PLY_KMSG_VIEWER_LOG_DEBUG_COLOR);

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

                ply_event_loop_stop_watching_for_timeout (plugin->loop,
                                                          (ply_event_loop_timeout_handler_t)
                                                          on_update_kmsg_messages, plugin);


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
        ply_list_t *displays;
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
                                                             plugin->displays);
        plugin->script_plymouth_lib = script_lib_plymouth_setup (plugin->script_state,
                                                                 plugin->mode,
                                                                 FRAMES_PER_SECOND);
        plugin->script_math_lib = script_lib_math_setup (plugin->script_state);
        plugin->script_string_lib = script_lib_string_setup (plugin->script_state);


        displays = script_lib_get_displays (plugin->script_sprite_lib);
        node = ply_list_get_first_node (displays);
        while (node != NULL) {
                script_lib_display_t *display;
                ply_list_node_t *next_node;

                display = ply_list_node_get_data (node);
                next_node = ply_list_get_next_node (displays, node);

                display->kmsg_viewer = ply_kmsg_viewer_new (display->pixel_display, plugin->monospace_font);
                ply_kmsg_viewer_attach_kmsg_messages (display->kmsg_viewer, plugin->kmsg_messages);
                ply_kmsg_viewer_set_priority_color (display->kmsg_viewer, LOG_EMERG, plugin->kmsg_colors[LOG_EMERG]);
                ply_kmsg_viewer_set_priority_color (display->kmsg_viewer, LOG_ALERT, plugin->kmsg_colors[LOG_ALERT]);
                ply_kmsg_viewer_set_priority_color (display->kmsg_viewer, LOG_CRIT, plugin->kmsg_colors[LOG_CRIT]);
                ply_kmsg_viewer_set_priority_color (display->kmsg_viewer, LOG_ERR, plugin->kmsg_colors[LOG_ERR]);
                ply_kmsg_viewer_set_priority_color (display->kmsg_viewer, LOG_WARNING, plugin->kmsg_colors[LOG_WARNING]);
                ply_kmsg_viewer_set_priority_color (display->kmsg_viewer, LOG_NOTICE, plugin->kmsg_colors[LOG_NOTICE]);
                ply_kmsg_viewer_set_priority_color (display->kmsg_viewer, LOG_INFO, plugin->kmsg_colors[LOG_INFO]);
                ply_kmsg_viewer_set_priority_color (display->kmsg_viewer, LOG_DEBUG, plugin->kmsg_colors[LOG_DEBUG]);

                node = next_node;
        }

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
}

static void
remove_pixel_display (ply_boot_splash_plugin_t *plugin,
                      ply_pixel_display_t      *display)
{
        script_lib_sprite_pixel_display_removed (plugin->script_sprite_lib, display);
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

        ply_event_loop_watch_for_timeout (loop,
                                          1.0 / KMSG_POLLS_PER_SECOND,
                                          (ply_event_loop_timeout_handler_t)
                                          on_update_kmsg_messages, plugin);

        ply_event_loop_watch_for_exit (loop, (ply_event_loop_exit_handler_t)
                                       detach_from_event_loop,
                                       plugin);

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

                ply_event_loop_stop_watching_for_timeout (plugin->loop,
                                                          (ply_event_loop_timeout_handler_t)
                                                          on_update_kmsg_messages, plugin);

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
        script_lib_plymouth_on_display_normal (plugin->script_state,
                                               plugin->script_plymouth_lib);

        if (plugin->should_show_kmsg_messages == false) {
                unpause_displays (plugin);
        } else {
                pause_displays (plugin);
        }
}

static void
display_password (ply_boot_splash_plugin_t *plugin,
                  const char               *prompt,
                  int                       bullets)
{
        pause_displays (plugin);
        script_lib_plymouth_on_display_password (plugin->script_state,
                                                 plugin->script_plymouth_lib,
                                                 prompt,
                                                 bullets);
        unpause_displays (plugin);
}

static void
display_question (ply_boot_splash_plugin_t *plugin,
                  const char               *prompt,
                  const char               *entry_text)
{
        pause_displays (plugin);
        script_lib_plymouth_on_display_question (plugin->script_state,
                                                 plugin->script_plymouth_lib,
                                                 prompt,
                                                 entry_text);
        unpause_displays (plugin);
}

static bool
validate_input (ply_boot_splash_plugin_t *plugin,
                const char               *entry_text,
                const char               *add_text)
{
        return script_lib_plymouth_on_validate_input (plugin->script_state,
                                                      plugin->script_plymouth_lib,
                                                      entry_text,
                                                      add_text);
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
        unpause_displays (plugin);
}

static void
attach_kmsg_messages (ply_boot_splash_plugin_t *plugin,
                      ply_list_t               *kmsg_messages)
{
        plugin->kmsg_messages = kmsg_messages;
}

static void
on_update_kmsg_messages (ply_boot_splash_plugin_t *plugin)
{
        if (plugin->loop != NULL) {
                ply_event_loop_watch_for_timeout (plugin->loop,
                                                  1.0 / KMSG_POLLS_PER_SECOND,
                                                  (ply_event_loop_timeout_handler_t)
                                                  on_update_kmsg_messages, plugin);
        }

        if (plugin->has_new_kmsgs == false)
                return;

        plugin->has_new_kmsgs = false;

        if (plugin->should_show_kmsg_messages == false)
                return;

        display_kmsg_messages (plugin);
}

static void
display_kmsg_messages (ply_boot_splash_plugin_t *plugin)
{
        pause_displays (plugin);
        script_lib_kmsg_viewer_show (plugin->script_sprite_lib);
        unpause_displays (plugin);
}

static void
unhide_kmsg_messages (ply_boot_splash_plugin_t *plugin)
{
        plugin->should_show_kmsg_messages = true;
        display_kmsg_messages (plugin);
}

static void
hide_kmsg_messages (ply_boot_splash_plugin_t *plugin)
{
        plugin->should_show_kmsg_messages = false;
        script_lib_kmsg_viewer_hide (plugin->script_sprite_lib);
}

static void
update_kmsg_messages (ply_boot_splash_plugin_t *plugin)
{
        plugin->has_new_kmsgs = true;
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
                .validate_input       = validate_input,
                .attach_kmsg_messages = attach_kmsg_messages,
                .unhide_kmsg_messages = unhide_kmsg_messages,
                .hide_kmsg_messages   = hide_kmsg_messages,
                .update_kmsg_messages = update_kmsg_messages,
        };

        return &plugin_interface;
}

