/* plymouthd-private.h - internal daemon application
 *
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#ifndef PLYMOUTHD_PRIVATE_H
#define PLYMOUTHD_PRIVATE_H

#include "ply-private.h"
#include "plymouthd-options-private.h"

typedef struct _plymouthd plymouthd_t;

PLY_PRIVATE plymouthd_t *plymouthd_new (plymouthd_options_t *options,
                                        char                *program_name,
                                        int                 *exit_code);
PLY_PRIVATE int plymouthd_run (plymouthd_t *daemon);
PLY_PRIVATE void plymouthd_free (plymouthd_t *daemon);

#endif /* PLYMOUTHD_PRIVATE_H */
