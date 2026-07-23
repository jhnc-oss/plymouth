/* ply-peer-credentials.c - internal peer credential source
 *
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#include "ply-peer-credentials-private.h"

#include "ply-utils.h"

typedef enum
{
        PLY_PEER_CREDENTIALS_MODE_SYSTEM,
        PLY_PEER_CREDENTIALS_MODE_FIXED,
        PLY_PEER_CREDENTIALS_MODE_UNAVAILABLE,
} ply_peer_credentials_mode_t;

static ply_peer_credentials_mode_t mode;
static pid_t fixed_pid;
static uid_t fixed_uid;
static gid_t fixed_gid;

bool
ply_peer_credentials_read (int    fd,
                           pid_t *pid,
                           uid_t *uid,
                           gid_t *gid)
{
        if (mode == PLY_PEER_CREDENTIALS_MODE_UNAVAILABLE)
                return false;

        if (mode == PLY_PEER_CREDENTIALS_MODE_SYSTEM)
                return ply_get_credentials_from_fd (fd, pid, uid, gid);

        if (pid != NULL)
                *pid = fixed_pid;

        if (uid != NULL)
                *uid = fixed_uid;

        if (gid != NULL)
                *gid = fixed_gid;

        return true;
}

void
ply_peer_credentials_set (pid_t pid,
                          uid_t uid,
                          gid_t gid)
{
        fixed_pid = pid;
        fixed_uid = uid;
        fixed_gid = gid;
        mode = PLY_PEER_CREDENTIALS_MODE_FIXED;
}

void
ply_peer_credentials_set_unavailable (void)
{
        mode = PLY_PEER_CREDENTIALS_MODE_UNAVAILABLE;
}

void
ply_peer_credentials_use_system (void)
{
        mode = PLY_PEER_CREDENTIALS_MODE_SYSTEM;
}
