/* ply-active-console-private.h - internal active console reader
 *
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#ifndef PLY_ACTIVE_CONSOLE_PRIVATE_H
#define PLY_ACTIVE_CONSOLE_PRIVATE_H

#include <stdbool.h>
#include <stddef.h>

#include "ply-private.h"

typedef struct
{
        char **names;
        size_t count;
} ply_active_console_list_t;

PLY_PRIVATE bool ply_active_console_list_load (const char                *path,
                                               ply_active_console_list_t *consoles);
PLY_PRIVATE void ply_active_console_list_clear (ply_active_console_list_t *consoles);

#endif /* PLY_ACTIVE_CONSOLE_PRIVATE_H */
