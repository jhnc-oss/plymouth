/* plymouthd-splash-delay-private.h - internal splash delay timer
 *
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#ifndef PLYMOUTHD_SPLASH_DELAY_PRIVATE_H
#define PLYMOUTHD_SPLASH_DELAY_PRIVATE_H

#include <stdbool.h>

#include "ply-event-loop.h"
#include "ply-private.h"

typedef struct _plymouthd_splash_delay plymouthd_splash_delay_t;

PLY_PRIVATE plymouthd_splash_delay_t *
plymouthd_splash_delay_new (ply_event_loop_t                *loop,
                            double                           start_time,
                            double                           duration,
                            ply_event_loop_timeout_handler_t handler,
                            void                            *user_data);
PLY_PRIVATE void plymouthd_splash_delay_free (plymouthd_splash_delay_t *delay);
PLY_PRIVATE bool plymouthd_splash_delay_defer (plymouthd_splash_delay_t *delay,
                                               double                   *time_left);
PLY_PRIVATE void plymouthd_splash_delay_cancel (plymouthd_splash_delay_t *delay);

#endif /* PLYMOUTHD_SPLASH_DELAY_PRIVATE_H */
