/* plymouthd-commands-private.h - internal daemon command adapter
 *
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#ifndef PLYMOUTHD_COMMANDS_PRIVATE_H
#define PLYMOUTHD_COMMANDS_PRIVATE_H

#include "ply-event-loop.h"
#include "ply-private.h"

typedef struct _plymouthd plymouthd_t;
typedef struct _plymouthd_commands plymouthd_commands_t;

PLY_PRIVATE plymouthd_commands_t *plymouthd_commands_new (ply_event_loop_t *loop,
                                                          plymouthd_t      *daemon);
PLY_PRIVATE void plymouthd_commands_free (plymouthd_commands_t *commands);

#endif /* PLYMOUTHD_COMMANDS_PRIVATE_H */
