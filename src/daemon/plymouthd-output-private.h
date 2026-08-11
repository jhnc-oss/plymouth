/* plymouthd-output-private.h - internal captured boot output
 *
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#ifndef PLYMOUTHD_OUTPUT_PRIVATE_H
#define PLYMOUTHD_OUTPUT_PRIVATE_H

#include <stddef.h>

#include "ply-private.h"

typedef struct _ply_boot_splash ply_boot_splash_t;
typedef struct _ply_buffer ply_buffer_t;
typedef struct _plymouthd_output plymouthd_output_t;

PLY_PRIVATE plymouthd_output_t *plymouthd_output_new (void);
PLY_PRIVATE void plymouthd_output_free (plymouthd_output_t *output);
PLY_PRIVATE ply_buffer_t *plymouthd_output_get_buffer (plymouthd_output_t *output);
PLY_PRIVATE void plymouthd_output_append (plymouthd_output_t *output,
                                          ply_boot_splash_t  *splash,
                                          const char         *bytes,
                                          size_t              size);

#endif /* PLYMOUTHD_OUTPUT_PRIVATE_H */
