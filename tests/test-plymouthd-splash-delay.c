/*
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#include "ply-test.h"

#include <math.h>

#include "ply-event-loop.h"
#include "ply-utils.h"
#include "plymouthd-splash-delay-private.h"

typedef struct
{
        bool elapsed;
} test_context_t;

static void
on_delay_elapsed (test_context_t   *context,
                  ply_event_loop_t *loop)
{
        context->elapsed = true;
        ply_event_loop_exit (loop, 0);
}

static bool
test_inactive_deadlines_do_not_defer (void)
{
        plymouthd_splash_delay_t *delay;
        ply_event_loop_t *loop;
        double now;

        loop = ply_event_loop_new ();
        now = ply_get_timestamp ();

        delay = plymouthd_splash_delay_new (loop, now, NAN, NULL, NULL);
        PLY_TEST_ASSERT (!plymouthd_splash_delay_defer (delay, NULL));
        plymouthd_splash_delay_free (delay);

        delay = plymouthd_splash_delay_new (loop,
                                            now - 1.0,
                                            0.1,
                                            NULL,
                                            NULL);
        PLY_TEST_ASSERT (!plymouthd_splash_delay_defer (delay, NULL));
        plymouthd_splash_delay_free (delay);

        plymouthd_splash_delay_free (NULL);
        ply_event_loop_free (loop);
        return true;
}

static bool
test_live_deadline_runs_once (void)
{
        plymouthd_splash_delay_t *delay;
        test_context_t context = { 0 };
        ply_event_loop_t *loop;
        double time_left;

        loop = ply_event_loop_new ();
        delay = plymouthd_splash_delay_new (
                loop,
                ply_get_timestamp (),
                0.001,
                (ply_event_loop_timeout_handler_t) on_delay_elapsed,
                &context);

        PLY_TEST_ASSERT (plymouthd_splash_delay_defer (delay, &time_left));
        PLY_TEST_ASSERT (time_left > 0.0);
        PLY_TEST_ASSERT (time_left <= 0.001);
        PLY_TEST_ASSERT (plymouthd_splash_delay_defer (delay, NULL));
        PLY_TEST_ASSERT (ply_event_loop_run (loop) == 0);
        PLY_TEST_ASSERT (context.elapsed);
        PLY_TEST_ASSERT (!plymouthd_splash_delay_defer (delay, NULL));

        plymouthd_splash_delay_free (delay);
        ply_event_loop_free (loop);
        return true;
}

static bool
test_cancel_disables_pending_deadline (void)
{
        plymouthd_splash_delay_t *delay;
        ply_event_loop_t *loop;

        loop = ply_event_loop_new ();
        delay = plymouthd_splash_delay_new (loop,
                                            ply_get_timestamp (),
                                            60.0,
                                            NULL,
                                            NULL);

        PLY_TEST_ASSERT (plymouthd_splash_delay_defer (delay, NULL));
        plymouthd_splash_delay_cancel (delay);
        PLY_TEST_ASSERT (!plymouthd_splash_delay_defer (delay, NULL));

        plymouthd_splash_delay_free (delay);
        ply_event_loop_free (loop);
        return true;
}

static const ply_test_case_t test_cases[] =
{
        PLY_TEST_CASE (test_inactive_deadlines_do_not_defer),
        PLY_TEST_CASE (test_live_deadline_runs_once),
        PLY_TEST_CASE (test_cancel_disables_pending_deadline),
};

PLY_TEST_MAIN (test_cases)
