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

#include "ply-boot-server.h"
#include "ply-event-loop.h"
#include "ply-private.h"

typedef struct _plymouthd plymouthd_t;

PLY_PRIVATE ply_boot_server_t *plymouthd_start_commands (ply_event_loop_t *loop,
                                                         plymouthd_t      *daemon);

#endif /* PLYMOUTHD_COMMANDS_PRIVATE_H */
