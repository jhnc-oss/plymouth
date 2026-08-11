/* plymouthd-control-private.h - daemon command and transition control
 *
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#ifndef PLYMOUTHD_CONTROL_PRIVATE_H
#define PLYMOUTHD_CONTROL_PRIVATE_H

#include <stdbool.h>

#include "ply-private.h"
#include "ply-trigger.h"

typedef struct _plymouthd plymouthd_t;

PLY_PRIVATE void plymouthd_handle_show_splash (plymouthd_t *daemon);
PLY_PRIVATE void plymouthd_handle_hide_splash (plymouthd_t *daemon);
PLY_PRIVATE void plymouthd_handle_deactivate (plymouthd_t   *daemon,
                                              ply_trigger_t *trigger);
PLY_PRIVATE void plymouthd_handle_reactivate (plymouthd_t *daemon);
PLY_PRIVATE void plymouthd_handle_quit (plymouthd_t   *daemon,
                                        bool           retain_splash,
                                        ply_trigger_t *trigger);
#endif /* PLYMOUTHD_CONTROL_PRIVATE_H */
