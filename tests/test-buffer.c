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

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ply-buffer.h"

static bool
test_new_buffer_is_empty (void)
{
        ply_buffer_t *buffer;

        buffer = ply_buffer_new ();

        PLY_TEST_ASSERT (ply_buffer_get_size (buffer) == 0);
        PLY_TEST_ASSERT (ply_buffer_get_capacity (buffer) == 4096);
        PLY_TEST_ASSERT (strcmp (ply_buffer_get_bytes (buffer), "") == 0);

        ply_buffer_free (buffer);
        return true;
}

static bool
test_append_preserves_binary_data (void)
{
        static const char bytes[] = { 'a', '\0', 'b' };
        ply_buffer_t *buffer;

        buffer = ply_buffer_new ();
        ply_buffer_append_bytes (buffer, bytes, sizeof (bytes));

        PLY_TEST_ASSERT (ply_buffer_get_size (buffer) == sizeof (bytes));
        PLY_TEST_ASSERT (memcmp (ply_buffer_get_bytes (buffer),
                                 bytes,
                                 sizeof (bytes)) == 0);
        PLY_TEST_ASSERT (ply_buffer_get_bytes (buffer)[sizeof (bytes)] == '\0');

        ply_buffer_free (buffer);
        return true;
}

static bool
test_formatted_append_rejects_percent_n (void)
{
        ply_buffer_t *buffer;
        int written = -1;

        buffer = ply_buffer_new ();
        ply_buffer_append (buffer, "%s:%d", "value", 7);

        PLY_TEST_ASSERT (strcmp (ply_buffer_get_bytes (buffer), "value:7") == 0);

        ply_buffer_append_with_non_literal_format_string (buffer,
                                                          "%n",
                                                          &written);
        PLY_TEST_ASSERT (written == -1);
        PLY_TEST_ASSERT (strcmp (ply_buffer_get_bytes (buffer), "value:7") == 0);

        ply_buffer_free (buffer);
        return true;
}

static bool
test_remove_clamps_to_available_bytes (void)
{
        ply_buffer_t *buffer;

        buffer = ply_buffer_new ();
        ply_buffer_append (buffer, "abcdef");

        ply_buffer_remove_bytes (buffer, 2);
        PLY_TEST_ASSERT (strcmp (ply_buffer_get_bytes (buffer), "cdef") == 0);

        ply_buffer_remove_bytes_at_end (buffer, 1);
        PLY_TEST_ASSERT (strcmp (ply_buffer_get_bytes (buffer), "cde") == 0);

        ply_buffer_remove_bytes (buffer, 100);
        PLY_TEST_ASSERT (ply_buffer_get_size (buffer) == 0);
        PLY_TEST_ASSERT (strcmp (ply_buffer_get_bytes (buffer), "") == 0);

        ply_buffer_free (buffer);
        return true;
}

static bool
test_append_from_ready_fd (void)
{
        static const char message[] = "pipe data";
        ply_buffer_t *buffer;
        int pipe_fds[2];

        PLY_TEST_ASSERT (pipe (pipe_fds) == 0);
        PLY_TEST_ASSERT (write (pipe_fds[1], message, sizeof (message) - 1) ==
                         sizeof (message) - 1);

        buffer = ply_buffer_new ();
        ply_buffer_append_from_fd (buffer, pipe_fds[0]);

        PLY_TEST_ASSERT (ply_buffer_get_size (buffer) == sizeof (message) - 1);
        PLY_TEST_ASSERT (strcmp (ply_buffer_get_bytes (buffer), message) == 0);

        close (pipe_fds[0]);
        close (pipe_fds[1]);
        ply_buffer_free (buffer);
        return true;
}

static bool
test_steal_resets_buffer (void)
{
        ply_buffer_t *buffer;
        char *bytes;

        buffer = ply_buffer_new ();
        ply_buffer_append (buffer, "owned");
        bytes = ply_buffer_steal_bytes (buffer);

        PLY_TEST_ASSERT (strcmp (bytes, "owned") == 0);
        PLY_TEST_ASSERT (ply_buffer_get_size (buffer) == 0);
        PLY_TEST_ASSERT (strcmp (ply_buffer_get_bytes (buffer), "") == 0);

        free (bytes);
        ply_buffer_free (buffer);
        return true;
}

static bool
test_large_append_reaches_capacity_ceiling (void)
{
        const size_t payload_size = 600000;
        ply_buffer_t *buffer;
        char *payload;

        payload = malloc (payload_size);
        PLY_TEST_ASSERT (payload != NULL);
        memset (payload, 'x', payload_size);

        buffer = ply_buffer_new ();
        ply_buffer_append_bytes (buffer, payload, payload_size);

        PLY_TEST_ASSERT (ply_buffer_get_size (buffer) == payload_size);
        PLY_TEST_ASSERT (ply_buffer_get_capacity (buffer) > payload_size);
        PLY_TEST_ASSERT (memcmp (ply_buffer_get_bytes (buffer),
                                 payload,
                                 payload_size) == 0);
        PLY_TEST_ASSERT (ply_buffer_get_bytes (buffer)[payload_size] == '\0');

        ply_buffer_free (buffer);
        free (payload);
        return true;
}

static const ply_test_case_t test_cases[] =
{
        PLY_TEST_CASE (test_new_buffer_is_empty),
        PLY_TEST_CASE (test_append_preserves_binary_data),
        PLY_TEST_CASE (test_formatted_append_rejects_percent_n),
        PLY_TEST_CASE (test_remove_clamps_to_available_bytes),
        PLY_TEST_CASE (test_append_from_ready_fd),
        PLY_TEST_CASE (test_steal_resets_buffer),
        PLY_TEST_CASE (test_large_append_reaches_capacity_ceiling),
};

PLY_TEST_MAIN (test_cases)
