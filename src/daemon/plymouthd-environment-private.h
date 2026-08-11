/* plymouthd-environment-private.h - daemon operating environment
 *
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#ifndef PLYMOUTHD_ENVIRONMENT_PRIVATE_H
#define PLYMOUTHD_ENVIRONMENT_PRIVATE_H

#include <stdbool.h>

#include "ply-private.h"

typedef struct _plymouthd plymouthd_t;

PLY_PRIVATE bool plymouthd_initialize_environment (plymouthd_t *daemon,
                                                   const char  *debug_path,
                                                   bool         capture_debug,
                                                   char        *pid_file);

#endif /* PLYMOUTHD_ENVIRONMENT_PRIVATE_H */
