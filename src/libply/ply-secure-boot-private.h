/* ply-secure-boot-private.h - internal Secure Boot state reader
 *
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#ifndef PLY_SECURE_BOOT_PRIVATE_H
#define PLY_SECURE_BOOT_PRIVATE_H

#include <stdbool.h>

#include "ply-private.h"

PLY_PRIVATE bool ply_secure_boot_enabled_at_path (const char *path);

#endif /* PLY_SECURE_BOOT_PRIVATE_H */
