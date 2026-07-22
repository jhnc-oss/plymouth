/* ply-clock-private.h - internal monotonic clock control
 *
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#ifndef PLY_CLOCK_PRIVATE_H
#define PLY_CLOCK_PRIVATE_H

#include "ply-private.h"

PLY_PRIVATE double ply_clock_get_time (void);
PLY_PRIVATE void ply_clock_set_time (double time);
PLY_PRIVATE void ply_clock_use_system_time (void);

#endif /* PLY_CLOCK_PRIVATE_H */
