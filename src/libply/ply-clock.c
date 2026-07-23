/* ply-clock.c - internal monotonic clock control
 *
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#include "ply-clock-private.h"
#include "ply-utils.h"

#include <stdbool.h>

static bool time_is_overridden;
static double overridden_time;

double
ply_clock_get_time (void)
{
        if (time_is_overridden)
                return overridden_time;

        return ply_get_timestamp ();
}

void
ply_clock_set_time (double time)
{
        overridden_time = time;
        time_is_overridden = true;
}

void
ply_clock_use_system_time (void)
{
        time_is_overridden = false;
}
