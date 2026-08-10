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
#include "plymouthd-transition-private.h"

bool
plymouthd_initialize_session (plymouthd_t *daemon,
                              bool         should_attach)
{
        daemon->boot_buffer = ply_buffer_new ();
        daemon->transition = plymouthd_transition_new ();
        daemon->session = plymouthd_session_new (
                daemon->loop,
                (plymouthd_session_output_handler_t)
                plymouthd_handle_session_output,
                (plymouthd_session_hangup_handler_t)
                plymouthd_handle_session_hangup,
                (plymouthd_session_kmsg_handler_t)
                plymouthd_handle_kmsg,
                daemon);

        if (!should_attach)
                return true;

        daemon->should_be_attached = true;
        return plymouthd_attach_session (daemon);
}

void
plymouthd_handle_session_output (plymouthd_t *daemon,
                                 const char  *output,
                                 size_t       size)
{
        ply_buffer_append_bytes (daemon->boot_buffer, output, size);
        if (daemon->boot_splash != NULL)
                ply_boot_splash_update_output (daemon->boot_splash,
                                               output, size);
}

void
plymouthd_handle_session_hangup (plymouthd_t *daemon)
{
        ply_trace ("got hang up on terminal session fd");
}

void
plymouthd_handle_kmsg (plymouthd_t    *daemon,
                       kmsg_message_t *kmsg_message)
{
        ply_buffer_append (daemon->boot_buffer, "%s\n", kmsg_message->message);

        if (daemon->boot_splash != NULL) {
                ply_boot_splash_update_output (daemon->boot_splash, kmsg_message->message, strlen (kmsg_message->message));
                ply_boot_splash_update_output (daemon->boot_splash, "\n", 1);
        }
}

bool
plymouthd_attach_session (plymouthd_t *daemon)
{
        bool should_be_redirected;

        should_be_redirected = plymouthd_logging_is_enabled (daemon->logging);

        if (!plymouthd_session_attach (daemon->session, should_be_redirected)) {
                ply_buffer_free (daemon->boot_buffer);
                daemon->boot_buffer = NULL;
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
plymouthd_handle_term_signal (plymouthd_t *daemon)
{
        bool retain_splash = false;

        ply_trace ("received SIGTERM");

        /*
         * On shutdown/reboot with pixel-displays active, start the plymouthd-fd-escrow
         * helper to hold on to the pixel-displays fds until the end.
         */
        if ((daemon->mode == PLY_BOOT_SPLASH_MODE_SHUTDOWN ||
             daemon->mode == PLY_BOOT_SPLASH_MODE_REBOOT) &&
            !daemon->is_inactive && daemon->boot_splash &&
            ply_boot_splash_uses_pixel_displays (daemon->boot_splash)) {
                start_plymouthd_fd_escrow ();
                retain_splash = true;
        }

        plymouthd_handle_quit (daemon,
                               retain_splash,
                               ply_trigger_new (NULL));
}
