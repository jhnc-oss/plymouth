/* ply-secure-boot.c - internal Secure Boot state reader
 *
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#include "ply-secure-boot-private.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>

#include "ply-utils.h"

bool
ply_secure_boot_enabled_at_path (const char *path)
{
        uint8_t bytes[5];
        uint8_t extra_byte;
        ssize_t result;
        int fd;

        fd = open (path, O_RDONLY | O_CLOEXEC);
        if (fd < 0)
                return false;

        if (!ply_read (fd, bytes, sizeof(bytes))) {
                close (fd);
                return false;
        }

        do {
                result = read (fd, &extra_byte, sizeof(extra_byte));
        } while (result < 0 && errno == EINTR);

        close (fd);

        return result == 0 && bytes[4] == 0x01;
}
