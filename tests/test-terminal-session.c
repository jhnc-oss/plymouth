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

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "ply-event-loop.h"
#include "ply-terminal-session.h"

typedef struct
{
        ply_event_loop_t *loop;
        ply_terminal_session_t *session;
        uint8_t output[128];
        size_t output_size;
        int output_calls;
        int hangup_calls;
        bool output_overflowed;
        bool output_session_matched;
        bool hangup_session_matched;
        bool timed_out;
} session_context_t;

static int
open_terminal_slave (int master_fd)
{
        struct termios attributes;
        const char *slave_name;
        int slave_fd;

        slave_name = ptsname (master_fd);
        if (slave_name == NULL)
                return -1;

        slave_fd = open (slave_name, O_RDWR | O_NOCTTY | O_CLOEXEC);
        if (slave_fd < 0)
                return -1;

        if (tcgetattr (slave_fd, &attributes) < 0) {
                close (slave_fd);
                return -1;
        }

        cfmakeraw (&attributes);
        if (tcsetattr (slave_fd, TCSANOW, &attributes) < 0) {
                close (slave_fd);
                return -1;
        }

        return slave_fd;
}

static bool
open_terminal_pair (int *master_fd,
                    int *slave_fd)
{
        *master_fd = posix_openpt (O_RDWR | O_NOCTTY | O_CLOEXEC);
        if (*master_fd < 0)
                return false;

        if (unlockpt (*master_fd) < 0) {
                close (*master_fd);
                return false;
        }

        *slave_fd = open_terminal_slave (*master_fd);
        if (*slave_fd < 0) {
                close (*master_fd);
                return false;
        }

        return true;
}

static bool
read_file (const char *path,
           char       *contents,
           size_t      capacity)
{
        size_t offset = 0;
        int fd;

        fd = open (path, O_RDONLY | O_CLOEXEC);
        if (fd < 0)
                return false;

        while (offset + 1 < capacity) {
                ssize_t bytes_read;

                bytes_read = read (fd,
                                   contents + offset,
                                   capacity - offset - 1);
                if (bytes_read < 0) {
                        if (errno == EINTR)
                                continue;

                        close (fd);
                        return false;
                }

                if (bytes_read == 0)
                        break;

                offset += (size_t) bytes_read;
        }

        contents[offset] = '\0';
        close (fd);
        return true;
}

static void
on_output (void                   *user_data,
           const uint8_t          *output,
           size_t                  size,
           ply_terminal_session_t *session)
{
        session_context_t *context = user_data;

        context->output_calls++;
        context->output_session_matched = session == context->session;

        if (size > sizeof(context->output) - context->output_size) {
                context->output_overflowed = true;
        } else {
                memcpy (context->output + context->output_size, output, size);
                context->output_size += size;
        }

        ply_event_loop_exit (context->loop, 0);
}

static void
on_hangup (void                   *user_data,
           ply_terminal_session_t *session)
{
        session_context_t *context = user_data;

        context->hangup_calls++;
        context->hangup_session_matched = session == context->session;
        ply_event_loop_exit (context->loop, 0);
}

static void
on_watchdog (void             *user_data,
             ply_event_loop_t *loop)
{
        session_context_t *context = user_data;

        context->timed_out = true;
        ply_event_loop_exit (loop, 99);
}

static bool
test_external_terminal_forwards_output_and_log (void)
{
        static const char message[] = "terminal session output";
        char path[] = "/tmp/plymouth-terminal-session-test-XXXXXX";
        char log_contents[512];
        session_context_t context = { 0 };
        ply_event_loop_t *loop;
        ply_terminal_session_t *session;
        int master_fd;
        int slave_fd;
        int log_fd;

        PLY_TEST_ASSERT (open_terminal_pair (&master_fd, &slave_fd));
        log_fd = mkstemp (path);
        PLY_TEST_ASSERT (log_fd >= 0);
        close (log_fd);

        loop = ply_event_loop_new ();
        PLY_TEST_ASSERT (loop != NULL);
        session = ply_terminal_session_new (NULL);
        PLY_TEST_ASSERT (session != NULL);
        context.loop = loop;
        context.session = session;

        ply_terminal_session_attach_to_event_loop (session, loop);
        PLY_TEST_ASSERT (ply_terminal_session_open_log (session, path));
        PLY_TEST_ASSERT (ply_terminal_session_attach (
                                 session,
                                 PLY_TERMINAL_SESSION_FLAGS_NONE,
                                 on_output,
                                 on_hangup,
                                 master_fd,
                                 &context));
        PLY_TEST_ASSERT (ply_terminal_session_get_fd (session) == master_fd);
        ply_event_loop_watch_for_timeout (loop, 1.0, on_watchdog, &context);

        PLY_TEST_ASSERT (write (slave_fd, message, sizeof(message) - 1) ==
                         sizeof(message) - 1);
        PLY_TEST_ASSERT (ply_event_loop_run (loop) == 0);

        PLY_TEST_ASSERT (!context.timed_out);
        PLY_TEST_ASSERT (!context.output_overflowed);
        PLY_TEST_ASSERT (context.output_calls == 1);
        PLY_TEST_ASSERT (context.hangup_calls == 0);
        PLY_TEST_ASSERT (context.output_session_matched);
        PLY_TEST_ASSERT (context.output_size == sizeof(message) - 1);
        PLY_TEST_ASSERT (memcmp (context.output,
                                 message,
                                 sizeof(message) - 1) == 0);

        ply_terminal_session_close_log (session);
        PLY_TEST_ASSERT (read_file (path,
                                    log_contents,
                                    sizeof(log_contents)));
        PLY_TEST_ASSERT (strstr (log_contents, message) != NULL);

        ply_terminal_session_detach (session);
        ply_terminal_session_free (session);
        errno = 0;
        PLY_TEST_ASSERT (fcntl (master_fd, F_GETFD) == -1);
        PLY_TEST_ASSERT (errno == EBADF);

        close (slave_fd);
        ply_event_loop_free (loop);
        PLY_TEST_ASSERT (unlink (path) == 0);
        return true;
}

static bool
test_external_terminal_reports_hangup (void)
{
        session_context_t context = { 0 };
        ply_event_loop_t *loop;
        ply_terminal_session_t *session;
        int master_fd;
        int slave_fd;

        PLY_TEST_ASSERT (open_terminal_pair (&master_fd, &slave_fd));
        loop = ply_event_loop_new ();
        PLY_TEST_ASSERT (loop != NULL);
        session = ply_terminal_session_new (NULL);
        PLY_TEST_ASSERT (session != NULL);
        context.loop = loop;
        context.session = session;

        ply_terminal_session_attach_to_event_loop (session, loop);
        PLY_TEST_ASSERT (ply_terminal_session_attach (
                                 session,
                                 PLY_TERMINAL_SESSION_FLAGS_NONE,
                                 on_output,
                                 on_hangup,
                                 master_fd,
                                 &context));
        ply_event_loop_watch_for_timeout (loop, 1.0, on_watchdog, &context);
        close (slave_fd);

        PLY_TEST_ASSERT (ply_event_loop_run (loop) == 0);
        PLY_TEST_ASSERT (!context.timed_out);
        PLY_TEST_ASSERT (context.output_calls == 0);
        PLY_TEST_ASSERT (context.hangup_calls == 1);
        PLY_TEST_ASSERT (context.hangup_session_matched);
        PLY_TEST_ASSERT (ply_terminal_session_get_fd (session) == master_fd);

        ply_terminal_session_free (session);
        errno = 0;
        PLY_TEST_ASSERT (fcntl (master_fd, F_GETFD) == -1);
        PLY_TEST_ASSERT (errno == EBADF);

        ply_event_loop_free (loop);
        return true;
}

static bool
test_owned_terminal_closes_on_detach (void)
{
        ply_event_loop_t *loop;
        ply_terminal_session_t *session;
        int master_fd;
        int slave_fd;

        loop = ply_event_loop_new ();
        PLY_TEST_ASSERT (loop != NULL);
        session = ply_terminal_session_new (NULL);
        PLY_TEST_ASSERT (session != NULL);
        ply_terminal_session_attach_to_event_loop (session, loop);

        PLY_TEST_ASSERT (ply_terminal_session_attach (
                                 session,
                                 PLY_TERMINAL_SESSION_FLAGS_NONE,
                                 NULL,
                                 NULL,
                                 -1,
                                 NULL));
        master_fd = ply_terminal_session_get_fd (session);
        PLY_TEST_ASSERT (master_fd >= 0);
        slave_fd = open_terminal_slave (master_fd);
        PLY_TEST_ASSERT (slave_fd >= 0);

        ply_terminal_session_detach (session);
        PLY_TEST_ASSERT (ply_terminal_session_get_fd (session) == -1);
        errno = 0;
        PLY_TEST_ASSERT (fcntl (master_fd, F_GETFD) == -1);
        PLY_TEST_ASSERT (errno == EBADF);

        close (slave_fd);
        ply_terminal_session_free (session);
        ply_event_loop_free (loop);
        return true;
}

static bool
test_log_open_failure_preserves_errno (void)
{
        ply_terminal_session_t *session;

        session = ply_terminal_session_new (NULL);
        PLY_TEST_ASSERT (session != NULL);

        errno = EDOM;
        PLY_TEST_ASSERT (!ply_terminal_session_open_log (
                                 session,
                                 "/dev/null/plymouth-test-log"));
        PLY_TEST_ASSERT (errno == EDOM);

        ply_terminal_session_free (session);
        return true;
}

static const ply_test_case_t test_cases[] =
{
        PLY_TEST_CASE (test_external_terminal_forwards_output_and_log),
        PLY_TEST_CASE (test_external_terminal_reports_hangup),
        PLY_TEST_CASE (test_owned_terminal_closes_on_detach),
        PLY_TEST_CASE (test_log_open_failure_preserves_errno),
};

PLY_TEST_MAIN (test_cases)
