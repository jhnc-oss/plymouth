/* ply-kmsg-viewer.c - kernel log message viewer
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
#ifndef PLY_KMSG_VIEWER_H
#define PLY_KMSG_VIEWER_H

#include "ply-key-file.h"
#include "ply-list.h"
#include "ply-event-loop.h"
#include "ply-pixel-display.h"

typedef struct _ply_kmsg_viewer ply_kmsg_viewer_t;

#define PLY_KMSG_VIEWER_LOG_EMERG_COLOR 0xff0000ff   /* red */
#define PLY_KMSG_VIEWER_LOG_ALERT_COLOR 0xff0000ff   /* red */
#define PLY_KMSG_VIEWER_LOG_CRIT_COLOR 0xff0000ff    /* red */
#define PLY_KMSG_VIEWER_LOG_ERR_COLOR  0xff0000ff    /* red */
#define PLY_KMSG_VIEWER_LOG_WARNING_COLOR 0xffff00ff /* yellow */
#define PLY_KMSG_VIEWER_LOG_NOTICE_COLOR 0x00ff00ff  /* green */
#define PLY_KMSG_VIEWER_LOG_INFO_COLOR 0xffffffff    /* white */
#define PLY_KMSG_VIEWER_LOG_DEBUG_COLOR 0xffffffff   /* white */

#ifndef PLY_HIDE_FUNCTION_DECLARATIONS
ply_kmsg_viewer_t *ply_kmsg_viewer_new (ply_pixel_display_t *display,
                                        const char          *fontdesc);
void ply_kmsg_viewer_free (ply_kmsg_viewer_t *kmsg_viewer);
void ply_kmsg_viewer_show (ply_kmsg_viewer_t   *kmsg_viewer,
                           ply_pixel_display_t *display);
void ply_kmsg_viewer_draw_area (ply_kmsg_viewer_t  *kmsg_viewer,
                                ply_pixel_buffer_t *buffer,
                                long                x,
                                long                y,
                                unsigned long       width,
                                unsigned long       height);
void ply_kmsg_viewer_hide (ply_kmsg_viewer_t *kmsg_viewer);
void ply_kmsg_viewer_attach_kmsg_messages (ply_kmsg_viewer_t *kmsg_viewer,
                                           ply_list_t        *kmsg_messages);
void ply_kmsg_viewer_set_priority_color (ply_kmsg_viewer_t *kmsg_viewer,
                                         int                priority,
                                         uint32_t           hex_color);
#endif //PLY_HIDE_FUNCTION_DECLARATIONS

#endif //PLY_KMSG_VIEWER_H
