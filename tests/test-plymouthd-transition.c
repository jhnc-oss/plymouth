/*
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#include "ply-test.h"

#include "ply-trigger.h"
#include "plymouthd-transition-private.h"

static void
count_pull (int           *pull_count,
            const void    *data,
            ply_trigger_t *trigger)
{
        (*pull_count)++;
}

static bool
test_duplicate_deactivate_requests_complete_together (void)
{
        plymouthd_transition_t *transition;
        ply_trigger_t *first;
        ply_trigger_t *second;
        int first_pull_count = 0;
        int second_pull_count = 0;

        transition = plymouthd_transition_new ();
        first = ply_trigger_new (NULL);
        second = ply_trigger_new (NULL);
        ply_trigger_add_handler (first,
                                 (ply_trigger_handler_t) count_pull,
                                 &first_pull_count);
        ply_trigger_add_handler (second,
                                 (ply_trigger_handler_t) count_pull,
                                 &second_pull_count);

        PLY_TEST_ASSERT (plymouthd_transition_queue_deactivate (transition,
                                                                first));
        PLY_TEST_ASSERT (!plymouthd_transition_queue_deactivate (transition,
                                                                 second));
        PLY_TEST_ASSERT (plymouthd_transition_has_deactivate (transition));

        plymouthd_transition_complete_deactivate (transition);
        PLY_TEST_ASSERT (!plymouthd_transition_has_deactivate (transition));
        PLY_TEST_ASSERT (first_pull_count == 1);
        PLY_TEST_ASSERT (second_pull_count == 1);

        ply_trigger_free (first);
        ply_trigger_free (second);
        plymouthd_transition_free (transition);
        return true;
}

static bool
test_first_quit_request_controls_retention (void)
{
        plymouthd_transition_t *transition;
        ply_trigger_t *first;
        ply_trigger_t *second;
        int first_pull_count = 0;
        int second_pull_count = 0;

        transition = plymouthd_transition_new ();
        first = ply_trigger_new (NULL);
        second = ply_trigger_new (NULL);
        ply_trigger_add_handler (first,
                                 (ply_trigger_handler_t) count_pull,
                                 &first_pull_count);
        ply_trigger_add_handler (second,
                                 (ply_trigger_handler_t) count_pull,
                                 &second_pull_count);

        PLY_TEST_ASSERT (plymouthd_transition_queue_quit (transition,
                                                          true,
                                                          first));
        PLY_TEST_ASSERT (!plymouthd_transition_queue_quit (transition,
                                                           false,
                                                           second));
        PLY_TEST_ASSERT (plymouthd_transition_has_quit (transition));
        PLY_TEST_ASSERT (plymouthd_transition_should_retain_splash (transition));

        plymouthd_transition_complete_all (transition);
        PLY_TEST_ASSERT (!plymouthd_transition_has_quit (transition));
        PLY_TEST_ASSERT (first_pull_count == 1);
        PLY_TEST_ASSERT (second_pull_count == 1);

        ply_trigger_free (first);
        ply_trigger_free (second);
        plymouthd_transition_free (transition);
        return true;
}

static bool
test_idle_transition_only_begins_once (void)
{
        plymouthd_transition_t *transition;

        transition = plymouthd_transition_new ();

        PLY_TEST_ASSERT (plymouthd_transition_begin_idle (transition));
        PLY_TEST_ASSERT (!plymouthd_transition_begin_idle (transition));
        plymouthd_transition_end_idle (transition);
        PLY_TEST_ASSERT (plymouthd_transition_begin_idle (transition));

        plymouthd_transition_free (transition);
        return true;
}

static const ply_test_case_t test_cases[] =
{
        PLY_TEST_CASE (test_duplicate_deactivate_requests_complete_together),
        PLY_TEST_CASE (test_first_quit_request_controls_retention),
        PLY_TEST_CASE (test_idle_transition_only_begins_once),
};

PLY_TEST_MAIN (test_cases)
