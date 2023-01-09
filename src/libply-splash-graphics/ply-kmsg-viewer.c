/* ply-kmsg-view.c - kernel log message viewer
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
 */

#include <stdlib.h>
#include <assert.h>

#include "ply-kmsg-viewer.h"
#include "ply-label.h"
#include "ply-list.h"
#include "ply-pixel-display.h"
#include "ply-image.h"
#include "ply-kmsg-reader.h"

#define KMSG_POLLS_PER_SECOND 4

struct _ply_kmsg_viewer
{
        ply_event_loop_t    *loop;

        ply_list_t          *kmsg_messages;

        ply_pixel_display_t *display;
        ply_rectangle_t      area;

        ply_list_t          *message_labels;

        int                  is_hidden;

        char                *fontdesc;
        long                 font_height;
        long                 font_width;
        int                  line_max_chars;
        int                  messages_updating;

        uint32_t             log_colors[LOG_PRIMASK + 1];
};

ply_kmsg_viewer_t *
ply_kmsg_viewer_new (ply_pixel_display_t *display,
                     const char          *fontdesc)
{
        ply_kmsg_viewer_t *kmsg_viewer;

        kmsg_viewer = calloc (1, sizeof(struct _ply_kmsg_viewer));

        kmsg_viewer->messages_updating = false;
        kmsg_viewer->message_labels = ply_list_new ();

        kmsg_viewer->fontdesc = strdup (fontdesc);

        ply_label_t *kmsg_message_label, *measure_label;
        int label_count;

        measure_label = ply_label_new ();
        ply_label_set_text (measure_label, " ");
        ply_label_set_font (measure_label, kmsg_viewer->fontdesc);

        kmsg_viewer->log_colors[LOG_EMERG] = PLY_KMSG_VIEWER_LOG_EMERG_COLOR;
        kmsg_viewer->log_colors[LOG_ALERT] = PLY_KMSG_VIEWER_LOG_ALERT_COLOR;
        kmsg_viewer->log_colors[LOG_CRIT] = PLY_KMSG_VIEWER_LOG_CRIT_COLOR;
        kmsg_viewer->log_colors[LOG_ERR] = PLY_KMSG_VIEWER_LOG_ERR_COLOR;
        kmsg_viewer->log_colors[LOG_WARNING] = PLY_KMSG_VIEWER_LOG_WARNING_COLOR;
        kmsg_viewer->log_colors[LOG_NOTICE] = PLY_KMSG_VIEWER_LOG_NOTICE_COLOR;
        kmsg_viewer->log_colors[LOG_INFO] = PLY_KMSG_VIEWER_LOG_INFO_COLOR;
        kmsg_viewer->log_colors[LOG_DEBUG] = PLY_KMSG_VIEWER_LOG_DEBUG_COLOR;

        kmsg_viewer->font_height = ply_label_get_height (measure_label);
        kmsg_viewer->font_width = ply_label_get_width (measure_label);
        //Allow the label to be the size of how many characters can fit in the width of the screeen, minus one for larger fonts that have some size overhead
        kmsg_viewer->line_max_chars = ply_pixel_display_get_width (display) / kmsg_viewer->font_width - 1;
        label_count = ply_pixel_display_get_height (display) / kmsg_viewer->font_height;
        ply_label_free (measure_label);

        for (int label_index = 0; label_index < label_count; label_index++) {
                kmsg_message_label = ply_label_new ();
                ply_label_set_font (kmsg_message_label, kmsg_viewer->fontdesc);
                ply_list_append_data (kmsg_viewer->message_labels, kmsg_message_label);
        }

        return kmsg_viewer;
}

void
ply_kmsg_viewer_free (ply_kmsg_viewer_t *kmsg_viewer)
{
        ply_list_node_t *node;
        ply_label_t *kmsg_message_label;

        for (int i = 0; i < ply_list_get_length (kmsg_viewer->message_labels); i++) {
                node = ply_list_get_nth_node (kmsg_viewer->message_labels, i);
                kmsg_message_label = ply_list_node_get_data (node);
                ply_label_free (kmsg_message_label);
        }
        ply_list_free (kmsg_viewer->message_labels);


        free (kmsg_viewer->fontdesc);
        free (kmsg_viewer);
}

void
ply_kmsg_viewer_show (ply_kmsg_viewer_t   *kmsg_viewer,
                      ply_pixel_display_t *display)
{
        assert (kmsg_viewer != NULL);

        kmsg_viewer->display = display;
        kmsg_viewer->is_hidden = false;

        ply_list_node_t *node;
        ply_label_t *kmsg_message_label;
        int label_count, message_number, label_index, string_length_left, utf8_string_offset, utf8_string_range;
        kmsg_message_t *kmsg_message;
        char *kmsg_message_str;
        uint32_t label_color;

        if (kmsg_viewer->kmsg_messages == NULL)
                return;

        label_index = 0;
        label_count = ply_list_get_length (kmsg_viewer->message_labels) - 1;
        message_number = ply_list_get_length (kmsg_viewer->kmsg_messages) - 1;
        while (label_index <= label_count) {
                if (message_number >= 0) {
                        kmsg_message = ply_list_node_get_data (ply_list_get_nth_node (kmsg_viewer->kmsg_messages, message_number));
                        kmsg_message_str = kmsg_message->message;

                        //use the log priority level
                        if (kmsg_message->priority >= LOG_EMERG && kmsg_message->priority <= LOG_DEBUG) {
                                label_color = kmsg_viewer->log_colors[kmsg_message->priority];
                        } else {
                                label_color = kmsg_viewer->log_colors[LOG_INFO];
                        }
                } else {
                        label_color = kmsg_viewer->log_colors[LOG_INFO];
                        kmsg_message_str = "";
                }

                string_length_left = ply_utf8_string_get_length (kmsg_message_str, strlen (kmsg_message_str));
                utf8_string_offset = string_length_left;
                while (string_length_left >= 0) {
                        node = ply_list_get_nth_node (kmsg_viewer->message_labels, label_index);
                        kmsg_message_label = ply_list_node_get_data (node);

                        utf8_string_range = utf8_string_offset % kmsg_viewer->line_max_chars;
                        if (utf8_string_range == 0)
                                utf8_string_range = kmsg_viewer->line_max_chars;

                        utf8_string_offset -= utf8_string_range;
                        string_length_left = utf8_string_offset - 1;

                        //prevent underrun
                        if (utf8_string_offset < 0)
                                utf8_string_offset = 0;

                        ply_label_set_text (kmsg_message_label,
                                            ply_utf_string_get_substring_range (kmsg_message_str, utf8_string_offset, utf8_string_range));
                        ply_label_show (kmsg_message_label, kmsg_viewer->display, kmsg_viewer->font_width / 2,
                                        (ply_pixel_display_get_height (kmsg_viewer->display) - (kmsg_viewer->font_height * label_index) - kmsg_viewer->font_height));
                        ply_label_set_hex_color (kmsg_message_label, label_color);

                        label_index++;
                        if (label_index > label_count)
                                break;

                        utf8_string_offset--;
                }
                message_number--;
                if (message_number < 0)
                        break;
        }
}

void
ply_kmsg_viewer_draw_area (ply_kmsg_viewer_t  *kmsg_viewer,
                           ply_pixel_buffer_t *buffer,
                           long                x,
                           long                y,
                           unsigned long       width,
                           unsigned long       height)
{
        ply_list_node_t *node;
        ply_label_t *kmsg_message_label;

        if (kmsg_viewer->messages_updating == false) {
                kmsg_viewer->messages_updating = true;

                for (int label_index = 0; label_index < ply_list_get_length (kmsg_viewer->message_labels); label_index++) {
                        node = ply_list_get_nth_node (kmsg_viewer->message_labels, label_index);
                        kmsg_message_label = ply_list_node_get_data (node);
                        ply_label_draw_area (kmsg_message_label, buffer, x, y, width, height);
                }
                kmsg_viewer->messages_updating = false;
        }
}

void
ply_kmsg_viewer_hide (ply_kmsg_viewer_t *kmsg_viewer)
{
        int label_count = ply_list_get_length (kmsg_viewer->message_labels);
        ply_list_node_t *node;
        ply_label_t *kmsg_message_label;

        if (kmsg_viewer->is_hidden == true)
                return;

        kmsg_viewer->is_hidden = true;

        kmsg_viewer->messages_updating = true;

        for (int label_index = 0; label_index < label_count; label_index++) {
                node = ply_list_get_nth_node (kmsg_viewer->message_labels, label_index); kmsg_message_label = ply_list_node_get_data (node);
                ply_label_hide (kmsg_message_label);
        }
        kmsg_viewer->messages_updating = false;

        kmsg_viewer->display = NULL;
}

void
ply_kmsg_viewer_attach_kmsg_messages (ply_kmsg_viewer_t *kmsg_viewer,
                                      ply_list_t        *kmsg_messages)
{
        kmsg_viewer->kmsg_messages = kmsg_messages;
}

void
ply_kmsg_viewer_set_priority_color (ply_kmsg_viewer_t *kmsg_viewer,
                                    int                priority,
                                    uint32_t           hex_color)
{
        kmsg_viewer->log_colors[priority] = hex_color;
}
