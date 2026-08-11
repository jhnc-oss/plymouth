/* plymouthd-progress-private.h - internal boot progress lifecycle
 *
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#ifndef PLYMOUTHD_PROGRESS_PRIVATE_H
#define PLYMOUTHD_PROGRESS_PRIVATE_H

#include "ply-boot-splash-plugin.h"
#include "ply-private.h"
#include "ply-progress.h"

typedef struct _plymouthd_progress plymouthd_progress_t;

PLY_PRIVATE plymouthd_progress_t *plymouthd_progress_new (ply_boot_splash_mode_t mode);
PLY_PRIVATE void plymouthd_progress_free (plymouthd_progress_t *progress);
PLY_PRIVATE void plymouthd_progress_set_mode (plymouthd_progress_t  *progress,
                                              ply_boot_splash_mode_t mode);
PLY_PRIVATE const char *plymouthd_progress_get_cache_file (plymouthd_progress_t *progress);
PLY_PRIVATE ply_progress_t *plymouthd_progress_get_core (plymouthd_progress_t *progress);
PLY_PRIVATE void plymouthd_progress_status_update (plymouthd_progress_t *progress,
                                                   const char           *status);
PLY_PRIVATE void plymouthd_progress_pause (plymouthd_progress_t *progress);
PLY_PRIVATE void plymouthd_progress_unpause (plymouthd_progress_t *progress);
PLY_PRIVATE void plymouthd_progress_load_cache (plymouthd_progress_t *progress);
PLY_PRIVATE void plymouthd_progress_save_cache (plymouthd_progress_t *progress);

#endif /* PLYMOUTHD_PROGRESS_PRIVATE_H */
