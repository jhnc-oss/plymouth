/* ply-peer-credentials-private.h - internal peer credential source
 *
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#ifndef PLY_PEER_CREDENTIALS_PRIVATE_H
#define PLY_PEER_CREDENTIALS_PRIVATE_H

#include <stdbool.h>
#include <sys/types.h>

#include "ply-private.h"

PLY_PRIVATE bool ply_peer_credentials_read (int    fd,
                                            pid_t *pid,
                                            uid_t *uid,
                                            gid_t *gid);
PLY_PRIVATE void ply_peer_credentials_set (pid_t pid,
                                           uid_t uid,
                                           gid_t gid);
PLY_PRIVATE void ply_peer_credentials_set_unavailable (void);
PLY_PRIVATE void ply_peer_credentials_use_system (void);

#endif /* PLY_PEER_CREDENTIALS_PRIVATE_H */
