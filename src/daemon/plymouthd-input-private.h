/* plymouthd-input-private.h - internal keyboard input routing
 *
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#ifndef PLYMOUTHD_INPUT_PRIVATE_H
#define PLYMOUTHD_INPUT_PRIVATE_H

#include <stdbool.h>

#include "ply-private.h"

typedef struct _ply_keyboard ply_keyboard_t;
typedef struct _plymouthd plymouthd_t;

PLY_PRIVATE void plymouthd_handle_escape (plymouthd_t *daemon,
                                          bool         has_vt_console);
PLY_PRIVATE void plymouthd_handle_keyboard_added (plymouthd_t    *daemon,
                                                  ply_keyboard_t *keyboard);
PLY_PRIVATE void plymouthd_handle_keyboard_removed (plymouthd_t    *daemon,
                                                    ply_keyboard_t *keyboard);

#endif /* PLYMOUTHD_INPUT_PRIVATE_H */
