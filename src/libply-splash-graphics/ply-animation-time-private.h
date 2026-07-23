/* ply-animation-time-private.h - internal animation timing decisions
 *
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#ifndef PLY_ANIMATION_TIME_PRIVATE_H
#define PLY_ANIMATION_TIME_PRIVATE_H

#include "ply-clock-private.h"

PLY_PRIVATE double ply_animation_time_get_delay (double frame_duration,
                                                 double callback_start_time);
PLY_PRIVATE int ply_animation_time_get_frame_number (double elapsed_time,
                                                     double animation_duration,
                                                     int    number_of_frames);
PLY_PRIVATE double ply_animation_time_get_transition_fraction (double transition_start_time,
                                                               double transition_duration);

#endif /* PLY_ANIMATION_TIME_PRIVATE_H */
