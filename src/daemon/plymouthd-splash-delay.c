/* plymouthd-splash-delay.c - internal splash delay timer
 *
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#include "plymouthd-splash-delay-private.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>

#include "ply-utils.h"

struct _plymouthd_splash_delay
{
        ply_event_loop_t                *loop;
        double                           start_time;
        double                           duration;
        ply_event_loop_timeout_handler_t handler;
        void                            *user_data;

        uint32_t                         is_scheduled : 1;
};

static void
on_timeout (plymouthd_splash_delay_t *delay,
            ply_event_loop_t         *loop)
{
        delay->is_scheduled = false;

        if (delay->handler != NULL)
                delay->handler (delay->user_data, loop);
}

plymouthd_splash_delay_t *
plymouthd_splash_delay_new (ply_event_loop_t                *loop,
                            double                           start_time,
                            double                           duration,
                            ply_event_loop_timeout_handler_t handler,
                            void                            *user_data)
{
        plymouthd_splash_delay_t *delay;

        delay = calloc (1, sizeof(plymouthd_splash_delay_t));
        delay->loop = loop;
        delay->start_time = start_time;
        delay->duration = duration;
        delay->handler = handler;
        delay->user_data = user_data;

        return delay;
}

void
plymouthd_splash_delay_free (plymouthd_splash_delay_t *delay)
{
        if (delay == NULL)
                return;

        plymouthd_splash_delay_cancel (delay);
        free (delay);
}

bool
plymouthd_splash_delay_defer (plymouthd_splash_delay_t *delay,
                              double                   *time_left)
{
        double running_time;
        double remaining_time;

        if (isnan (delay->duration))
                return false;

        running_time = ply_get_timestamp () - delay->start_time;
        remaining_time = delay->duration - running_time;
        if (remaining_time <= 0.0)
                return false;

        if (delay->is_scheduled) {
                ply_event_loop_stop_watching_for_timeout (
                        delay->loop,
                        (ply_event_loop_timeout_handler_t) on_timeout,
                        delay);
        }

        ply_event_loop_watch_for_timeout (
                delay->loop,
                remaining_time,
                (ply_event_loop_timeout_handler_t) on_timeout,
                delay);
        delay->is_scheduled = true;

        if (time_left != NULL)
                *time_left = remaining_time;

        return true;
}

void
plymouthd_splash_delay_cancel (plymouthd_splash_delay_t *delay)
{
        if (delay->is_scheduled) {
                ply_event_loop_stop_watching_for_timeout (
                        delay->loop,
                        (ply_event_loop_timeout_handler_t) on_timeout,
                        delay);
                delay->is_scheduled = false;
        }

        delay->duration = NAN;
}
