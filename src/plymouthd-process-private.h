/* plymouthd-process-private.h - internal process resource lifecycle
 *
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#ifndef PLYMOUTHD_PROCESS_PRIVATE_H
#define PLYMOUTHD_PROCESS_PRIVATE_H

#include <stdbool.h>

#include "ply-boot-splash-plugin.h"
#include "ply-private.h"

typedef struct _plymouthd_process plymouthd_process_t;

PLY_PRIVATE plymouthd_process_t *
plymouthd_process_new (ply_boot_splash_mode_t mode,
                       const char            *default_tty,
                       const char            *debug_path,
                       bool                   capture_debug,
                       char                  *pid_file);
PLY_PRIVATE void plymouthd_process_free (plymouthd_process_t *process);
PLY_PRIVATE void plymouthd_process_install_crash_handlers (plymouthd_process_t *process);
PLY_PRIVATE bool plymouthd_process_has_diagnostics (plymouthd_process_t *process);
PLY_PRIVATE void plymouthd_process_dump_diagnostics (plymouthd_process_t *process);
PLY_PRIVATE void plymouthd_process_write_pid_file (plymouthd_process_t *process);
PLY_PRIVATE void plymouthd_process_remove_pid_file (plymouthd_process_t *process);

#endif /* PLYMOUTHD_PROCESS_PRIVATE_H */
