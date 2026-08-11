/* plymouthd-diagnostics-private.h - internal debug capture lifecycle
 *
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#ifndef PLYMOUTHD_DIAGNOSTICS_PRIVATE_H
#define PLYMOUTHD_DIAGNOSTICS_PRIVATE_H

#include <stdbool.h>

#include "ply-boot-splash-plugin.h"
#include "ply-private.h"

typedef struct _plymouthd_diagnostics plymouthd_diagnostics_t;

PLY_PRIVATE plymouthd_diagnostics_t *
plymouthd_diagnostics_new (ply_boot_splash_mode_t mode,
                           const char            *default_tty,
                           const char            *configured_path,
                           bool                   capture_requested);
PLY_PRIVATE void plymouthd_diagnostics_free (plymouthd_diagnostics_t *diagnostics);
PLY_PRIVATE bool plymouthd_diagnostics_has_buffer (plymouthd_diagnostics_t *diagnostics);
PLY_PRIVATE const char *
plymouthd_get_default_diagnostics_path (ply_boot_splash_mode_t mode);
PLY_PRIVATE const char *
plymouthd_diagnostics_get_path (plymouthd_diagnostics_t *diagnostics);
PLY_PRIVATE int plymouthd_diagnostics_get_crash_fd (plymouthd_diagnostics_t *diagnostics);
PLY_PRIVATE void plymouthd_diagnostics_dump (plymouthd_diagnostics_t *diagnostics);

#endif /* PLYMOUTHD_DIAGNOSTICS_PRIVATE_H */
