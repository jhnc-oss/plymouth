/*
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#include "ply-test.h"

#include "ply-event-loop.h"
#include "plymouthd-session-private.h"

static bool
test_new_session_starts_detached (void)
{
        plymouthd_session_t *session;

        session = plymouthd_session_new (ply_event_loop_get_default (),
                                         NULL,
                                         NULL,
                                         NULL,
                                         NULL);

        PLY_TEST_ASSERT (session != NULL);
        PLY_TEST_ASSERT (!plymouthd_session_is_attached (session));
        PLY_TEST_ASSERT (!plymouthd_session_is_redirected (session));
        PLY_TEST_ASSERT (!plymouthd_session_has_terminal (session));
        PLY_TEST_ASSERT (!plymouthd_session_open_log (session, "/dev/null"));
        plymouthd_session_close_log (session);

        plymouthd_session_free (session);
        return true;
}

static bool
test_detaching_pristine_session_is_idempotent (void)
{
        plymouthd_session_t *session;

        session = plymouthd_session_new (ply_event_loop_get_default (),
                                         NULL,
                                         NULL,
                                         NULL,
                                         NULL);

        plymouthd_session_detach (session);
        plymouthd_session_detach (session);

        PLY_TEST_ASSERT (!plymouthd_session_is_attached (session));
        PLY_TEST_ASSERT (!plymouthd_session_is_redirected (session));

        plymouthd_session_free (session);
        plymouthd_session_free (NULL);
        return true;
}

static const ply_test_case_t test_cases[] =
{
        PLY_TEST_CASE (test_new_session_starts_detached),
        PLY_TEST_CASE (test_detaching_pristine_session_is_idempotent),
};

PLY_TEST_MAIN (test_cases)
