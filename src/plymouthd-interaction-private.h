/* plymouthd-interaction-private.h - internal prompt and keystroke state
 *
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#ifndef PLYMOUTHD_INTERACTION_PRIVATE_H
#define PLYMOUTHD_INTERACTION_PRIVATE_H

#include <stdbool.h>
#include <stddef.h>

#include "ply-boot-server.h"
#include "ply-boot-splash.h"
#include "ply-private.h"
#include "ply-trigger.h"

typedef struct _plymouthd_interaction plymouthd_interaction_t;

PLY_PRIVATE plymouthd_interaction_t *plymouthd_interaction_new (void);
PLY_PRIVATE void plymouthd_interaction_free (plymouthd_interaction_t *interaction);

PLY_PRIVATE void plymouthd_interaction_queue_password (plymouthd_interaction_t *interaction,
                                                       ply_boot_splash_t       *splash,
                                                       const char              *prompt,
                                                       ply_trigger_t           *answer,
                                                       ply_boot_connection_t   *connection);
PLY_PRIVATE void plymouthd_interaction_queue_question (plymouthd_interaction_t *interaction,
                                                       ply_boot_splash_t       *splash,
                                                       const char              *prompt,
                                                       ply_trigger_t           *answer,
                                                       ply_boot_connection_t   *connection);
PLY_PRIVATE void plymouthd_interaction_watch_keystroke (plymouthd_interaction_t *interaction,
                                                        const char              *keys,
                                                        ply_trigger_t           *trigger,
                                                        ply_boot_connection_t   *connection);
PLY_PRIVATE void plymouthd_interaction_ignore_keystroke (plymouthd_interaction_t *interaction,
                                                         const char              *keys);
PLY_PRIVATE void plymouthd_interaction_cancel_connection (plymouthd_interaction_t *interaction,
                                                          ply_boot_splash_t       *splash,
                                                          ply_boot_connection_t   *connection);

PLY_PRIVATE bool plymouthd_validate_prompt_input (ply_boot_splash_t *splash,
                                                  const char        *entry_text,
                                                  const char        *add_text);
PLY_PRIVATE void plymouthd_interaction_update_display (plymouthd_interaction_t *interaction,
                                                       ply_boot_splash_t       *splash);
PLY_PRIVATE void plymouthd_interaction_handle_input (plymouthd_interaction_t *interaction,
                                                     ply_boot_splash_t       *splash,
                                                     const char              *keyboard_input,
                                                     size_t                   character_size);
PLY_PRIVATE void plymouthd_interaction_handle_backspace (plymouthd_interaction_t *interaction,
                                                         ply_boot_splash_t       *splash);
PLY_PRIVATE void plymouthd_interaction_handle_enter (plymouthd_interaction_t *interaction,
                                                     ply_boot_splash_t       *splash,
                                                     const char              *line);

#endif /* PLYMOUTHD_INTERACTION_PRIVATE_H */
