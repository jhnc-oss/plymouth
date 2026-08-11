/* plymouthd-state-private.h - internal daemon runtime state
 *
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#ifndef PLYMOUTHD_STATE_PRIVATE_H
#define PLYMOUTHD_STATE_PRIVATE_H

#include <stdint.h>

#include "ply-boot-splash-plugin.h"
#include "ply-private.h"

typedef struct _ply_boot_splash ply_boot_splash_t;
typedef struct _ply_event_loop ply_event_loop_t;
typedef struct _plymouthd_commands plymouthd_commands_t;
typedef struct _plymouthd_devices plymouthd_devices_t;
typedef struct _plymouthd_interaction plymouthd_interaction_t;
typedef struct _plymouthd_logging plymouthd_logging_t;
typedef struct _plymouthd_messages plymouthd_messages_t;
typedef struct _plymouthd_output plymouthd_output_t;
typedef struct _plymouthd_process plymouthd_process_t;
typedef struct _plymouthd_progress plymouthd_progress_t;
typedef struct _plymouthd_session plymouthd_session_t;
typedef struct _plymouthd_settings plymouthd_settings_t;
typedef struct _plymouthd_transition plymouthd_transition_t;
typedef struct _plymouthd plymouthd_t;

struct _plymouthd
{
        ply_event_loop_t        *loop;
        ply_boot_splash_t       *boot_splash;
        plymouthd_commands_t    *commands;
        plymouthd_devices_t     *devices;
        plymouthd_interaction_t *interaction;
        plymouthd_logging_t     *logging;
        plymouthd_messages_t    *messages;
        plymouthd_output_t      *output;
        plymouthd_process_t     *process;
        plymouthd_progress_t    *progress;
        plymouthd_session_t     *session;
        plymouthd_transition_t  *transition;
        ply_boot_splash_mode_t   mode;

        double                   start_time;
        plymouthd_settings_t    *settings;

        uint32_t                 showing_details : 1;
        uint32_t                 should_be_attached : 1;
        uint32_t                 is_inactive : 1;
        uint32_t                 is_shown : 1;
        uint32_t                 should_force_details : 1;
        uint32_t                 should_force_default_splash : 1;
        const char              *default_tty;
};

#endif /* PLYMOUTHD_STATE_PRIVATE_H */
