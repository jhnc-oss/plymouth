/* ply-animation-time.c - internal animation timing decisions
 *
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#include "ply-animation-time-private.h"
#include "ply-utils.h"

#include <math.h>

double
ply_animation_time_get_delay (double frame_duration,
                              double callback_start_time)
{
        return ply_animation_time_get_delay_with_minimum (frame_duration,
                                                          callback_start_time,
                                                          0.005);
}

double
ply_animation_time_get_delay_with_minimum (double frame_duration,
                                           double callback_start_time,
                                           double minimum_delay)
{
        double delay;

        delay = frame_duration -
                (ply_clock_get_time () - callback_start_time);

        return MAX (delay, minimum_delay);
}

int
ply_animation_time_get_frame_number (double elapsed_time,
                                     double animation_duration,
                                     int    number_of_frames)
{
        int frame_number;

        ply_animation_time_get_frame_transition (elapsed_time,
                                                 animation_duration,
                                                 number_of_frames,
                                                 &frame_number,
                                                 NULL,
                                                 NULL);

        return frame_number;
}

void
ply_animation_time_get_frame_transition (double  elapsed_time,
                                         double  animation_duration,
                                         int     number_of_frames,
                                         int    *frame_number,
                                         int    *next_frame_number,
                                         double *fraction)
{
        double frame_position;
        int current_frame_number;

        frame_position = number_of_frames *
                         fmod (elapsed_time, animation_duration) /
                         animation_duration;
        current_frame_number = (int) frame_position;

        if (frame_number != NULL)
                *frame_number = current_frame_number;

        if (next_frame_number != NULL)
                *next_frame_number = (current_frame_number + 1) % number_of_frames;

        if (fraction != NULL)
                *fraction = frame_position - current_frame_number;
}

bool
ply_animation_time_needs_interpolation (double animation_duration,
                                        int    number_of_frames,
                                        double refresh_rate)
{
        return number_of_frames < animation_duration * refresh_rate;
}

double
ply_animation_time_get_transition_fraction (double transition_start_time,
                                            double transition_duration)
{
        double fraction;

        fraction = (ply_clock_get_time () - transition_start_time) /
                   transition_duration;

        return CLAMP (fraction, 0.0, 1.0);
}
