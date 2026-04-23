/* console-viewer - boot splash plugin
 *
 * Copyright (C) 2026
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
 * Based off of the fade-throbber plugin by Ray Strode <rstrode@redhat.com>
 */

#include <assert.h>
#include <stdlib.h>

#include "ply-boot-splash-plugin.h"
#include "ply-logger.h"
#include "ply-console-viewer.h"

typedef enum
{
        PLY_BOOT_SPLASH_DISPLAY_NORMAL,
        PLY_BOOT_SPLASH_DISPLAY_QUESTION_ENTRY,
        PLY_BOOT_SPLASH_DISPLAY_PASSWORD_ENTRY
} ply_boot_splash_display_type_t;

typedef struct
{
        ply_boot_splash_plugin_t *plugin;
        ply_pixel_display_t      *display;

        ply_console_viewer_t     *console_viewer;
} view_t;

struct _ply_boot_splash_plugin
{
        ply_event_loop_t              *loop;
        ply_boot_splash_mode_t         mode;
        ply_list_t                    *views;

        ply_boot_splash_display_type_t state;

        uint32_t                       needs_redraw : 1;
        uint32_t                       is_animating : 1;
        uint32_t                       is_visible : 1;

        char                          *monospace_font;
        uint32_t                       plugin_console_messages_updating : 1;
        ply_buffer_t                  *boot_buffer;
        uint32_t                       console_text_color;
        uint32_t                       console_background_color;
};

ply_boot_splash_plugin_interface_t *ply_boot_splash_plugin_get_interface (void);
static bool validate_input (ply_boot_splash_plugin_t *plugin,
                            const char               *entry_text,
                            const char               *add_text);
static void on_boot_output (ply_boot_splash_plugin_t *plugin,
                            const char               *output,
                            size_t                    size);
static void display_console_messages (ply_boot_splash_plugin_t *plugin);

static void
view_show_prompt_on_console_viewer (view_t     *view,
                                    const char *prompt,
                                    const char *entry_text,
                                    int         number_of_bullets)
{
        ply_boot_splash_plugin_t *plugin = view->plugin;

        if (view->console_viewer == NULL)
                return;

        if (plugin->state == PLY_BOOT_SPLASH_DISPLAY_NORMAL)
                ply_console_viewer_print (view->console_viewer, "\n");

        ply_console_viewer_clear_line (view->console_viewer);

        ply_console_viewer_print (view->console_viewer, prompt);

        ply_console_viewer_print (view->console_viewer, ": ");
        if (entry_text)
                ply_console_viewer_print (view->console_viewer, "%s", entry_text);

        for (int i = 0; i < number_of_bullets; i++) {
                ply_console_viewer_print (view->console_viewer, " ");
        }

        ply_console_viewer_print (view->console_viewer, "_");
}

static void
view_hide_prompt (view_t *view)
{
        ply_boot_splash_plugin_t *plugin;

        assert (view != NULL);

        plugin = view->plugin;

        if (view->console_viewer != NULL) {
                /* Obscure the password length in the scroll back */
                if (plugin->state == PLY_BOOT_SPLASH_DISPLAY_PASSWORD_ENTRY)
                        ply_console_viewer_clear_line (view->console_viewer);

                /* Remove the last character that acts as the fake cursor when prompting */
                if (plugin->state == PLY_BOOT_SPLASH_DISPLAY_QUESTION_ENTRY)
                        ply_console_viewer_print (view->console_viewer, "\b ");

                ply_console_viewer_print (view->console_viewer, "\n");
        }
}


static ply_boot_splash_plugin_t *
create_plugin (ply_key_file_t *key_file)
{
        ply_boot_splash_plugin_t *plugin;

        plugin = calloc (1, sizeof(ply_boot_splash_plugin_t));

        plugin->plugin_console_messages_updating = false;

        /* Likely only able to set the font if the font is in the initrd */
        plugin->monospace_font = ply_key_file_get_value (key_file, "console-viewer", "MonospaceFont");

        if (plugin->monospace_font == NULL)
                plugin->monospace_font = strdup ("monospace 10");

        plugin->console_text_color =
                ply_key_file_get_ulong (key_file, "console-viewer",
                                        "ConsoleLogTextColor",
                                        PLY_CONSOLE_VIEWER_LOG_TEXT_COLOR);

        plugin->console_background_color =
                ply_key_file_get_ulong (key_file, "console-viewer",
                                        "ConsoleLogBackgroundColor",
                                        0x00000000);

        plugin->state = PLY_BOOT_SPLASH_DISPLAY_NORMAL;
        plugin->views = ply_list_new ();

        plugin->needs_redraw = true;

        return plugin;
}

static void detach_from_event_loop (ply_boot_splash_plugin_t *plugin);

static view_t *
view_new (ply_boot_splash_plugin_t *plugin,
          ply_pixel_display_t      *display)
{
        view_t *view;

        view = calloc (1, sizeof(view_t));
        view->plugin = plugin;
        view->display = display;

        if (ply_console_viewer_preferred ()) {
                view->console_viewer = ply_console_viewer_new (view->display, plugin->monospace_font);
                ply_console_viewer_set_text_color (view->console_viewer, plugin->console_text_color);

                if (plugin->boot_buffer)
                        ply_console_viewer_convert_boot_buffer (view->console_viewer, plugin->boot_buffer);
        } else {
                view->console_viewer = NULL;
        }

        return view;
}

static void
view_free (view_t *view)
{
        ply_console_viewer_free (view->console_viewer);

        ply_pixel_display_set_draw_handler (view->display, NULL, NULL);

        free (view);
}

static void
view_redraw (view_t *view)
{
        unsigned long screen_width, screen_height;

        screen_width = ply_pixel_display_get_width (view->display);
        screen_height = ply_pixel_display_get_height (view->display);

        ply_pixel_display_draw_area (view->display, 0, 0,
                                     screen_width, screen_height);
}

static void
redraw_views (ply_boot_splash_plugin_t *plugin)
{
        plugin->needs_redraw = true;
}

static void
process_needed_redraws (ply_boot_splash_plugin_t *plugin)
{
        ply_list_node_t *node;

        if (!plugin->needs_redraw)
                return;

        node = ply_list_get_first_node (plugin->views);
        while (node != NULL) {
                ply_list_node_t *next_node;
                view_t *view;

                view = ply_list_node_get_data (node);
                next_node = ply_list_get_next_node (plugin->views, node);

                view_redraw (view);

                node = next_node;
        }

        plugin->needs_redraw = false;
}

static void
pause_views (ply_boot_splash_plugin_t *plugin)
{
        ply_list_node_t *node;

        node = ply_list_get_first_node (plugin->views);
        while (node != NULL) {
                ply_list_node_t *next_node;
                view_t *view;

                view = ply_list_node_get_data (node);
                next_node = ply_list_get_next_node (plugin->views, node);

                ply_pixel_display_pause_updates (view->display);

                node = next_node;
        }
}

static void
unpause_views (ply_boot_splash_plugin_t *plugin)
{
        ply_list_node_t *node;

        node = ply_list_get_first_node (plugin->views);
        while (node != NULL) {
                ply_list_node_t *next_node;
                view_t *view;

                view = ply_list_node_get_data (node);
                next_node = ply_list_get_next_node (plugin->views, node);

                ply_pixel_display_unpause_updates (view->display);

                node = next_node;
        }
}

static void
free_views (ply_boot_splash_plugin_t *plugin)
{
        ply_list_node_t *node;

        node = ply_list_get_first_node (plugin->views);

        while (node != NULL) {
                ply_list_node_t *next_node;
                view_t *view;

                view = ply_list_node_get_data (node);
                next_node = ply_list_get_next_node (plugin->views, node);

                view_free (view);
                ply_list_remove_node (plugin->views, node);

                node = next_node;
        }

        ply_list_free (plugin->views);
        plugin->views = NULL;
}

static void
destroy_plugin (ply_boot_splash_plugin_t *plugin)
{
        if (plugin == NULL)
                return;

        if (plugin->loop != NULL) {
                ply_event_loop_stop_watching_for_exit (plugin->loop, (ply_event_loop_exit_handler_t)
                                                       detach_from_event_loop,
                                                       plugin);
                detach_from_event_loop (plugin);
        }

        free_views (plugin);
        free (plugin->monospace_font);
        free (plugin);
}

static void
detach_from_event_loop (ply_boot_splash_plugin_t *plugin)
{
        plugin->loop = NULL;
}

static void
draw_background (view_t             *view,
                 ply_pixel_buffer_t *pixel_buffer,
                 int                 x,
                 int                 y,
                 int                 width,
                 int                 height)
{
        ply_boot_splash_plugin_t *plugin;
        ply_rectangle_t area;

        area.x = x;
        area.y = y;
        area.width = width;
        area.height = height;

        plugin = view->plugin;

        ply_pixel_buffer_fill_with_hex_color (pixel_buffer, &area, plugin->console_background_color);
}

static void
on_draw (view_t             *view,
         ply_pixel_buffer_t *pixel_buffer,
         int                 x,
         int                 y,
         int                 width,
         int                 height)
{
        ply_boot_splash_plugin_t *plugin;

        plugin = view->plugin;

        draw_background (view, pixel_buffer, x, y, width, height);

        if (!plugin->plugin_console_messages_updating && view->console_viewer != NULL)
                ply_console_viewer_draw_area (view->console_viewer, pixel_buffer, x, y, width, height);
}

static void
add_pixel_display (ply_boot_splash_plugin_t *plugin,
                   ply_pixel_display_t      *display)
{
        view_t *view;

        view = view_new (plugin, display);

        ply_pixel_display_set_draw_handler (view->display,
                                            (ply_pixel_display_draw_handler_t)
                                            on_draw, view);

        ply_list_append_data (plugin->views, view);
}

static void
remove_pixel_display (ply_boot_splash_plugin_t *plugin,
                      ply_pixel_display_t      *display)
{
        ply_list_node_t *node;

        node = ply_list_get_first_node (plugin->views);
        while (node != NULL) {
                view_t *view;
                ply_list_node_t *next_node;

                view = ply_list_node_get_data (node);
                next_node = ply_list_get_next_node (plugin->views, node);

                if (view->display == display) {
                        view_free (view);
                        ply_list_remove_node (plugin->views, node);
                        return;
                }

                node = next_node;
        }
}

static bool
show_splash_screen (ply_boot_splash_plugin_t *plugin,
                    ply_event_loop_t         *loop,
                    ply_buffer_t             *boot_buffer,
                    ply_boot_splash_mode_t    mode)
{
        ply_list_node_t *node;

        assert (plugin != NULL);

        plugin->loop = loop;
        plugin->mode = mode;

        if (boot_buffer && ply_console_viewer_preferred ()) {
                plugin->boot_buffer = boot_buffer;

                node = ply_list_get_first_node (plugin->views);
                while (node != NULL) {
                        view_t *view;
                        ply_list_node_t *next_node;

                        view = ply_list_node_get_data (node);
                        next_node = ply_list_get_next_node (plugin->views, node);

                        if (view->console_viewer != NULL)
                                ply_console_viewer_convert_boot_buffer (view->console_viewer, plugin->boot_buffer);

                        node = next_node;
                }
        }

        ply_event_loop_watch_for_exit (loop, (ply_event_loop_exit_handler_t)
                                       detach_from_event_loop,
                                       plugin);

        plugin->is_visible = true;

        return true;
}

static void
update_status (ply_boot_splash_plugin_t *plugin,
               const char               *status)
{
        assert (plugin != NULL);
}

static void
show_message (ply_boot_splash_plugin_t *plugin,
              const char               *message)
{
        ply_trace ("Showing message '%s'", message);
        ply_list_node_t *node;
        node = ply_list_get_first_node (plugin->views);
        while (node != NULL) {
                ply_list_node_t *next_node;
                view_t *view;

                view = ply_list_node_get_data (node);
                next_node = ply_list_get_next_node (plugin->views, node);

                if (view->console_viewer != NULL)
                        ply_console_viewer_print (view->console_viewer, "\n%s\n", message);
                node = next_node;
        }
}

static void
hide_splash_screen (ply_boot_splash_plugin_t *plugin,
                    ply_event_loop_t         *loop)
{
        assert (plugin != NULL);

        plugin->is_visible = false;

        if (plugin->loop != NULL) {
                ply_event_loop_stop_watching_for_exit (plugin->loop, (ply_event_loop_exit_handler_t)
                                                       detach_from_event_loop,
                                                       plugin);

                detach_from_event_loop (plugin);
        }
}

static void
show_password_prompt (ply_boot_splash_plugin_t *plugin,
                      const char               *text,
                      int                       number_of_bullets)
{
        ply_list_node_t *node;

        node = ply_list_get_first_node (plugin->views);
        while (node != NULL) {
                ply_list_node_t *next_node;
                view_t *view;

                view = ply_list_node_get_data (node);
                next_node = ply_list_get_next_node (plugin->views, node);

                view_show_prompt_on_console_viewer (view, text, NULL, number_of_bullets);

                node = next_node;
        }
}

static void
show_prompt (ply_boot_splash_plugin_t *plugin,
             const char               *prompt,
             const char               *entry_text)
{
        ply_list_node_t *node;

        node = ply_list_get_first_node (plugin->views);
        while (node != NULL) {
                ply_list_node_t *next_node;
                view_t *view;

                view = ply_list_node_get_data (node);
                next_node = ply_list_get_next_node (plugin->views, node);

                view_show_prompt_on_console_viewer (view, prompt, entry_text, -1);

                node = next_node;
        }
}

static void
hide_prompt (ply_boot_splash_plugin_t *plugin)
{
        ply_list_node_t *node;

        node = ply_list_get_first_node (plugin->views);
        while (node != NULL) {
                ply_list_node_t *next_node;
                view_t *view;

                view = ply_list_node_get_data (node);
                next_node = ply_list_get_next_node (plugin->views, node);

                view_hide_prompt (view);

                node = next_node;
        }
}

static void
display_normal (ply_boot_splash_plugin_t *plugin)
{
        pause_views (plugin);
        if (plugin->state != PLY_BOOT_SPLASH_DISPLAY_NORMAL)
                hide_prompt (plugin);

        plugin->state = PLY_BOOT_SPLASH_DISPLAY_NORMAL;

        redraw_views (plugin);

        display_console_messages (plugin);

        process_needed_redraws (plugin);
        unpause_views (plugin);
}

static void
display_password (ply_boot_splash_plugin_t *plugin,
                  const char               *prompt,
                  int                       bullets)
{
        pause_views (plugin);

        plugin->state = PLY_BOOT_SPLASH_DISPLAY_PASSWORD_ENTRY;
        show_password_prompt (plugin, prompt, bullets);
        redraw_views (plugin);

        display_console_messages (plugin);

        process_needed_redraws (plugin);
        unpause_views (plugin);
}

static void
display_question (ply_boot_splash_plugin_t *plugin,
                  const char               *prompt,
                  const char               *entry_text)
{
        pause_views (plugin);

        plugin->state = PLY_BOOT_SPLASH_DISPLAY_QUESTION_ENTRY;
        show_prompt (plugin, prompt, entry_text);
        redraw_views (plugin);

        display_console_messages (plugin);

        process_needed_redraws (plugin);
        unpause_views (plugin);
}


static void
display_message (ply_boot_splash_plugin_t *plugin,
                 const char               *message)
{
        show_message (plugin, message);
}

static bool
validate_input (ply_boot_splash_plugin_t *plugin,
                const char               *entry_text,
                const char               *add_text)
{
        if (!ply_console_viewer_preferred ())
                return true;

        if (strcmp (add_text, "\e") == 0) {
                return false;
        }

        return true;
}

static void
display_console_messages (ply_boot_splash_plugin_t *plugin)
{
        ply_list_node_t *node;
        view_t *view;

        pause_views (plugin);

        plugin->plugin_console_messages_updating = true;
        node = ply_list_get_first_node (plugin->views);
        while (node != NULL) {
                view = ply_list_node_get_data (node);
                ply_console_viewer_show (view->console_viewer, view->display);
                node = ply_list_get_next_node (plugin->views, node);
        }
        plugin->plugin_console_messages_updating = false;

        redraw_views (plugin);
        process_needed_redraws (plugin);
        unpause_views (plugin);
}

static void
on_boot_output (ply_boot_splash_plugin_t *plugin,
                const char               *output,
                size_t                    size)
{
        ply_list_node_t *node;
        view_t *view;

        if (!ply_console_viewer_preferred ())
                return;

        node = ply_list_get_first_node (plugin->views);
        while (node != NULL) {
                view = ply_list_node_get_data (node);
                ply_console_viewer_write (view->console_viewer, output, size);
                node = ply_list_get_next_node (plugin->views, node);
        }
}

ply_boot_splash_plugin_interface_t *
ply_boot_splash_plugin_get_interface (void)
{
        static ply_boot_splash_plugin_interface_t plugin_interface =
        {
                .create_plugin        = create_plugin,
                .destroy_plugin       = destroy_plugin,
                .add_pixel_display    = add_pixel_display,
                .remove_pixel_display = remove_pixel_display,
                .show_splash_screen   = show_splash_screen,
                .update_status        = update_status,
                .hide_splash_screen   = hide_splash_screen,
                .display_normal       = display_normal,
                .display_password     = display_password,
                .display_question     = display_question,
                .display_message      = display_message,
                .on_boot_output       = on_boot_output,
                .validate_input       = validate_input,
        };

        return &plugin_interface;
}

