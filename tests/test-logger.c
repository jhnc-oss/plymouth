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

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ply-logger.h"

typedef struct
{
        ply_logger_t *logger;
        int calls;
        bool received_logger;
} filter_context_t;

static bool
read_exactly (int     fd,
              void   *bytes,
              size_t  size)
{
        uint8_t *cursor = bytes;
        size_t bytes_read = 0;

        while (bytes_read < size) {
                ssize_t result;

                result = read (fd, cursor + bytes_read, size - bytes_read);
                if (result < 0) {
                        if (errno == EINTR)
                                continue;

                        return false;
                }

                if (result == 0)
                        return false;

                bytes_read += (size_t) result;
        }

        return true;
}

static bool
fd_has_data (int fd)
{
        struct pollfd poll_fd = {
                .fd = fd,
                .events = POLLIN,
        };

        return poll (&poll_fd, 1, 0) == 1 &&
               (poll_fd.revents & POLLIN) != 0;
}

static bool
test_new_logger_has_deferred_defaults (void)
{
        ply_logger_t *logger;

        logger = ply_logger_new ();

        PLY_TEST_ASSERT (logger != NULL);
        PLY_TEST_ASSERT (ply_logger_is_logging (logger));
        PLY_TEST_ASSERT (ply_logger_get_output_fd (logger) == -1);
        PLY_TEST_ASSERT (ply_logger_get_flush_policy (logger) ==
                         PLY_LOGGER_FLUSH_POLICY_WHEN_ASKED);

        ply_logger_free (logger);
        return true;
}

static bool
test_deferred_output_waits_for_flush (void)
{
        static const char message[] = "deferred output";
        char output[sizeof(message) - 1];
        ply_logger_t *logger;
        int pipe_fds[2];

        PLY_TEST_ASSERT (pipe (pipe_fds) == 0);
        logger = ply_logger_new ();
        ply_logger_set_output_fd (logger, pipe_fds[1]);

        ply_logger_inject (logger, "%s", message);
        PLY_TEST_ASSERT (!fd_has_data (pipe_fds[0]));
        PLY_TEST_ASSERT (ply_logger_flush (logger));
        PLY_TEST_ASSERT (read_exactly (pipe_fds[0], output, sizeof(output)));
        PLY_TEST_ASSERT (memcmp (output, message, sizeof(output)) == 0);

        ply_logger_free (logger);
        close (pipe_fds[0]);
        return true;
}

static bool
test_immediate_policy_flushes_each_injection (void)
{
        static const char message[] = "immediate output";
        char output[sizeof(message) - 1];
        ply_logger_t *logger;
        int pipe_fds[2];

        PLY_TEST_ASSERT (pipe (pipe_fds) == 0);
        logger = ply_logger_new ();
        ply_logger_set_output_fd (logger, pipe_fds[1]);
        ply_logger_set_flush_policy (logger,
                                     PLY_LOGGER_FLUSH_POLICY_EVERY_TIME);

        ply_logger_inject_bytes (logger, message, sizeof(message) - 1);

        PLY_TEST_ASSERT (fd_has_data (pipe_fds[0]));
        PLY_TEST_ASSERT (read_exactly (pipe_fds[0], output, sizeof(output)));
        PLY_TEST_ASSERT (memcmp (output, message, sizeof(output)) == 0);

        ply_logger_free (logger);
        close (pipe_fds[0]);
        return true;
}

static bool
test_disabled_logger_discards_injections (void)
{
        static const char kept[] = "kept";
        char output[sizeof(kept) - 1];
        ply_logger_t *logger;
        int pipe_fds[2];

        PLY_TEST_ASSERT (pipe (pipe_fds) == 0);
        logger = ply_logger_new ();
        ply_logger_set_output_fd (logger, pipe_fds[1]);

        ply_logger_toggle_logging (logger);
        PLY_TEST_ASSERT (!ply_logger_is_logging (logger));
        ply_logger_inject (logger, "discarded");
        ply_logger_toggle_logging (logger);
        ply_logger_inject_bytes (logger, kept, sizeof(kept) - 1);

        PLY_TEST_ASSERT (ply_logger_flush (logger));
        PLY_TEST_ASSERT (read_exactly (pipe_fds[0], output, sizeof(output)));
        PLY_TEST_ASSERT (memcmp (output, kept, sizeof(output)) == 0);
        PLY_TEST_ASSERT (!fd_has_data (pipe_fds[0]));

        ply_logger_free (logger);
        close (pipe_fds[0]);
        return true;
}

static void
uppercase_filter (void         *user_data,
                  const void   *in_bytes,
                  size_t        in_size,
                  void        **out_bytes,
                  size_t       *out_size,
                  ply_logger_t *logger)
{
        filter_context_t *context = user_data;
        const uint8_t *input = in_bytes;
        uint8_t *output;

        context->calls++;
        context->received_logger = logger == context->logger;

        output = malloc (in_size);
        if (output == NULL)
                return;

        for (size_t i = 0; i < in_size; i++)
                output[i] = (uint8_t) toupper (input[i]);

        *out_bytes = output;
        *out_size = in_size;
}

static bool
test_filter_transforms_injected_bytes (void)
{
        static const char message[] = "Mixed Case";
        static const char expected[] = "MIXED CASE";
        char output[sizeof(expected) - 1];
        filter_context_t context = { 0 };
        ply_logger_t *logger;
        int pipe_fds[2];

        PLY_TEST_ASSERT (pipe (pipe_fds) == 0);
        logger = ply_logger_new ();
        context.logger = logger;
        ply_logger_set_output_fd (logger, pipe_fds[1]);
        ply_logger_add_filter (logger, uppercase_filter, &context);

        ply_logger_inject_bytes (logger, message, sizeof(message) - 1);

        PLY_TEST_ASSERT (ply_logger_flush (logger));
        PLY_TEST_ASSERT (read_exactly (pipe_fds[0], output, sizeof(output)));
        PLY_TEST_ASSERT (memcmp (output, expected, sizeof(output)) == 0);
        PLY_TEST_ASSERT (context.calls == 1);
        PLY_TEST_ASSERT (context.received_logger);

        ply_logger_free (logger);
        close (pipe_fds[0]);
        return true;
}

static bool
test_invalid_format_does_not_write_through_percent_n (void)
{
        static const char expected[] =
                "[couldn't write a log entry: log format string invalid]\n";
        char output[sizeof(expected) - 1];
        ply_logger_t *logger;
        int pipe_fds[2];
        int written = -1;

        PLY_TEST_ASSERT (pipe (pipe_fds) == 0);
        logger = ply_logger_new ();
        ply_logger_set_output_fd (logger, pipe_fds[1]);

        ply_logger_inject_with_non_literal_format_string (logger,
                                                          "%n",
                                                          &written);

        PLY_TEST_ASSERT (written == -1);
        PLY_TEST_ASSERT (read_exactly (pipe_fds[0], output, sizeof(output)));
        PLY_TEST_ASSERT (memcmp (output, expected, sizeof(output)) == 0);

        ply_logger_free (logger);
        close (pipe_fds[0]);
        return true;
}

static bool
test_full_buffer_retains_recent_injections (void)
{
        const size_t injection_size = 4095;
        const size_t retained_injections = 8;
        const size_t output_size = injection_size * retained_injections;
        char path[] = "/tmp/plymouth-logger-test-XXXXXX";
        uint8_t *injection;
        uint8_t *output;
        ply_logger_t *logger;
        int fd;

        fd = mkstemp (path);
        PLY_TEST_ASSERT (fd >= 0);
        PLY_TEST_ASSERT (unlink (path) == 0);

        injection = malloc (injection_size);
        output = malloc (output_size);
        PLY_TEST_ASSERT (injection != NULL);
        PLY_TEST_ASSERT (output != NULL);

        logger = ply_logger_new ();
        ply_logger_set_output_fd (logger, fd);

        for (size_t i = 0; i < retained_injections + 1; i++) {
                memset (injection, (int) ('A' + i), injection_size);
                ply_logger_inject_bytes (logger, injection, injection_size);
        }

        PLY_TEST_ASSERT (ply_logger_flush (logger));
        PLY_TEST_ASSERT (lseek (fd, 0, SEEK_SET) == 0);
        PLY_TEST_ASSERT (read_exactly (fd, output, output_size));

        for (size_t i = 0; i < retained_injections; i++) {
                PLY_TEST_ASSERT (output[i * injection_size] == 'B' + i);
                PLY_TEST_ASSERT (output[(i + 1) * injection_size - 1] ==
                                 'B' + i);
        }

        ply_logger_free (logger);
        free (output);
        free (injection);
        return true;
}

static const ply_test_case_t test_cases[] =
{
        PLY_TEST_CASE (test_new_logger_has_deferred_defaults),
        PLY_TEST_CASE (test_deferred_output_waits_for_flush),
        PLY_TEST_CASE (test_immediate_policy_flushes_each_injection),
        PLY_TEST_CASE (test_disabled_logger_discards_injections),
        PLY_TEST_CASE (test_filter_transforms_injected_bytes),
        PLY_TEST_CASE (test_invalid_format_does_not_write_through_percent_n),
        PLY_TEST_CASE (test_full_buffer_retains_recent_injections),
};

PLY_TEST_MAIN (test_cases)
