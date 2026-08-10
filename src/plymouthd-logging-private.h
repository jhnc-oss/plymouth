/* plymouthd-logging-private.h - internal boot logging lifecycle
 *
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#ifndef PLYMOUTHD_LOGGING_PRIVATE_H
#define PLYMOUTHD_LOGGING_PRIVATE_H

#include <stdbool.h>

#include "ply-boot-splash-plugin.h"
#include "ply-private.h"
#include "ply-terminal-session.h"

typedef struct _plymouthd_logging plymouthd_logging_t;

PLY_PRIVATE plymouthd_logging_t *plymouthd_logging_new (ply_boot_splash_mode_t mode,
                                                        const char            *boot_log_file,
                                                        bool                   disabled);
PLY_PRIVATE void plymouthd_logging_free (plymouthd_logging_t *logging);
PLY_PRIVATE void plymouthd_logging_set_mode (plymouthd_logging_t   *logging,
                                             ply_boot_splash_mode_t mode);
PLY_PRIVATE bool plymouthd_logging_is_enabled (plymouthd_logging_t *logging);
PLY_PRIVATE bool plymouthd_logging_is_initialized (plymouthd_logging_t *logging);
PLY_PRIVATE const char *plymouthd_logging_get_log_file (plymouthd_logging_t *logging);
PLY_PRIVATE const char *plymouthd_logging_get_spool_file (plymouthd_logging_t *logging);
PLY_PRIVATE void plymouthd_logging_prepare (plymouthd_logging_t    *logging,
                                            ply_terminal_session_t *session);
PLY_PRIVATE void plymouthd_logging_system_initialized (plymouthd_logging_t    *logging,
                                                       ply_terminal_session_t *session);
PLY_PRIVATE void plymouthd_logging_record_error (plymouthd_logging_t *logging);

#endif /* PLYMOUTHD_LOGGING_PRIVATE_H */
