/* plymouthd-runtime.c - daemon runtime callbacks
 *
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#include "plymouthd-runtime-private.h"

#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "ply-boot-splash.h"
#include "ply-buffer.h"
#include "ply-logger.h"
#include "ply-trigger.h"
#include "plymouthd-control-private.h"
#include "plymouthd-logging-private.h"
#include "plymouthd-session-private.h"
#include "plymouthd-state-private.h"

typedef plymouthd_t state_t;

void
plymouthd_handle_session_output (state_t    *state,
                                 const char *output,
                                 size_t      size)
{
        ply_buffer_append_bytes (state->boot_buffer, output, size);
        if (state->boot_splash != NULL)
                ply_boot_splash_update_output (state->boot_splash,
                                               output, size);
}

void
plymouthd_handle_session_hangup (state_t *state)
{
        ply_trace ("got hang up on terminal session fd");
}

void
plymouthd_handle_kmsg (state_t        *state,
                       kmsg_message_t *kmsg_message)
{
        ply_buffer_append (state->boot_buffer, "%s\n", kmsg_message->message);

        if (state->boot_splash != NULL) {
                ply_boot_splash_update_output (state->boot_splash, kmsg_message->message, strlen (kmsg_message->message));
                ply_boot_splash_update_output (state->boot_splash, "\n", 1);
        }
}

bool
plymouthd_attach_session (state_t *state)
{
        bool should_be_redirected;

        should_be_redirected = plymouthd_logging_is_enabled (state->logging);

        if (!plymouthd_session_attach (state->session, should_be_redirected)) {
                ply_buffer_free (state->boot_buffer);
                state->boot_buffer = NULL;
                return false;
        }

        return true;
}

static void
start_plymouthd_fd_escrow (void)
{
        pid_t pid;

        pid = fork ();
        if (pid == 0) {
                const char *argv[] = { PLYMOUTH_DRM_ESCROW_DIRECTORY "/plymouthd-fd-escrow", NULL };

                execve (argv[0], (char * const *) argv, NULL);
                ply_trace ("could not launch fd escrow process: %m");
                _exit (1);
        }
}

void
plymouthd_handle_term_signal (state_t *state)
{
        bool retain_splash = false;

        ply_trace ("received SIGTERM");

        /*
         * On shutdown/reboot with pixel-displays active, start the plymouthd-fd-escrow
         * helper to hold on to the pixel-displays fds until the end.
         */
        if ((state->mode == PLY_BOOT_SPLASH_MODE_SHUTDOWN ||
             state->mode == PLY_BOOT_SPLASH_MODE_REBOOT) &&
            !state->is_inactive && state->boot_splash &&
            ply_boot_splash_uses_pixel_displays (state->boot_splash)) {
                start_plymouthd_fd_escrow ();
                retain_splash = true;
        }

        plymouthd_handle_quit (state,
                               retain_splash,
                               ply_trigger_new (NULL));
}
