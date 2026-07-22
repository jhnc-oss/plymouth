/* ply-boot-server-private.h - internal boot server interfaces
 *
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#ifndef PLY_BOOT_SERVER_PRIVATE_H
#define PLY_BOOT_SERVER_PRIVATE_H

#include "ply-boot-server.h"

bool ply_boot_server_attach_connection_to_event_loop (ply_boot_server_t *server,
                                                      ply_event_loop_t  *loop,
                                                      int                fd);

#endif /* PLY_BOOT_SERVER_PRIVATE_H */
