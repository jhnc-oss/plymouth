/* plymouthd-session-private.h - internal console capture session
 *
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#ifndef PLYMOUTHD_SESSION_PRIVATE_H
#define PLYMOUTHD_SESSION_PRIVATE_H

#include <stdbool.h>
#include <stddef.h>

#include "ply-event-loop.h"
#include "ply-kmsg-reader.h"
#include "ply-private.h"

typedef struct _plymouthd_session plymouthd_session_t;

typedef void (*plymouthd_session_output_handler_t) (void       *user_data,
                                                    const char *output,
                                                    size_t      size);
typedef void (*plymouthd_session_hangup_handler_t) (void *user_data);
typedef void (*plymouthd_session_kmsg_handler_t) (void           *user_data,
                                                  kmsg_message_t *message);

PLY_PRIVATE plymouthd_session_t *
plymouthd_session_new (ply_event_loop_t                  *loop,
                       plymouthd_session_output_handler_t output_handler,
                       plymouthd_session_hangup_handler_t hangup_handler,
                       plymouthd_session_kmsg_handler_t   kmsg_handler,
                       void                              *user_data);
PLY_PRIVATE void plymouthd_session_free (plymouthd_session_t *session);
PLY_PRIVATE bool plymouthd_session_attach (plymouthd_session_t *session,
                                           bool                 redirect_console);
PLY_PRIVATE void plymouthd_session_detach (plymouthd_session_t *session);
PLY_PRIVATE void plymouthd_session_set_should_attach (plymouthd_session_t *session,
                                                      bool                 should_attach);
PLY_PRIVATE bool plymouthd_session_should_attach (plymouthd_session_t *session);
PLY_PRIVATE bool plymouthd_session_is_attached (plymouthd_session_t *session);
PLY_PRIVATE bool plymouthd_session_is_redirected (plymouthd_session_t *session);
PLY_PRIVATE bool plymouthd_session_has_terminal (plymouthd_session_t *session);
PLY_PRIVATE void plymouthd_session_request_details (plymouthd_session_t *session);
PLY_PRIVATE bool plymouthd_session_open_log (plymouthd_session_t *session,
                                             const char          *path);
PLY_PRIVATE void plymouthd_session_close_log (plymouthd_session_t *session);

#endif /* PLYMOUTHD_SESSION_PRIVATE_H */
