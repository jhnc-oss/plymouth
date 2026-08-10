/* plymouthd-state.c - internal daemon runtime state owner
 *
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#include "plymouthd-state-private.h"

#include <stdlib.h>

#include "ply-event-loop.h"
#include "ply-utils.h"
#include "plymouthd-messages-private.h"
#include "plymouthd-process-private.h"
#include "plymouthd-settings-private.h"

plymouthd_t *
plymouthd_new_state (ply_boot_splash_mode_t mode)
{
        plymouthd_t *state;

        state = calloc (1, sizeof(plymouthd_t));
        state->start_time = ply_get_timestamp ();
        state->loop = ply_event_loop_get_default ();
        state->mode = mode;
        plymouthd_settings_init (&state->settings);

        return state;
}

void
plymouthd_free_state (plymouthd_t *state)
{
        if (state == NULL)
                return;

        plymouthd_messages_free (state->messages);
        plymouthd_settings_free (&state->settings);
        plymouthd_process_free (state->process);

        free (state);
}
