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

#include "ply-boot-splash.h"
#include "ply-private.h"

PLY_PRIVATE ply_boot_splash_t *
plymouthd_load_splash (const char       *theme_path,
                       const char       *plugin_directory,
                       ply_buffer_t     *boot_buffer,
                       ply_event_loop_t *loop,
                       ply_progress_t   *progress);

#endif /* PLYMOUTHD_SPLASH_PRIVATE_H */
