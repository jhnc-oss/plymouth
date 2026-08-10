/* plymouthd-progress.c - internal boot progress lifecycle
 *
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#include "plymouthd-progress-private.h"

#include <stdlib.h>

#include "ply-logger.h"
#include "ply-progress.h"
#include "ply-utils.h"

#define BOOT_DURATION_FILE     PLYMOUTH_TIME_DIRECTORY "/boot-duration"
#define SHUTDOWN_DURATION_FILE PLYMOUTH_TIME_DIRECTORY "/shutdown-duration"

struct _plymouthd_progress
{
        ply_progress_t        *core;
        ply_boot_splash_mode_t mode;
};

plymouthd_progress_t *
plymouthd_progress_new (ply_boot_splash_mode_t mode)
{
        plymouthd_progress_t *progress;

        progress = calloc (1, sizeof(plymouthd_progress_t));
        progress->core = ply_progress_new ();
        progress->mode = mode;

        return progress;
}

void
plymouthd_progress_free (plymouthd_progress_t *progress)
{
        if (progress == NULL)
                return;

        ply_progress_free (progress->core);
        free (progress);
}

void
plymouthd_progress_set_mode (plymouthd_progress_t  *progress,
                             ply_boot_splash_mode_t mode)
{
        progress->mode = mode;
}

const char *
plymouthd_progress_get_cache_file (plymouthd_progress_t *progress)
{
        const char *filename;

        switch (progress->mode) {
        case PLY_BOOT_SPLASH_MODE_BOOT_UP:
                filename = BOOT_DURATION_FILE;
                break;
        case PLY_BOOT_SPLASH_MODE_SHUTDOWN:
        case PLY_BOOT_SPLASH_MODE_REBOOT:
                filename = SHUTDOWN_DURATION_FILE;
                break;
        case PLY_BOOT_SPLASH_MODE_UPDATES:
        case PLY_BOOT_SPLASH_MODE_SYSTEM_UPGRADE:
        case PLY_BOOT_SPLASH_MODE_FIRMWARE_UPGRADE:
        case PLY_BOOT_SPLASH_MODE_SYSTEM_RESET:
                filename = NULL;
                break;
        case PLY_BOOT_SPLASH_MODE_INVALID:
        default:
                ply_error ("Unhandled case in %s line %d\n", __FILE__, __LINE__);
                abort ();
                break;
        }

        ply_trace ("returning cache file '%s'", filename);
        return filename;
}

void
plymouthd_progress_attach_to_splash (plymouthd_progress_t *progress,
                                     ply_boot_splash_t    *splash)
{
        ply_boot_splash_attach_progress (splash, progress->core);
}

void
plymouthd_progress_status_update (plymouthd_progress_t *progress,
                                  const char           *status)
{
        ply_progress_status_update (progress->core, status);
}

void
plymouthd_progress_pause (plymouthd_progress_t *progress)
{
        ply_progress_pause (progress->core);
}

void
plymouthd_progress_unpause (plymouthd_progress_t *progress)
{
        ply_progress_unpause (progress->core);
}

void
plymouthd_progress_load_cache (plymouthd_progress_t *progress)
{
        ply_progress_load_cache (progress->core,
                                 plymouthd_progress_get_cache_file (progress));
}

void
plymouthd_progress_save_cache (plymouthd_progress_t *progress)
{
        ply_create_directory (PLYMOUTH_TIME_DIRECTORY);
        ply_progress_save_cache (progress->core,
                                 plymouthd_progress_get_cache_file (progress));
}
