/* plymouthd-messages-private.h - internal splash message backlog
 *
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#ifndef PLYMOUTHD_MESSAGES_PRIVATE_H
#define PLYMOUTHD_MESSAGES_PRIVATE_H

#include "ply-boot-splash.h"
#include "ply-private.h"

typedef struct _plymouthd_messages plymouthd_messages_t;

PLY_PRIVATE plymouthd_messages_t *plymouthd_messages_new (void);
PLY_PRIVATE void plymouthd_messages_free (plymouthd_messages_t *messages);
PLY_PRIVATE void plymouthd_messages_display (plymouthd_messages_t *messages,
                                             ply_boot_splash_t    *splash,
                                             const char           *message);
PLY_PRIVATE void plymouthd_messages_hide (plymouthd_messages_t *messages,
                                          ply_boot_splash_t    *splash,
                                          const char           *message);
PLY_PRIVATE void plymouthd_messages_replay (plymouthd_messages_t *messages,
                                            ply_boot_splash_t    *splash);

#endif /* PLYMOUTHD_MESSAGES_PRIVATE_H */
