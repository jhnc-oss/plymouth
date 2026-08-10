/* plymouthd-session.c - internal console capture session
 *
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#include "plymouthd-session-private.h"

#include <stdint.h>
#include <stdlib.h>

#include "ply-logger.h"
#include "ply-terminal-session.h"

struct _plymouthd_session
{
        ply_event_loop_t                  *loop;
        ply_terminal_session_t            *terminal_session;
        ply_kmsg_reader_t                 *kmsg_reader;
        plymouthd_session_output_handler_t output_handler;
        plymouthd_session_hangup_handler_t hangup_handler;
        plymouthd_session_kmsg_handler_t   kmsg_handler;
        void                              *user_data;

        uint32_t                           attached : 1;
        uint32_t                           redirected : 1;
};

static void
on_terminal_output (plymouthd_session_t    *session,
                    const uint8_t          *output,
                    size_t                  size,
                    ply_terminal_session_t *terminal_session)
{
        if (session->output_handler != NULL)
                session->output_handler (session->user_data,
                                         (const char *) output,
                                         size);
}

static void
on_terminal_hangup (plymouthd_session_t    *session,
                    ply_terminal_session_t *terminal_session)
{
        if (session->hangup_handler != NULL)
                session->hangup_handler (session->user_data);
}

static void
on_kmsg (plymouthd_session_t *session,
         kmsg_message_t      *message)
{
        if (session->kmsg_handler != NULL)
                session->kmsg_handler (session->user_data, message);
}

plymouthd_session_t *
plymouthd_session_new (ply_event_loop_t                  *loop,
                       plymouthd_session_output_handler_t output_handler,
                       plymouthd_session_hangup_handler_t hangup_handler,
                       plymouthd_session_kmsg_handler_t   kmsg_handler,
                       void                              *user_data)
{
        plymouthd_session_t *session;

        session = calloc (1, sizeof(plymouthd_session_t));
        session->loop = loop;
        session->output_handler = output_handler;
        session->hangup_handler = hangup_handler;
        session->kmsg_handler = kmsg_handler;
        session->user_data = user_data;

        return session;
}

void
plymouthd_session_free (plymouthd_session_t *session)
{
        if (session == NULL)
                return;

        if (session->kmsg_reader != NULL) {
                ply_kmsg_reader_stop (session->kmsg_reader);
                ply_kmsg_reader_free (session->kmsg_reader);
        }

        ply_terminal_session_free (session->terminal_session);
        free (session);
}

bool
plymouthd_session_attach (plymouthd_session_t *session,
                          bool                 redirect_console)
{
        ply_terminal_session_flags_t flags;

        flags = PLY_TERMINAL_SESSION_FLAGS_NONE;
        if (redirect_console)
                flags |= PLY_TERMINAL_SESSION_FLAGS_REDIRECT_CONSOLE;

        if (session->terminal_session == NULL) {
                ply_trace ("creating new terminal session");
                session->terminal_session = ply_terminal_session_new (NULL);
                ply_terminal_session_attach_to_event_loop (session->terminal_session,
                                                           session->loop);
        } else {
                ply_trace ("session already created");
        }

        if (!ply_terminal_session_attach (
                    session->terminal_session,
                    flags,
                    (ply_terminal_session_output_handler_t) on_terminal_output,
                    redirect_console ?
                    (ply_terminal_session_hangup_handler_t) on_terminal_hangup :
                    NULL,
                    -1,
                    session)) {
                ply_save_errno ();
                ply_terminal_session_free (session->terminal_session);
                session->terminal_session = NULL;
                ply_restore_errno ();

                session->redirected = false;
                session->attached = false;
                return false;
        }

        if (session->kmsg_reader == NULL) {
                ply_trace ("Creating new kmsg reader");
                session->kmsg_reader = ply_kmsg_reader_new ();
                ply_kmsg_reader_watch_for_messages (
                        session->kmsg_reader,
                        (ply_kmsg_reader_message_handler_t) on_kmsg,
                        session);
        }

        ply_kmsg_reader_start (session->kmsg_reader);
        session->redirected = redirect_console;
        session->attached = true;

        return true;
}

void
plymouthd_session_detach (plymouthd_session_t *session)
{
        if (session->terminal_session == NULL || !session->attached)
                return;

        ply_trace ("stopping kmsg reader");
        ply_kmsg_reader_stop (session->kmsg_reader);

        ply_trace ("detaching from terminal session");
        ply_terminal_session_detach (session->terminal_session);
        session->redirected = false;
        session->attached = false;
}

bool
plymouthd_session_is_attached (plymouthd_session_t *session)
{
        return session->attached;
}

bool
plymouthd_session_is_redirected (plymouthd_session_t *session)
{
        return session->redirected;
}

bool
plymouthd_session_has_terminal (plymouthd_session_t *session)
{
        return session->terminal_session != NULL;
}

bool
plymouthd_session_open_log (plymouthd_session_t *session,
                            const char          *path)
{
        if (session->terminal_session == NULL)
                return false;

        return ply_terminal_session_open_log (session->terminal_session, path);
}

void
plymouthd_session_close_log (plymouthd_session_t *session)
{
        if (session->terminal_session == NULL)
                return;

        ply_terminal_session_close_log (session->terminal_session);
}
