/* plymouthd-transition-private.h - internal daemon transition state
 *
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#ifndef PLYMOUTHD_TRANSITION_PRIVATE_H
#define PLYMOUTHD_TRANSITION_PRIVATE_H

#include <stdbool.h>

#include "ply-private.h"
#include "ply-trigger.h"

typedef struct _plymouthd_transition plymouthd_transition_t;

PLY_PRIVATE plymouthd_transition_t *plymouthd_transition_new (void);
PLY_PRIVATE void plymouthd_transition_free (plymouthd_transition_t *transition);
PLY_PRIVATE bool plymouthd_transition_queue_deactivate (plymouthd_transition_t *transition,
                                                        ply_trigger_t          *trigger);
PLY_PRIVATE bool plymouthd_transition_queue_quit (plymouthd_transition_t *transition,
                                                  bool                    retain_splash,
                                                  ply_trigger_t          *trigger);
PLY_PRIVATE bool plymouthd_transition_has_deactivate (plymouthd_transition_t *transition);
PLY_PRIVATE bool plymouthd_transition_has_quit (plymouthd_transition_t *transition);
PLY_PRIVATE void plymouthd_transition_set_retain_splash (plymouthd_transition_t *transition,
                                                         bool                    retain_splash);
PLY_PRIVATE bool plymouthd_transition_should_retain_splash (plymouthd_transition_t *transition);
PLY_PRIVATE bool plymouthd_transition_begin_idle (plymouthd_transition_t *transition);
PLY_PRIVATE void plymouthd_transition_end_idle (plymouthd_transition_t *transition);
PLY_PRIVATE void plymouthd_transition_complete_deactivate (plymouthd_transition_t *transition);
PLY_PRIVATE void plymouthd_transition_complete_all (plymouthd_transition_t *transition);

#endif /* PLYMOUTHD_TRANSITION_PRIVATE_H */
