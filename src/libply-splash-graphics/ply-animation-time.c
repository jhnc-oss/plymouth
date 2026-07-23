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
        double delay;

        delay = frame_duration -
                (ply_clock_get_time () - callback_start_time);

        return MAX (delay, 0.005);
}

int
ply_animation_time_get_frame_number (double elapsed_time,
                                     double animation_duration,
                                     int    number_of_frames)
{
        double fraction;

        fraction = fmod (elapsed_time, animation_duration) /
                   animation_duration;

        return (int) (number_of_frames * fraction);
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
