/* throbber.c - boot throbber
 *
 * Copyright (C) 2022 Hans Christian Schmitz
 * Copyright (C) 2007, 2008 Red Hat, Inc.
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
 * Written by: Ray Strode <rstrode@redhat.com>
 *             Hans Christian Schmitz <git@hcsch.eu>
 */
#include "config.h"

#include <assert.h>
#include <dirent.h>
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

#include "ply-throbber.h"
#include "ply-image-sequence.h"
#include "ply-event-loop.h"
#include "ply-pixel-buffer.h"
#include "ply-pixel-display.h"
#include "ply-array.h"
#include "ply-logger.h"
#include "ply-image.h"
#include "ply-utils.h"

#include <linux/kd.h>

#ifndef FRAMES_PER_SECOND
#define FRAMES_PER_SECOND 30
#endif

#ifndef THROBBER_DURATION
#define THROBBER_DURATION 2.0
#endif

struct _ply_throbber
{
        ply_image_sequence_t *frame_sequence;
        ply_event_loop_t     *loop;

        ply_pixel_display_t  *display;
        ply_trigger_t        *stop_trigger;

        long                  x, y;
        long                  width, height;
        double                start_time, now;

        int                   frame_number;
        uint32_t              is_stopped : 1;
};

static void ply_throbber_stop_now (ply_throbber_t *throbber,
                                   bool            redraw);

ply_throbber_t *
ply_throbber_new (const char *image_dir,
                  const char *frames_prefix,
                  int         device_scale)
{
        ply_throbber_t *throbber;

        assert (image_dir != NULL);
        assert (frames_prefix != NULL);

        throbber = calloc (1, sizeof(ply_throbber_t));

        throbber->frame_sequence = ply_image_sequence_new (image_dir, frames_prefix, device_scale);
        throbber->is_stopped = true;
        throbber->width = 0;
        throbber->height = 0;
        throbber->frame_number = 0;

        return throbber;
}

void
ply_throbber_free (ply_throbber_t *throbber)
{
        if (throbber == NULL)
                return;

        if (!throbber->is_stopped)
                ply_throbber_stop_now (throbber, false);

        ply_image_sequence_free (throbber->frame_sequence);

        free (throbber);
}

static bool
animate_at_time (ply_throbber_t *throbber,
                 double          time)
{
        int number_of_frames;
        bool should_continue;
        double percent_in_sequence;
        int last_frame_number;

        number_of_frames = ply_image_sequence_get_number_of_frames (throbber->frame_sequence);

        if (number_of_frames == 0)
                return true;

        should_continue = true;
        percent_in_sequence = fmod (time, THROBBER_DURATION) / THROBBER_DURATION;
        last_frame_number = throbber->frame_number;
        throbber->frame_number = (int) (number_of_frames * percent_in_sequence);

        if (throbber->stop_trigger != NULL) {
                /* If we are trying to stop, make sure we don't skip the last
                 * frame and loop around. Clamp it to the last frame.
                 */
                if (last_frame_number > throbber->frame_number)
                        throbber->frame_number = number_of_frames - 1;

                if (throbber->frame_number == number_of_frames - 1)
                        should_continue = false;
        }

        ply_pixel_display_draw_area (throbber->display,
                                     throbber->x, throbber->y,
                                     ply_image_sequence_get_width (throbber->frame_sequence),
                                     ply_image_sequence_get_height (throbber->frame_sequence));

        return should_continue;
}

static void
on_timeout (ply_throbber_t *throbber)
{
        double sleep_time;
        bool should_continue;

        throbber->now = ply_get_timestamp ();

        should_continue = animate_at_time (throbber,
                                           throbber->now - throbber->start_time);

        sleep_time = 1.0 / FRAMES_PER_SECOND;
        sleep_time = MAX (sleep_time - (ply_get_timestamp () - throbber->now),
                          0.005);

        if (!should_continue) {
                throbber->is_stopped = true;
                if (throbber->stop_trigger != NULL) {
                        ply_trigger_pull (throbber->stop_trigger, NULL);
                        throbber->stop_trigger = NULL;
                }
        } else {
                ply_event_loop_watch_for_timeout (throbber->loop,
                                                  sleep_time,
                                                  (ply_event_loop_timeout_handler_t)
                                                  on_timeout, throbber);
        }
}

bool
ply_throbber_load (ply_throbber_t *throbber)
{
        return ply_image_sequence_load (throbber->frame_sequence);
}

bool
ply_throbber_start (ply_throbber_t      *throbber,
                    ply_event_loop_t    *loop,
                    ply_pixel_display_t *display,
                    long                 x,
                    long                 y)
{
        assert (throbber != NULL);
        assert (throbber->loop == NULL);

        throbber->loop = loop;
        throbber->display = display;
        throbber->is_stopped = false;

        throbber->x = x;
        throbber->y = y;

        throbber->start_time = ply_get_timestamp ();

        ply_event_loop_watch_for_timeout (throbber->loop,
                                          1.0 / FRAMES_PER_SECOND,
                                          (ply_event_loop_timeout_handler_t)
                                          on_timeout, throbber);

        return true;
}

static void
ply_throbber_stop_now (ply_throbber_t *throbber, bool redraw)
{
        throbber->is_stopped = true;

        if (redraw) {
                ply_pixel_display_draw_area (throbber->display,
                                             throbber->x,
                                             throbber->y,
                                             ply_image_sequence_get_width (throbber->frame_sequence),
                                             ply_image_sequence_get_height (throbber->frame_sequence));
        }

        if (throbber->loop != NULL) {
                ply_event_loop_stop_watching_for_timeout (throbber->loop,
                                                          (ply_event_loop_timeout_handler_t)
                                                          on_timeout, throbber);
                throbber->loop = NULL;
        }
        throbber->display = NULL;
}

void
ply_throbber_stop (ply_throbber_t *throbber,
                   ply_trigger_t  *stop_trigger)
{
        if (throbber->is_stopped) {
                ply_trace ("throbber already stopped");
                if (stop_trigger != NULL) {
                        ply_trace ("pulling stop trigger right away");
                        ply_trigger_pull (stop_trigger, NULL);
                }
                return;
        }

        if (stop_trigger == NULL) {
                ply_throbber_stop_now (throbber, true);
                return;
        }

        throbber->stop_trigger = stop_trigger;
}

bool
ply_throbber_is_stopped (ply_throbber_t *throbber)
{
        return throbber->is_stopped;
}

void
ply_throbber_draw_area (ply_throbber_t     *throbber,
                        ply_pixel_buffer_t *buffer,
                        long                x,
                        long                y,
                        unsigned long       width,
                        unsigned long       height)
{
        if (throbber->is_stopped)
                return;

        ply_image_sequence_draw_area (throbber->frame_sequence,
                                      throbber->frame_number,
                                      buffer,
                                      throbber->x, throbber->y,
                                      width, height);
}

long
ply_throbber_get_width (ply_throbber_t *throbber)
{
        return throbber->width;
}

long
ply_throbber_get_height (ply_throbber_t *throbber)
{
        return throbber->height;
}

/* vim: set ts=4 sw=4 expandtab autoindent cindent cino={.5s,(0: */
