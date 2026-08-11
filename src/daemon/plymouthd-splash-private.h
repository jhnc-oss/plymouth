/* plymouthd-splash-private.h - internal splash construction
 *
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#ifndef PLYMOUTHD_SPLASH_PRIVATE_H
#define PLYMOUTHD_SPLASH_PRIVATE_H

#include <stdbool.h>

#include "ply-boot-splash.h"
#include "ply-private.h"

typedef struct _plymouthd_splash plymouthd_splash_t;

PLY_PRIVATE plymouthd_splash_t *plymouthd_splash_new (void);
PLY_PRIVATE void plymouthd_splash_free (plymouthd_splash_t *splash);
PLY_PRIVATE ply_boot_splash_t *plymouthd_splash_get (const plymouthd_splash_t *splash);
PLY_PRIVATE void plymouthd_splash_take (plymouthd_splash_t *splash,
                                        ply_boot_splash_t  *boot_splash);
PLY_PRIVATE void plymouthd_splash_clear (plymouthd_splash_t *splash);
PLY_PRIVATE bool plymouthd_splash_is_shown (const plymouthd_splash_t *splash);
PLY_PRIVATE void plymouthd_splash_set_shown (plymouthd_splash_t *splash,
                                             bool                is_shown);
PLY_PRIVATE bool plymouthd_splash_is_showing_details (const plymouthd_splash_t *splash);
PLY_PRIVATE void plymouthd_splash_set_showing_details (plymouthd_splash_t *splash,
                                                       bool                is_showing_details);
PLY_PRIVATE void plymouthd_splash_force_details (plymouthd_splash_t *splash);
PLY_PRIVATE void plymouthd_splash_force_default (plymouthd_splash_t *splash);
PLY_PRIVATE bool plymouthd_splash_should_show_default (const plymouthd_splash_t *splash);
PLY_PRIVATE ply_boot_splash_t *
plymouthd_load_splash (const char       *theme_path,
                       const char       *plugin_directory,
                       ply_buffer_t     *boot_buffer,
                       ply_event_loop_t *loop);

#endif /* PLYMOUTHD_SPLASH_PRIVATE_H */
