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

#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "ply-boot-client-private.h"
#include "ply-boot-protocol.h"
#include "ply-event-loop.h"

typedef struct
{
        ply_event_loop_t *loop;
        ply_boot_client_t *client;
        int peer_fd;
        int expected_successes;
        int successes;
        int failures;
        int disconnects;
        int answer_calls;
        int null_answers;
        int multiple_answer_calls;
        int multiple_answer_count;
        char answer[64];
        char multiple_answers[4][32];
        bool timed_out;
} client_context_t;

static void
on_disconnect (void              *user_data,
               ply_boot_client_t *client)
{
        client_context_t *context = user_data;

        if (client != context->client)
                context->failures++;
        context->disconnects++;
}

static bool
initialize_client (client_context_t *context)
{
        int socket_fds[2];

        memset (context, 0, sizeof(*context));
        context->peer_fd = -1;

        if (socketpair (AF_UNIX,
                        SOCK_STREAM | SOCK_CLOEXEC,
                        0,
                        socket_fds) < 0)
                return false;

        context->loop = ply_event_loop_new ();
        context->client = ply_boot_client_new ();
        context->peer_fd = socket_fds[1];

        if (context->loop == NULL || context->client == NULL ||
            !ply_boot_client_connect_to_fd (context->client,
                                            socket_fds[0],
                                            on_disconnect,
                                            context)) {
                close (socket_fds[0]);
                close (socket_fds[1]);
                ply_boot_client_free (context->client);
                ply_event_loop_free (context->loop);
                return false;
        }

        ply_boot_client_attach_to_event_loop (context->client, context->loop);
        return true;
}

static void
free_client_context (client_context_t *context)
{
        ply_boot_client_free (context->client);
        ply_event_loop_free (context->loop);
        if (context->peer_fd >= 0)
                close (context->peer_fd);
}

static bool
write_bytes (int            fd,
             const uint8_t *bytes,
             size_t         size)
{
        size_t offset = 0;

        while (offset < size) {
                ssize_t written;

                written = write (fd, bytes + offset, size - offset);
                if (written <= 0)
                        return false;

                offset += (size_t) written;
        }

        return true;
}

static void
set_uint32_le (uint8_t  *bytes,
               uint32_t value)
{
        bytes[0] = (uint8_t) value;
        bytes[1] = (uint8_t) (value >> 8);
        bytes[2] = (uint8_t) (value >> 16);
        bytes[3] = (uint8_t) (value >> 24);
}

static void
on_watchdog (void             *user_data,
             ply_event_loop_t *loop)
{
        client_context_t *context = user_data;

        context->timed_out = true;
        ply_event_loop_exit (loop, 99);
}

static void
on_success (void              *user_data,
            ply_boot_client_t *client)
{
        client_context_t *context = user_data;

        if (client != context->client) {
                context->failures++;
                ply_event_loop_exit (context->loop, 2);
                return;
        }

        context->successes++;
        if (context->successes == context->expected_successes)
                ply_event_loop_exit (context->loop, 0);
}

static void
on_failure (void              *user_data,
            ply_boot_client_t *client)
{
        client_context_t *context = user_data;

        if (client != context->client)
                context->successes++;

        context->failures++;
        ply_event_loop_exit (context->loop, 0);
}

static void
on_answer (void              *user_data,
           const char        *answer,
           ply_boot_client_t *client)
{
        client_context_t *context = user_data;

        if (client != context->client) {
                context->failures++;
                ply_event_loop_exit (context->loop, 2);
                return;
        }

        context->answer_calls++;
        if (answer == NULL) {
                context->null_answers++;
        } else {
                strncpy (context->answer,
                         answer,
                         sizeof(context->answer) - 1);
        }

        ply_event_loop_exit (context->loop, 0);
}

static void
on_multiple_answers (void              *user_data,
                     const char *const *answers,
                     ply_boot_client_t *client)
{
        client_context_t *context = user_data;
        int i;

        if (client != context->client) {
                context->failures++;
                ply_event_loop_exit (context->loop, 2);
                return;
        }

        context->multiple_answer_calls++;
        for (i = 0; answers[i] != NULL && i < 4; i++) {
                strncpy (context->multiple_answers[i],
                         answers[i],
                         sizeof(context->multiple_answers[i]) - 1);
        }
        context->multiple_answer_count = i;
        ply_event_loop_exit (context->loop, 0);
}

static void
watch_for_timeout (client_context_t *context)
{
        ply_event_loop_watch_for_timeout (context->loop,
                                          1.0,
                                          on_watchdog,
                                          context);
}

static ssize_t
read_request (int      fd,
              uint8_t *bytes,
              size_t   capacity)
{
        return recv (fd, bytes, capacity, MSG_DONTWAIT);
}

static bool
test_ping_frame_receives_ack (void)
{
        static const uint8_t response[] = PLY_BOOT_PROTOCOL_RESPONSE_TYPE_ACK;
        static const uint8_t expected_request[] = {
                PLY_BOOT_PROTOCOL_REQUEST_TYPE_PING[0], 0x00,
        };
        client_context_t context;
        uint8_t request[16];
        ssize_t request_size;

        PLY_TEST_ASSERT (initialize_client (&context));
        context.expected_successes = 1;
        PLY_TEST_ASSERT (write_bytes (context.peer_fd,
                                      response,
                                      sizeof(response) - 1));
        ply_boot_client_ping_daemon (context.client,
                                     on_success,
                                     on_failure,
                                     &context);
        watch_for_timeout (&context);

        PLY_TEST_ASSERT (ply_event_loop_run (context.loop) == 0);
        PLY_TEST_ASSERT (!context.timed_out);
        PLY_TEST_ASSERT (context.successes == 1);
        PLY_TEST_ASSERT (context.failures == 0);

        request_size = read_request (context.peer_fd, request, sizeof(request));
        PLY_TEST_ASSERT (request_size == (ssize_t) sizeof(expected_request));
        PLY_TEST_ASSERT (memcmp (request,
                                 expected_request,
                                 sizeof(expected_request)) == 0);

        free_client_context (&context);
        return true;
}

static bool
test_argument_frame_includes_marker_length_and_nul (void)
{
        static const uint8_t response[] = PLY_BOOT_PROTOCOL_RESPONSE_TYPE_ACK;
        static const uint8_t expected_request[] = {
                PLY_BOOT_PROTOCOL_REQUEST_TYPE_UPDATE[0],
                0x02, 0x06, 'r', 'e', 'a', 'd', 'y', 0x00,
        };
        client_context_t context;
        uint8_t request[32];
        ssize_t request_size;

        PLY_TEST_ASSERT (initialize_client (&context));
        context.expected_successes = 1;
        PLY_TEST_ASSERT (write_bytes (context.peer_fd,
                                      response,
                                      sizeof(response) - 1));
        ply_boot_client_update_daemon (context.client,
                                       "ready",
                                       on_success,
                                       on_failure,
                                       &context);
        watch_for_timeout (&context);

        PLY_TEST_ASSERT (ply_event_loop_run (context.loop) == 0);
        PLY_TEST_ASSERT (!context.timed_out);
        PLY_TEST_ASSERT (context.successes == 1);

        request_size = read_request (context.peer_fd, request, sizeof(request));
        PLY_TEST_ASSERT (request_size == (ssize_t) sizeof(expected_request));
        PLY_TEST_ASSERT (memcmp (request,
                                 expected_request,
                                 sizeof(expected_request)) == 0);

        free_client_context (&context);
        return true;
}

static bool
test_queued_requests_match_pipelined_acks (void)
{
        static const uint8_t responses[] = {
                PLY_BOOT_PROTOCOL_RESPONSE_TYPE_ACK[0],
                PLY_BOOT_PROTOCOL_RESPONSE_TYPE_ACK[0],
        };
        static const uint8_t expected_request[] = {
                PLY_BOOT_PROTOCOL_REQUEST_TYPE_PING[0], 0x00,
                PLY_BOOT_PROTOCOL_REQUEST_TYPE_SHOW_SPLASH[0], 0x00,
        };
        client_context_t context;
        uint8_t request[32];
        ssize_t request_size;

        PLY_TEST_ASSERT (initialize_client (&context));
        context.expected_successes = 2;
        PLY_TEST_ASSERT (write_bytes (context.peer_fd,
                                      responses,
                                      sizeof(responses)));
        ply_boot_client_ping_daemon (context.client,
                                     on_success,
                                     on_failure,
                                     &context);
        ply_boot_client_tell_daemon_to_show_splash (context.client,
                                                    on_success,
                                                    on_failure,
                                                    &context);
        watch_for_timeout (&context);

        PLY_TEST_ASSERT (ply_event_loop_run (context.loop) == 0);
        PLY_TEST_ASSERT (!context.timed_out);
        PLY_TEST_ASSERT (context.successes == 2);
        PLY_TEST_ASSERT (context.failures == 0);

        request_size = read_request (context.peer_fd, request, sizeof(request));
        PLY_TEST_ASSERT (request_size == (ssize_t) sizeof(expected_request));
        PLY_TEST_ASSERT (memcmp (request,
                                 expected_request,
                                 sizeof(expected_request)) == 0);

        free_client_context (&context);
        return true;
}

static bool
test_answer_and_no_answer_responses (void)
{
        uint8_t answer_response[1 + 4 + 6] = {
                PLY_BOOT_PROTOCOL_RESPONSE_TYPE_ANSWER[0],
        };
        static const uint8_t no_answer_response[] =
                PLY_BOOT_PROTOCOL_RESPONSE_TYPE_NO_ANSWER;
        client_context_t context;

        set_uint32_le (&answer_response[1], 6);
        memcpy (&answer_response[5], "secret", 6);

        PLY_TEST_ASSERT (initialize_client (&context));
        PLY_TEST_ASSERT (write_bytes (context.peer_fd,
                                      answer_response,
                                      sizeof(answer_response)));
        ply_boot_client_ask_daemon_for_password (context.client,
                                                 "Password:",
                                                 on_answer,
                                                 on_failure,
                                                 &context);
        watch_for_timeout (&context);
        PLY_TEST_ASSERT (ply_event_loop_run (context.loop) == 0);
        PLY_TEST_ASSERT (context.answer_calls == 1);
        PLY_TEST_ASSERT (strcmp (context.answer, "secret") == 0);
        free_client_context (&context);

        PLY_TEST_ASSERT (initialize_client (&context));
        PLY_TEST_ASSERT (write_bytes (context.peer_fd,
                                      no_answer_response,
                                      sizeof(no_answer_response) - 1));
        ply_boot_client_ask_daemon_for_password (context.client,
                                                 "Password:",
                                                 on_answer,
                                                 on_failure,
                                                 &context);
        watch_for_timeout (&context);
        PLY_TEST_ASSERT (ply_event_loop_run (context.loop) == 0);
        PLY_TEST_ASSERT (context.answer_calls == 1);
        PLY_TEST_ASSERT (context.null_answers == 1);
        free_client_context (&context);
        return true;
}

static bool
test_multiple_answer_response_splits_strings (void)
{
        uint8_t response[1 + 4 + 8] = {
                PLY_BOOT_PROTOCOL_RESPONSE_TYPE_MULTIPLE_ANSWERS[0],
        };
        client_context_t context;

        set_uint32_le (&response[1], 8);
        memcpy (&response[5], "one\0two\0", 8);

        PLY_TEST_ASSERT (initialize_client (&context));
        PLY_TEST_ASSERT (write_bytes (context.peer_fd, response, sizeof(response)));
        ply_boot_client_ask_daemon_for_cached_passwords (context.client,
                                                         on_multiple_answers,
                                                         on_failure,
                                                         &context);
        watch_for_timeout (&context);

        PLY_TEST_ASSERT (ply_event_loop_run (context.loop) == 0);
        PLY_TEST_ASSERT (!context.timed_out);
        PLY_TEST_ASSERT (context.multiple_answer_calls == 1);
        PLY_TEST_ASSERT (context.multiple_answer_count == 2);
        PLY_TEST_ASSERT (strcmp (context.multiple_answers[0], "one") == 0);
        PLY_TEST_ASSERT (strcmp (context.multiple_answers[1], "two") == 0);
        PLY_TEST_ASSERT (context.failures == 0);

        free_client_context (&context);
        return true;
}

static bool
run_failed_reply (const uint8_t *response,
                  size_t         response_size)
{
        client_context_t context;
        bool passed;

        if (!initialize_client (&context))
                return false;

        if (!write_bytes (context.peer_fd, response, response_size)) {
                free_client_context (&context);
                return false;
        }

        ply_boot_client_ping_daemon (context.client,
                                     on_success,
                                     on_failure,
                                     &context);
        watch_for_timeout (&context);

        passed = ply_event_loop_run (context.loop) == 0 &&
                 !context.timed_out &&
                 context.successes == 0 &&
                 context.failures == 1;
        free_client_context (&context);
        return passed;
}

static bool
test_nak_and_malformed_payloads_fail_requests (void)
{
        static const uint8_t nak[] = PLY_BOOT_PROTOCOL_RESPONSE_TYPE_NAK;
        uint8_t oversized_answer[5] = {
                PLY_BOOT_PROTOCOL_RESPONSE_TYPE_ANSWER[0],
        };
        uint8_t unterminated_answers[1 + 4 + 3] = {
                PLY_BOOT_PROTOCOL_RESPONSE_TYPE_MULTIPLE_ANSWERS[0],
        };

        set_uint32_le (&oversized_answer[1], UINT32_C (1024 * 1024 + 1));
        set_uint32_le (&unterminated_answers[1], 3);
        memcpy (&unterminated_answers[5], "bad", 3);

        PLY_TEST_ASSERT (run_failed_reply (nak, sizeof(nak) - 1));
        PLY_TEST_ASSERT (run_failed_reply (oversized_answer,
                                           sizeof(oversized_answer)));
        PLY_TEST_ASSERT (run_failed_reply (unterminated_answers,
                                           sizeof(unterminated_answers)));
        return true;
}

static bool
test_peer_disconnect_cancels_request (void)
{
        client_context_t context;

        PLY_TEST_ASSERT (initialize_client (&context));
        ply_boot_client_ping_daemon (context.client,
                                     on_success,
                                     on_failure,
                                     &context);
        watch_for_timeout (&context);
        close (context.peer_fd);
        context.peer_fd = -1;

        PLY_TEST_ASSERT (ply_event_loop_run (context.loop) == 0);
        PLY_TEST_ASSERT (!context.timed_out);
        PLY_TEST_ASSERT (context.successes == 0);
        PLY_TEST_ASSERT (context.failures == 1);
        PLY_TEST_ASSERT (context.disconnects == 1);

        free_client_context (&context);
        return true;
}

static bool
test_disconnected_request_fails_without_running_loop (void)
{
        client_context_t context = { 0 };

        context.loop = ply_event_loop_new ();
        context.client = ply_boot_client_new ();
        PLY_TEST_ASSERT (context.loop != NULL);
        PLY_TEST_ASSERT (context.client != NULL);
        ply_boot_client_attach_to_event_loop (context.client, context.loop);

        ply_boot_client_ping_daemon (context.client,
                                     on_success,
                                     on_failure,
                                     &context);
        PLY_TEST_ASSERT (context.failures == 1);
        PLY_TEST_ASSERT (context.successes == 0);

        ply_boot_client_free (context.client);
        ply_event_loop_free (context.loop);
        return true;
}

static const ply_test_case_t test_cases[] =
{
        PLY_TEST_CASE (test_ping_frame_receives_ack),
        PLY_TEST_CASE (test_argument_frame_includes_marker_length_and_nul),
        PLY_TEST_CASE (test_queued_requests_match_pipelined_acks),
        PLY_TEST_CASE (test_answer_and_no_answer_responses),
        PLY_TEST_CASE (test_multiple_answer_response_splits_strings),
        PLY_TEST_CASE (test_nak_and_malformed_payloads_fail_requests),
        PLY_TEST_CASE (test_peer_disconnect_cancels_request),
        PLY_TEST_CASE (test_disconnected_request_fails_without_running_loop),
};

PLY_TEST_MAIN (test_cases)
