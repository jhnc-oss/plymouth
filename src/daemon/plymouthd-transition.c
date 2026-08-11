/* plymouthd-transition.c - internal daemon transition state
 *
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#include "plymouthd-transition-private.h"

#include <stdint.h>
#include <stdlib.h>

struct _plymouthd_transition
{
        ply_trigger_t *deactivate_trigger;
        ply_trigger_t *quit_trigger;

        uint32_t       retain_splash : 1;
        uint32_t       becoming_idle : 1;
        uint32_t       is_inactive : 1;
};

static bool
queue_trigger (ply_trigger_t **pending_trigger,
               ply_trigger_t  *trigger)
{
        if (*pending_trigger != NULL) {
                ply_trigger_add_handler (*pending_trigger,
                                         (ply_trigger_handler_t)
                                         ply_trigger_pull,
                                         trigger);
                return false;
        }

        *pending_trigger = trigger;
        return true;
}

static void
complete_trigger (ply_trigger_t **trigger)
{
        if (*trigger == NULL)
                return;

        ply_trigger_pull (*trigger, NULL);
        *trigger = NULL;
}

plymouthd_transition_t *
plymouthd_transition_new (void)
{
        return calloc (1, sizeof(plymouthd_transition_t));
}

void
plymouthd_transition_free (plymouthd_transition_t *transition)
{
        free (transition);
}

bool
plymouthd_transition_queue_deactivate (plymouthd_transition_t *transition,
                                       ply_trigger_t          *trigger)
{
        return queue_trigger (&transition->deactivate_trigger, trigger);
}

bool
plymouthd_transition_queue_quit (plymouthd_transition_t *transition,
                                 bool                    retain_splash,
                                 ply_trigger_t          *trigger)
{
        if (!queue_trigger (&transition->quit_trigger, trigger))
                return false;

        plymouthd_transition_set_retain_splash (transition, retain_splash);
        return true;
}

bool
plymouthd_transition_has_deactivate (plymouthd_transition_t *transition)
{
        return transition->deactivate_trigger != NULL;
}

bool
plymouthd_transition_has_quit (plymouthd_transition_t *transition)
{
        return transition->quit_trigger != NULL;
}

void
plymouthd_transition_set_retain_splash (plymouthd_transition_t *transition,
                                        bool                    retain_splash)
{
        transition->retain_splash = retain_splash;
}

bool
plymouthd_transition_should_retain_splash (plymouthd_transition_t *transition)
{
        return transition->retain_splash;
}

bool
plymouthd_transition_begin_idle (plymouthd_transition_t *transition)
{
        if (transition->becoming_idle)
                return false;

        transition->becoming_idle = true;
        return true;
}

void
plymouthd_transition_end_idle (plymouthd_transition_t *transition)
{
        transition->becoming_idle = false;
}

bool
plymouthd_transition_is_inactive (const plymouthd_transition_t *transition)
{
        return transition->is_inactive;
}

void
plymouthd_transition_activate (plymouthd_transition_t *transition)
{
        transition->is_inactive = false;
}

void
plymouthd_transition_complete_deactivate (plymouthd_transition_t *transition)
{
        transition->is_inactive = true;
        complete_trigger (&transition->deactivate_trigger);
}

void
plymouthd_transition_complete_all (plymouthd_transition_t *transition)
{
        complete_trigger (&transition->deactivate_trigger);
        complete_trigger (&transition->quit_trigger);
}
