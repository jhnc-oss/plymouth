/* ply-boot-client-private.h - internal boot client interfaces
 *
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#ifndef PLY_BOOT_CLIENT_PRIVATE_H
#define PLY_BOOT_CLIENT_PRIVATE_H

#include "ply-boot-client.h"

bool ply_boot_client_connect_to_fd (ply_boot_client_t                   *client,
                                    int                                  fd,
                                    ply_boot_client_disconnect_handler_t disconnect_handler,
                                    void                                *user_data);

#endif /* PLY_BOOT_CLIENT_PRIVATE_H */
