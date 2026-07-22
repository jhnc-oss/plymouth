/*
 * Copyright (C) 2026 Red Hat, Inc.
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
 */

#include "ply-test.h"

#include <math.h>

#include "ply-animation-time-private.h"

static bool
test_callback_delay_accounts_for_work_time (void)
{
        double delay;

        ply_clock_set_time (12.01);
        delay = ply_animation_time_get_delay (0.04, 12.0);

        PLY_TEST_ASSERT (fabs (delay - 0.03) < 0.000001);
        return true;
}

static bool
test_callback_delay_has_minimum (void)
{
        double delay;

        ply_clock_set_time (12.1);
        delay = ply_animation_time_get_delay (0.04, 12.0);

        PLY_TEST_ASSERT (delay == 0.005);
        return true;
}

static bool
test_frame_number_advances_and_wraps (void)
{
        PLY_TEST_ASSERT (ply_animation_time_get_frame_number (0.0,
                                                              2.0,
                                                              4) == 0);
        PLY_TEST_ASSERT (ply_animation_time_get_frame_number (0.5,
                                                              2.0,
                                                              4) == 1);
        PLY_TEST_ASSERT (ply_animation_time_get_frame_number (1.5,
                                                              2.0,
                                                              4) == 3);
        PLY_TEST_ASSERT (ply_animation_time_get_frame_number (2.0,
                                                              2.0,
                                                              4) == 0);
        PLY_TEST_ASSERT (ply_animation_time_get_frame_number (4.5,
                                                              2.0,
                                                              4) == 1);
        return true;
}

static bool
test_transition_fraction_is_clamped (void)
{
        double fraction;

        ply_clock_set_time (95.0);
        fraction = ply_animation_time_get_transition_fraction (100.0, 10.0);
        PLY_TEST_ASSERT (fraction == 0.0);

        ply_clock_set_time (102.5);
        fraction = ply_animation_time_get_transition_fraction (100.0, 10.0);
        PLY_TEST_ASSERT (fraction == 0.25);

        ply_clock_set_time (110.0);
        fraction = ply_animation_time_get_transition_fraction (100.0, 10.0);
        PLY_TEST_ASSERT (fraction == 1.0);

        ply_clock_set_time (125.0);
        fraction = ply_animation_time_get_transition_fraction (100.0, 10.0);
        PLY_TEST_ASSERT (fraction == 1.0);
        return true;
}

static const ply_test_case_t test_cases[] =
{
        PLY_TEST_CASE (test_callback_delay_accounts_for_work_time),
        PLY_TEST_CASE (test_callback_delay_has_minimum),
        PLY_TEST_CASE (test_frame_number_advances_and_wraps),
        PLY_TEST_CASE (test_transition_fraction_is_clamped),
};

PLY_TEST_MAIN (test_cases)
