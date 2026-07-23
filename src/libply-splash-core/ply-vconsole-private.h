/* ply-vconsole-private.h - internal virtual console settings reader
 *
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#ifndef PLY_VCONSOLE_PRIVATE_H
#define PLY_VCONSOLE_PRIVATE_H

#include <stdbool.h>

#include "ply-private.h"

typedef struct
{
        char *keymap;
        char *xkb_layout;
        char *xkb_model;
        char *xkb_variant;
        char *xkb_options;
} ply_vconsole_settings_t;

PLY_PRIVATE bool ply_vconsole_settings_load (const char              *path,
                                             ply_vconsole_settings_t *settings);
PLY_PRIVATE void ply_vconsole_settings_clear (ply_vconsole_settings_t *settings);

#endif /* PLY_VCONSOLE_PRIVATE_H */
