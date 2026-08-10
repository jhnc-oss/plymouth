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

#include "ply-boot-server.h"
#include "ply-boot-splash.h"
#include "ply-buffer.h"
#include "ply-device-manager.h"
#include "ply-event-loop.h"
#include "ply-terminal.h"
#include "plymouthd-interaction-private.h"
#include "plymouthd-logging-private.h"
#include "plymouthd-messages-private.h"
#include "plymouthd-progress-private.h"
#include "plymouthd-session-private.h"
#include "plymouthd-settings-private.h"
#include "plymouthd-transition-private.h"

struct _plymouthd
{
        ply_event_loop_t        *loop;
        ply_boot_server_t       *boot_server;
        ply_boot_splash_t       *boot_splash;
        ply_buffer_t            *boot_buffer;
        plymouthd_interaction_t *interaction;
        plymouthd_logging_t     *logging;
        plymouthd_messages_t    *messages;
        plymouthd_progress_t    *progress;
        plymouthd_session_t     *session;
        plymouthd_transition_t  *transition;
        ply_boot_splash_mode_t   mode;
        ply_terminal_t          *local_console_terminal;
        ply_device_manager_t    *device_manager;

        double                   start_time;
        plymouthd_settings_t     settings;

        uint32_t                 showing_details : 1;
        uint32_t                 should_be_attached : 1;
        uint32_t                 is_inactive : 1;
        uint32_t                 is_shown : 1;
        uint32_t                 should_force_details : 1;
        uint32_t                 should_force_default_splash : 1;
        const char              *default_tty;
};

#endif /* PLYMOUTHD_STATE_PRIVATE_H */
