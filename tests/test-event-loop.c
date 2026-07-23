/*
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include "ply-test.h"

#include <signal.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "ply-event-loop.h"

typedef struct
{
        ply_event_loop_t *loop;
        ply_fd_watch_t   *watch;
        int               calls;
        int               disconnected_calls;
        int               source_value;
        int               read_result;
        char              byte;
        bool              timed_out;
} fd_context_t;

typedef struct
{
        ply_event_loop_t *loop;
        int               order[4];
        int               count;
        bool              canceled_timeout_ran;
        bool              timed_out;
} timeout_context_t;

typedef struct
{
        timeout_context_t *context;
        int                value;
        bool               exits_loop;
} ordered_timeout_t;

typedef struct
{
        int               calls;
        int               exit_code;
        ply_event_loop_t *loop;
} exit_context_t;

static volatile sig_atomic_t sentinel_signal_count;

static void
on_watchdog (void             *user_data,
             ply_event_loop_t *loop)
{
        fd_context_t *context = user_data;

        context->timed_out = true;
        ply_event_loop_exit (loop, 99);
}

static void
on_readable (void *user_data,
             int   source_fd)
{
        fd_context_t *context = user_data;

        context->calls++;
        context->source_value = source_fd;
        context->read_result = (int) read (source_fd,
                                           &context->byte,
                                           sizeof(context->byte));
        ply_event_loop_exit (context->loop, 23);
}

static bool
test_readable_fd_dispatches_and_preserves_exit_code (void)
{
        fd_context_t context = { 0 };
        ply_event_loop_t *loop;
        int pipe_fds[2];

        PLY_TEST_ASSERT (pipe (pipe_fds) == 0);
        PLY_TEST_ASSERT (write (pipe_fds[1], "x", 1) == 1);

        loop = ply_event_loop_new ();
        PLY_TEST_ASSERT (loop != NULL);
        context.loop = loop;
        ply_event_loop_watch_fd (loop,
                                 pipe_fds[0],
                                 PLY_EVENT_LOOP_FD_STATUS_HAS_DATA,
                                 on_readable,
                                 NULL,
                                 &context);
        ply_event_loop_watch_for_timeout (loop, 1.0, on_watchdog, &context);

        PLY_TEST_ASSERT (ply_event_loop_run (loop) == 23);
        PLY_TEST_ASSERT (!context.timed_out);
        PLY_TEST_ASSERT (context.calls == 1);
        PLY_TEST_ASSERT (context.source_value == pipe_fds[0]);
        PLY_TEST_ASSERT (context.read_result == 1);
        PLY_TEST_ASSERT (context.byte == 'x');

        ply_event_loop_free (loop);
        close (pipe_fds[0]);
        close (pipe_fds[1]);
        return true;
}

static void
on_writable (void *user_data,
             int   source_fd)
{
        fd_context_t *context = user_data;

        context->calls++;
        context->source_value = source_fd;
        ply_event_loop_stop_watching_fd (context->loop, context->watch);
        context->watch = NULL;
        ply_event_loop_exit (context->loop, 0);
}

static bool
test_writable_fd_can_stop_its_own_watch (void)
{
        fd_context_t context = { 0 };
        ply_event_loop_t *loop;
        int pipe_fds[2];

        PLY_TEST_ASSERT (pipe (pipe_fds) == 0);
        loop = ply_event_loop_new ();
        PLY_TEST_ASSERT (loop != NULL);
        context.loop = loop;
        context.watch = ply_event_loop_watch_fd (
                loop,
                pipe_fds[1],
                PLY_EVENT_LOOP_FD_STATUS_CAN_TAKE_DATA,
                on_writable,
                NULL,
                &context);
        ply_event_loop_watch_for_timeout (loop, 1.0, on_watchdog, &context);

        PLY_TEST_ASSERT (ply_event_loop_run (loop) == 0);
        PLY_TEST_ASSERT (!context.timed_out);
        PLY_TEST_ASSERT (context.calls == 1);
        PLY_TEST_ASSERT (context.watch == NULL);
        PLY_TEST_ASSERT (context.source_value == pipe_fds[1]);

        ply_event_loop_free (loop);
        close (pipe_fds[0]);
        close (pipe_fds[1]);
        return true;
}

static void
on_disconnected (void *user_data,
                 int   source_fd)
{
        fd_context_t *context = user_data;

        context->disconnected_calls++;
        context->source_value = source_fd;
        ply_event_loop_exit (context->loop, 0);
}

static bool
test_closed_peer_dispatches_disconnect (void)
{
        fd_context_t context = { 0 };
        ply_event_loop_t *loop;
        int socket_fds[2];

        PLY_TEST_ASSERT (socketpair (AF_UNIX,
                                     SOCK_STREAM | SOCK_CLOEXEC,
                                     0,
                                     socket_fds) == 0);
        loop = ply_event_loop_new ();
        PLY_TEST_ASSERT (loop != NULL);
        context.loop = loop;
        ply_event_loop_watch_fd (loop,
                                 socket_fds[0],
                                 PLY_EVENT_LOOP_FD_STATUS_HAS_DATA,
                                 on_readable,
                                 on_disconnected,
                                 &context);
        ply_event_loop_watch_for_timeout (loop, 1.0, on_watchdog, &context);
        close (socket_fds[1]);

        PLY_TEST_ASSERT (ply_event_loop_run (loop) == 0);
        PLY_TEST_ASSERT (!context.timed_out);
        PLY_TEST_ASSERT (context.calls == 0);
        PLY_TEST_ASSERT (context.disconnected_calls == 1);
        PLY_TEST_ASSERT (context.source_value == socket_fds[0]);

        ply_event_loop_free (loop);
        close (socket_fds[0]);
        return true;
}

static void
on_ordered_timeout (void             *user_data,
                    ply_event_loop_t *loop)
{
        ordered_timeout_t *timeout = user_data;
        timeout_context_t *context = timeout->context;

        context->order[context->count++] = timeout->value;
        if (timeout->exits_loop)
                ply_event_loop_exit (loop, 0);
}

static void
on_canceled_timeout (void             *user_data,
                     ply_event_loop_t *loop)
{
        timeout_context_t *context = user_data;

        context->canceled_timeout_ran = true;
        ply_event_loop_exit (loop, 2);
}

static void
on_timeout_watchdog (void             *user_data,
                     ply_event_loop_t *loop)
{
        timeout_context_t *context = user_data;

        context->timed_out = true;
        ply_event_loop_exit (loop, 99);
}

static bool
test_timeouts_run_in_order_and_can_be_canceled (void)
{
        timeout_context_t context = { 0 };
        ordered_timeout_t first = {
                .context = &context,
                .value   = 1,
        };
        ordered_timeout_t second = {
                .context    = &context,
                .value      = 2,
                .exits_loop = true,
        };
        ply_event_loop_t *loop;

        loop = ply_event_loop_new ();
        PLY_TEST_ASSERT (loop != NULL);
        context.loop = loop;

        ply_event_loop_watch_for_timeout (loop, 0.005, on_canceled_timeout, &context);
        ply_event_loop_stop_watching_for_timeout (loop,
                                                  on_canceled_timeout,
                                                  &context);
        ply_event_loop_watch_for_timeout (loop, 0.01, on_ordered_timeout, &first);
        ply_event_loop_watch_for_timeout (loop, 0.02, on_ordered_timeout, &second);
        ply_event_loop_watch_for_timeout (loop, 1.0, on_timeout_watchdog, &context);

        PLY_TEST_ASSERT (ply_event_loop_run (loop) == 0);
        PLY_TEST_ASSERT (!context.timed_out);
        PLY_TEST_ASSERT (!context.canceled_timeout_ran);
        PLY_TEST_ASSERT (context.count == 2);
        PLY_TEST_ASSERT (context.order[0] == 1);
        PLY_TEST_ASSERT (context.order[1] == 2);

        ply_event_loop_free (loop);
        return true;
}

static void
on_loop_exit (void             *user_data,
              int               exit_code,
              ply_event_loop_t *loop)
{
        exit_context_t *context = user_data;

        context->calls++;
        context->exit_code = exit_code;
        context->loop = loop;
}

static void
exit_from_timeout (void             *user_data,
                   ply_event_loop_t *loop)
{
        int exit_code = *(int *) user_data;

        ply_event_loop_exit (loop, exit_code);
}

static bool
test_exit_watch_runs_and_removed_watch_stays_idle (void)
{
        exit_context_t kept = { 0 };
        exit_context_t removed = { 0 };
        ply_event_loop_t *loop;
        int exit_code = 37;

        loop = ply_event_loop_new ();
        PLY_TEST_ASSERT (loop != NULL);
        ply_event_loop_watch_for_exit (loop, on_loop_exit, &kept);
        ply_event_loop_watch_for_exit (loop, on_loop_exit, &removed);
        ply_event_loop_stop_watching_for_exit (loop, on_loop_exit, &removed);
        ply_event_loop_watch_for_timeout (loop,
                                          0.005,
                                          exit_from_timeout,
                                          &exit_code);

        PLY_TEST_ASSERT (ply_event_loop_run (loop) == exit_code);
        PLY_TEST_ASSERT (kept.calls == 1);
        PLY_TEST_ASSERT (kept.exit_code == exit_code);
        PLY_TEST_ASSERT (kept.loop == loop);
        PLY_TEST_ASSERT (removed.calls == 0);

        ply_event_loop_free (loop);
        return true;
}

static void
on_signal (void *user_data,
           int   signal_number)
{
        fd_context_t *context = user_data;

        context->calls++;
        context->source_value = signal_number;
        ply_event_loop_exit (context->loop, 0);
}

static void
on_sentinel_signal (int signal_number)
{
        if (signal_number == SIGUSR1)
                sentinel_signal_count++;
}

static bool
test_signal_dispatches_and_restores_previous_handler (void)
{
        struct sigaction action;
        struct sigaction old_action;
        fd_context_t context = { 0 };
        ply_event_loop_t *loop;

        memset (&action, 0, sizeof(action));
        action.sa_handler = on_sentinel_signal;
        sigemptyset (&action.sa_mask);
        PLY_TEST_ASSERT (sigaction (SIGUSR1, &action, &old_action) == 0);
        sentinel_signal_count = 0;

        loop = ply_event_loop_new ();
        PLY_TEST_ASSERT (loop != NULL);
        context.loop = loop;
        ply_event_loop_watch_signal (loop, SIGUSR1, on_signal, &context);
        ply_event_loop_watch_for_timeout (loop, 1.0, on_watchdog, &context);
        PLY_TEST_ASSERT (raise (SIGUSR1) == 0);

        PLY_TEST_ASSERT (ply_event_loop_run (loop) == 0);
        PLY_TEST_ASSERT (!context.timed_out);
        PLY_TEST_ASSERT (context.calls == 1);
        PLY_TEST_ASSERT (context.source_value == SIGUSR1);
        PLY_TEST_ASSERT (sentinel_signal_count == 0);

        ply_event_loop_free (loop);
        PLY_TEST_ASSERT (raise (SIGUSR1) == 0);
        PLY_TEST_ASSERT (sentinel_signal_count == 1);
        PLY_TEST_ASSERT (sigaction (SIGUSR1, &old_action, NULL) == 0);
        return true;
}

static bool
test_stopped_signal_watch_restores_previous_handler (void)
{
        struct sigaction action;
        struct sigaction old_action;
        fd_context_t context = { 0 };
        ply_event_loop_t *loop;

        memset (&action, 0, sizeof(action));
        action.sa_handler = on_sentinel_signal;
        sigemptyset (&action.sa_mask);
        PLY_TEST_ASSERT (sigaction (SIGUSR1, &action, &old_action) == 0);
        sentinel_signal_count = 0;

        loop = ply_event_loop_new ();
        PLY_TEST_ASSERT (loop != NULL);
        ply_event_loop_watch_signal (loop, SIGUSR1, on_signal, &context);
        ply_event_loop_stop_watching_signal (loop, SIGUSR1);

        PLY_TEST_ASSERT (raise (SIGUSR1) == 0);
        PLY_TEST_ASSERT (sentinel_signal_count == 1);

        ply_event_loop_free (loop);
        PLY_TEST_ASSERT (sigaction (SIGUSR1, &old_action, NULL) == 0);
        return true;
}

static void
on_pending_readable (void *user_data,
                     int   source_fd)
{
        fd_context_t *context = user_data;

        context->calls++;
        context->read_result = (int) read (source_fd,
                                           &context->byte,
                                           sizeof(context->byte));
        ply_event_loop_stop_watching_fd (context->loop, context->watch);
        context->watch = NULL;
}

static bool
test_pending_event_processing_handles_ready_fd (void)
{
        fd_context_t context = { 0 };
        ply_event_loop_t *loop;
        int pipe_fds[2];

        PLY_TEST_ASSERT (pipe (pipe_fds) == 0);
        PLY_TEST_ASSERT (write (pipe_fds[1], "p", 1) == 1);
        loop = ply_event_loop_new ();
        PLY_TEST_ASSERT (loop != NULL);
        context.loop = loop;
        context.watch = ply_event_loop_watch_fd (
                loop,
                pipe_fds[0],
                PLY_EVENT_LOOP_FD_STATUS_HAS_DATA,
                on_pending_readable,
                NULL,
                &context);

        ply_event_loop_process_pending_events (loop);

        PLY_TEST_ASSERT (context.calls == 1);
        PLY_TEST_ASSERT (context.read_result == 1);
        PLY_TEST_ASSERT (context.byte == 'p');
        PLY_TEST_ASSERT (context.watch == NULL);

        ply_event_loop_free (loop);
        close (pipe_fds[0]);
        close (pipe_fds[1]);
        return true;
}

static void
unused_timeout (void             *user_data,
                ply_event_loop_t *loop)
{
}

static bool
test_free_releases_pending_fd_and_timeout (void)
{
        ply_event_loop_t *loop;
        int pipe_fds[2];

        PLY_TEST_ASSERT (pipe (pipe_fds) == 0);
        loop = ply_event_loop_new ();
        PLY_TEST_ASSERT (loop != NULL);
        ply_event_loop_watch_fd (loop,
                                 pipe_fds[0],
                                 PLY_EVENT_LOOP_FD_STATUS_HAS_DATA,
                                 NULL,
                                 NULL,
                                 NULL);
        ply_event_loop_watch_for_timeout (loop, 60.0, unused_timeout, NULL);

        ply_event_loop_free (loop);
        close (pipe_fds[0]);
        close (pipe_fds[1]);
        return true;
}

static const ply_test_case_t test_cases[] =
{
        PLY_TEST_CASE (test_readable_fd_dispatches_and_preserves_exit_code),
        PLY_TEST_CASE (test_writable_fd_can_stop_its_own_watch),
        PLY_TEST_CASE (test_closed_peer_dispatches_disconnect),
        PLY_TEST_CASE (test_timeouts_run_in_order_and_can_be_canceled),
        PLY_TEST_CASE (test_exit_watch_runs_and_removed_watch_stays_idle),
        PLY_TEST_CASE (test_signal_dispatches_and_restores_previous_handler),
        PLY_TEST_CASE (test_stopped_signal_watch_restores_previous_handler),
        PLY_TEST_CASE (test_pending_event_processing_handles_ready_fd),
        PLY_TEST_CASE (test_free_releases_pending_fd_and_timeout),
};

PLY_TEST_MAIN (test_cases)
