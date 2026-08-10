/* plymouthd-devices-private.h - internal device coordination
 *
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#ifndef PLYMOUTHD_DEVICES_PRIVATE_H
#define PLYMOUTHD_DEVICES_PRIVATE_H

#include <stdbool.h>

#include "ply-private.h"

typedef struct _plymouthd plymouthd_t;

PLY_PRIVATE void plymouthd_initialize_devices (plymouthd_t *daemon,
                                               bool         should_ignore_serial_consoles);

#endif /* PLYMOUTHD_DEVICES_PRIVATE_H */
