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
#include <sys/types.h>
#include <unistd.h>

#include "ply-boot-protocol.h"
#include "ply-boot-server-private.h"
#include "ply-event-loop.h"
#include "ply-peer-credentials-private.h"
#include "ply-trigger.h"

typedef struct
{
        ply_event_loop_t *loop;
        ply_boot_server_t *server;
        int peer_fd;
        ply_fd_watch_t *peer_watch;
        size_t expected_response_size;
        size_t response_size;
        uint8_t response[64];
        int peer_disconnects;
        bool expect_disconnect;
        bool timed_out;

        int update_count;
        char updates[2][64];
        int system_update_count;
        int system_updates[2];
        int show_splash_count;
        int initialized_count;
        int question_count;
        char question[64];
        int active_vt_count;
        bool has_active_vt;
        uint32_t dispatches;
        char change_mode[32];
        char displayed_message[64];
        char hidden_message[64];
        char ignored_keys[32];
        char newroot[64];
        int password_count;
        char password_prompt[64];
        int keystroke_count;
        char watched_keys[32];
        int deactivate_count;
        int quit_count;
        bool retain_splash[2];
} server_context_t;

enum
{
        DISPATCH_CHANGE_MODE = 1 << 0,
        DISPATCH_DISPLAY_MESSAGE = 1 << 1,
        DISPATCH_HIDE_MESSAGE = 1 << 2,
        DISPATCH_IGNORE_KEYSTROKE = 1 << 3,
        DISPATCH_PROGRESS_PAUSE = 1 << 4,
        DISPATCH_PROGRESS_UNPAUSE = 1 << 5,
        DISPATCH_HIDE_SPLASH = 1 << 6,
        DISPATCH_NEWROOT = 1 << 7,
        DISPATCH_ERROR = 1 << 8,
        DISPATCH_REACTIVATE = 1 << 9,
        DISPATCH_RELOAD = 1 << 10,
        DISPATCH_PASSWORD = 1 << 11,
        DISPATCH_WATCH_KEYSTROKE = 1 << 12,
        DISPATCH_DEACTIVATE = 1 << 13,
        DISPATCH_QUIT = 1 << 14,
};

static bool
record_dispatch (server_context_t *context,
                 ply_boot_server_t *server,
                 uint32_t           dispatch)
{
        if (server != context->server)
                return false;

        context->dispatches |= dispatch;
        return true;
}

static void
on_update (void              *user_data,
           const char        *status,
           ply_boot_server_t *server)
{
        server_context_t *context = user_data;

        if (server != context->server || context->update_count >= 2)
                return;

        if (status != NULL)
                strncpy (context->updates[context->update_count],
                         status,
                         sizeof(context->updates[0]) - 1);
        context->update_count++;
}

static void
on_system_update (void              *user_data,
                  int                progress,
                  ply_boot_server_t *server)
{
        server_context_t *context = user_data;

        if (server != context->server || context->system_update_count >= 2)
                return;

        context->system_updates[context->system_update_count++] = progress;
}

static void
on_change_mode (void              *user_data,
                const char        *mode,
                ply_boot_server_t *server)
{
        server_context_t *context = user_data;

        if (!record_dispatch (context, server, DISPATCH_CHANGE_MODE))
                return;

        if (mode != NULL)
                strncpy (context->change_mode,
                         mode,
                         sizeof(context->change_mode) - 1);
}

static void
on_display_message (void              *user_data,
                    const char        *message,
                    ply_boot_server_t *server)
{
        server_context_t *context = user_data;

        if (!record_dispatch (context, server, DISPATCH_DISPLAY_MESSAGE))
                return;

        if (message != NULL)
                strncpy (context->displayed_message,
                         message,
                         sizeof(context->displayed_message) - 1);
}

static void
on_hide_message (void              *user_data,
                 const char        *message,
                 ply_boot_server_t *server)
{
        server_context_t *context = user_data;

        if (!record_dispatch (context, server, DISPATCH_HIDE_MESSAGE))
                return;

        if (message != NULL)
                strncpy (context->hidden_message,
                         message,
                         sizeof(context->hidden_message) - 1);
}

static void
on_ignore_keystroke (void              *user_data,
                     const char        *keys,
                     ply_boot_server_t *server)
{
        server_context_t *context = user_data;

        if (!record_dispatch (context, server, DISPATCH_IGNORE_KEYSTROKE))
                return;

        if (keys != NULL)
                strncpy (context->ignored_keys,
                         keys,
                         sizeof(context->ignored_keys) - 1);
}

static void
on_progress_pause (void              *user_data,
                   ply_boot_server_t *server)
{
        server_context_t *context = user_data;

        record_dispatch (context, server, DISPATCH_PROGRESS_PAUSE);
}

static void
on_progress_unpause (void              *user_data,
                     ply_boot_server_t *server)
{
        server_context_t *context = user_data;

        record_dispatch (context, server, DISPATCH_PROGRESS_UNPAUSE);
}

static void
on_password (void              *user_data,
             const char        *prompt,
             ply_trigger_t     *answer,
             ply_boot_server_t *server)
{
        server_context_t *context = user_data;

        if (!record_dispatch (context, server, DISPATCH_PASSWORD))
                return;

        context->password_count++;
        if (prompt != NULL)
                strncpy (context->password_prompt,
                         prompt,
                         sizeof(context->password_prompt) - 1);
        ply_trigger_pull (answer, "secret");
}

static void
on_watch_keystroke (void              *user_data,
                    const char        *keys,
                    ply_trigger_t     *answer,
                    ply_boot_server_t *server)
{
        server_context_t *context = user_data;

        if (!record_dispatch (context, server, DISPATCH_WATCH_KEYSTROKE))
                return;

        context->keystroke_count++;
        if (keys != NULL)
                strncpy (context->watched_keys,
                         keys,
                         sizeof(context->watched_keys) - 1);
        ply_trigger_pull (answer, "y");
}

static void
on_show_splash (void              *user_data,
                ply_boot_server_t *server)
{
        server_context_t *context = user_data;

        if (server == context->server)
                context->show_splash_count++;
}

static void
on_hide_splash (void              *user_data,
                ply_boot_server_t *server)
{
        server_context_t *context = user_data;

        record_dispatch (context, server, DISPATCH_HIDE_SPLASH);
}

static void
on_newroot (void              *user_data,
            const char        *root_dir,
            ply_boot_server_t *server)
{
        server_context_t *context = user_data;

        if (!record_dispatch (context, server, DISPATCH_NEWROOT))
                return;

        if (root_dir != NULL)
                strncpy (context->newroot,
                         root_dir,
                         sizeof(context->newroot) - 1);
}

static void
on_initialized (void              *user_data,
                ply_boot_server_t *server)
{
        server_context_t *context = user_data;

        if (server == context->server)
                context->initialized_count++;
}

static void
on_error (void              *user_data,
          ply_boot_server_t *server)
{
        server_context_t *context = user_data;

        record_dispatch (context, server, DISPATCH_ERROR);
}

static void
on_reactivate (void              *user_data,
               ply_boot_server_t *server)
{
        server_context_t *context = user_data;

        record_dispatch (context, server, DISPATCH_REACTIVATE);
}

static bool
on_reload (void              *user_data,
           ply_boot_server_t *server)
{
        server_context_t *context = user_data;

        return record_dispatch (context, server, DISPATCH_RELOAD);
}

static void
on_deactivate (void              *user_data,
               ply_trigger_t     *deactivate_trigger,
               ply_boot_server_t *server)
{
        server_context_t *context = user_data;

        if (!record_dispatch (context, server, DISPATCH_DEACTIVATE))
                return;

        context->deactivate_count++;
        ply_trigger_pull (deactivate_trigger, NULL);
}

static void
on_quit (void              *user_data,
         bool               retain_splash,
         ply_trigger_t     *quit_trigger,
         ply_boot_server_t *server)
{
        server_context_t *context = user_data;

        if (!record_dispatch (context, server, DISPATCH_QUIT))
                return;

        if (context->quit_count < 2)
                context->retain_splash[context->quit_count] = retain_splash;
        context->quit_count++;
        ply_trigger_pull (quit_trigger, NULL);
}

static void
on_question (void              *user_data,
             const char        *prompt,
             ply_trigger_t     *answer,
             ply_boot_server_t *server)
{
        server_context_t *context = user_data;

        if (server != context->server)
                return;

        context->question_count++;
        if (prompt != NULL)
                strncpy (context->question,
                         prompt,
                         sizeof(context->question) - 1);
        ply_trigger_pull (answer, "yes");
}

static bool
on_has_active_vt (void              *user_data,
                  ply_boot_server_t *server)
{
        server_context_t *context = user_data;

        if (server != context->server)
                return false;

        context->active_vt_count++;
        return context->has_active_vt;
}

static ply_boot_server_t *
new_server (server_context_t *context)
{
        return ply_boot_server_new (
                on_update,
                on_change_mode,
                on_system_update,
                on_password,
                on_question,
                on_display_message,
                on_hide_message,
                on_watch_keystroke,
                on_ignore_keystroke,
                on_progress_pause,
                on_progress_unpause,
                on_show_splash,
                on_hide_splash,
                on_newroot,
                on_initialized,
                on_error,
                on_deactivate,
                on_reactivate,
                on_quit,
                on_has_active_vt,
                on_reload,
                context);
}

static void
on_peer_response (void *user_data,
                  int   source_fd)
{
        server_context_t *context = user_data;
        ssize_t bytes_read;

        bytes_read = recv (source_fd,
                           context->response + context->response_size,
                           sizeof(context->response) - context->response_size,
                           0);
        if (bytes_read <= 0)
                return;

        context->response_size += (size_t) bytes_read;
        if (context->response_size >= context->expected_response_size)
                ply_event_loop_exit (context->loop, 0);
}

static void
on_peer_disconnect (void *user_data,
                    int   source_fd)
{
        server_context_t *context = user_data;

        context->peer_disconnects++;
        ply_event_loop_exit (context->loop,
                             context->expect_disconnect ? 0 : 2);
}

static void
on_watchdog (void             *user_data,
             ply_event_loop_t *loop)
{
        server_context_t *context = user_data;

        context->timed_out = true;
        ply_event_loop_exit (loop, 99);
}

static bool
initialize_server (server_context_t *context,
                   uid_t             credential_uid,
                   bool              credentials_available)
{
        int socket_fds[2];

        memset (context, 0, sizeof(*context));
        context->peer_fd = -1;

        if (socketpair (AF_UNIX,
                        SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK,
                        0,
                        socket_fds) < 0)
                return false;

        if (credentials_available)
                ply_peer_credentials_set (getpid (), credential_uid, getgid ());
        else
                ply_peer_credentials_set_unavailable ();

        context->loop = ply_event_loop_new ();
        context->server = new_server (context);
        context->peer_fd = socket_fds[1];

        if (context->loop == NULL || context->server == NULL ||
            !ply_boot_server_attach_connection_to_event_loop (context->server,
                                                              context->loop,
                                                              socket_fds[0])) {
                close (socket_fds[0]);
                close (socket_fds[1]);
                ply_boot_server_free (context->server);
                ply_event_loop_free (context->loop);
                return false;
        }

        return true;
}

static void
free_server_context (server_context_t *context)
{
        ply_boot_server_free (context->server);
        ply_event_loop_free (context->loop);
        if (context->peer_fd >= 0)
                close (context->peer_fd);
        ply_peer_credentials_use_system ();
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
watch_for_response (server_context_t *context,
                    size_t            response_size)
{
        context->expected_response_size = response_size;
        context->peer_watch =
                ply_event_loop_watch_fd (context->loop,
                                         context->peer_fd,
                                         PLY_EVENT_LOOP_FD_STATUS_HAS_DATA,
                                         on_peer_response,
                                         on_peer_disconnect,
                                         context);
        ply_event_loop_watch_for_timeout (context->loop,
                                          1.0,
                                          on_watchdog,
                                          context);
}

static void
watch_for_disconnect (server_context_t *context)
{
        context->expect_disconnect = true;
        context->peer_watch =
                ply_event_loop_watch_fd (context->loop,
                                         context->peer_fd,
                                         PLY_EVENT_LOOP_FD_STATUS_HAS_DATA,
                                         on_peer_response,
                                         on_peer_disconnect,
                                         context);
        ply_event_loop_watch_for_timeout (context->loop,
                                          1.0,
                                          on_watchdog,
                                          context);
}

static bool
test_update_argument_dispatches_and_acknowledges (void)
{
        static const uint8_t request[] = {
                PLY_BOOT_PROTOCOL_REQUEST_TYPE_UPDATE[0],
                0x02, 0x06, 'r', 'e', 'a', 'd', 'y', 0x00,
        };
        server_context_t context;

        PLY_TEST_ASSERT (initialize_server (&context, 0, true));
        PLY_TEST_ASSERT (write_bytes (context.peer_fd,
                                      request,
                                      sizeof(request)));
        watch_for_response (&context, 1);

        PLY_TEST_ASSERT (ply_event_loop_run (context.loop) == 0);
        PLY_TEST_ASSERT (!context.timed_out);
        PLY_TEST_ASSERT (context.update_count == 1);
        PLY_TEST_ASSERT (strcmp (context.updates[0], "ready") == 0);
        PLY_TEST_ASSERT (context.response_size == 1);
        PLY_TEST_ASSERT (context.response[0] ==
                         PLY_BOOT_PROTOCOL_RESPONSE_TYPE_ACK[0]);

        free_server_context (&context);
        return true;
}

static bool
test_pipelined_notifications_each_receive_ack (void)
{
        static const uint8_t request[] = {
                PLY_BOOT_PROTOCOL_REQUEST_TYPE_SHOW_SPLASH[0], 0x00,
                PLY_BOOT_PROTOCOL_REQUEST_TYPE_SYSTEM_INITIALIZED[0], 0x00,
        };
        server_context_t context;

        PLY_TEST_ASSERT (initialize_server (&context, 0, true));
        PLY_TEST_ASSERT (write_bytes (context.peer_fd,
                                      request,
                                      sizeof(request)));
        watch_for_response (&context, 2);

        PLY_TEST_ASSERT (ply_event_loop_run (context.loop) == 0);
        PLY_TEST_ASSERT (!context.timed_out);
        PLY_TEST_ASSERT (context.show_splash_count == 1);
        PLY_TEST_ASSERT (context.initialized_count == 1);
        PLY_TEST_ASSERT (context.response_size == 2);
        PLY_TEST_ASSERT (context.response[0] ==
                         PLY_BOOT_PROTOCOL_RESPONSE_TYPE_ACK[0]);
        PLY_TEST_ASSERT (context.response[1] ==
                         PLY_BOOT_PROTOCOL_RESPONSE_TYPE_ACK[0]);

        free_server_context (&context);
        return true;
}

static bool
test_unknown_command_naks_without_breaking_pipeline (void)
{
        static const uint8_t request[] = {
                '?', 0x00,
                PLY_BOOT_PROTOCOL_REQUEST_TYPE_PING[0], 0x00,
        };
        server_context_t context;

        PLY_TEST_ASSERT (initialize_server (&context, 0, true));
        PLY_TEST_ASSERT (write_bytes (context.peer_fd,
                                      request,
                                      sizeof(request)));
        watch_for_response (&context, 2);

        PLY_TEST_ASSERT (ply_event_loop_run (context.loop) == 0);
        PLY_TEST_ASSERT (!context.timed_out);
        PLY_TEST_ASSERT (context.response_size == 2);
        PLY_TEST_ASSERT (context.response[0] ==
                         PLY_BOOT_PROTOCOL_RESPONSE_TYPE_NAK[0]);
        PLY_TEST_ASSERT (context.response[1] ==
                         PLY_BOOT_PROTOCOL_RESPONSE_TYPE_ACK[0]);

        free_server_context (&context);
        return true;
}

static bool
test_non_root_request_is_rejected (void)
{
        static const uint8_t request[] = {
                PLY_BOOT_PROTOCOL_REQUEST_TYPE_UPDATE[0],
                0x02, 0x04, 'b', 'a', 'd', 0x00,
        };
        server_context_t context;

        PLY_TEST_ASSERT (initialize_server (&context, 1000, true));
        PLY_TEST_ASSERT (write_bytes (context.peer_fd,
                                      request,
                                      sizeof(request)));
        watch_for_response (&context, 1);

        PLY_TEST_ASSERT (ply_event_loop_run (context.loop) == 0);
        PLY_TEST_ASSERT (!context.timed_out);
        PLY_TEST_ASSERT (context.update_count == 0);
        PLY_TEST_ASSERT (context.response_size == 1);
        PLY_TEST_ASSERT (context.response[0] ==
                         PLY_BOOT_PROTOCOL_RESPONSE_TYPE_NAK[0]);

        free_server_context (&context);
        return true;
}

static bool
test_system_update_parses_valid_and_invalid_values (void)
{
        static const uint8_t request[] = {
                PLY_BOOT_PROTOCOL_REQUEST_TYPE_SYSTEM_UPDATE[0],
                0x02, 0x03, '7', '3', 0x00,
                PLY_BOOT_PROTOCOL_REQUEST_TYPE_SYSTEM_UPDATE[0],
                0x02, 0x04, 'b', 'a', 'd', 0x00,
        };
        server_context_t context;

        PLY_TEST_ASSERT (initialize_server (&context, 0, true));
        PLY_TEST_ASSERT (write_bytes (context.peer_fd,
                                      request,
                                      sizeof(request)));
        watch_for_response (&context, 2);

        PLY_TEST_ASSERT (ply_event_loop_run (context.loop) == 0);
        PLY_TEST_ASSERT (!context.timed_out);
        PLY_TEST_ASSERT (context.system_update_count == 2);
        PLY_TEST_ASSERT (context.system_updates[0] == 73);
        PLY_TEST_ASSERT (context.system_updates[1] == 0);
        PLY_TEST_ASSERT (context.response_size == 2);

        free_server_context (&context);
        return true;
}

static bool
test_immediate_notifications_dispatch_and_acknowledge (void)
{
        static const uint8_t request[] = {
                PLY_BOOT_PROTOCOL_REQUEST_TYPE_CHANGE_MODE[0],
                0x02, 0x09, 's', 'h', 'u', 't', 'd', 'o', 'w', 'n', 0x00,
                PLY_BOOT_PROTOCOL_REQUEST_TYPE_SHOW_MESSAGE[0],
                0x02, 0x09, 's', 't', 'a', 'r', 't', 'i', 'n', 'g', 0x00,
                PLY_BOOT_PROTOCOL_REQUEST_TYPE_HIDE_MESSAGE[0],
                0x02, 0x09, 's', 't', 'a', 'r', 't', 'i', 'n', 'g', 0x00,
                PLY_BOOT_PROTOCOL_REQUEST_TYPE_KEYSTROKE_REMOVE[0],
                0x02, 0x03, 'y', 'n', 0x00,
                PLY_BOOT_PROTOCOL_REQUEST_TYPE_PROGRESS_PAUSE[0], 0x00,
                PLY_BOOT_PROTOCOL_REQUEST_TYPE_PROGRESS_UNPAUSE[0], 0x00,
                PLY_BOOT_PROTOCOL_REQUEST_TYPE_HIDE_SPLASH[0], 0x00,
                PLY_BOOT_PROTOCOL_REQUEST_TYPE_NEWROOT[0],
                0x02, 0x09, '/', 's', 'y', 's', 'r', 'o', 'o', 't', 0x00,
                PLY_BOOT_PROTOCOL_REQUEST_TYPE_ERROR[0], 0x00,
                PLY_BOOT_PROTOCOL_REQUEST_TYPE_REACTIVATE[0], 0x00,
                PLY_BOOT_PROTOCOL_REQUEST_TYPE_RELOAD[0], 0x00,
        };
        static const uint8_t expected_response[] = {
                PLY_BOOT_PROTOCOL_RESPONSE_TYPE_ACK[0],
                PLY_BOOT_PROTOCOL_RESPONSE_TYPE_ACK[0],
                PLY_BOOT_PROTOCOL_RESPONSE_TYPE_ACK[0],
                PLY_BOOT_PROTOCOL_RESPONSE_TYPE_ACK[0],
                PLY_BOOT_PROTOCOL_RESPONSE_TYPE_ACK[0],
                PLY_BOOT_PROTOCOL_RESPONSE_TYPE_ACK[0],
                PLY_BOOT_PROTOCOL_RESPONSE_TYPE_ACK[0],
                PLY_BOOT_PROTOCOL_RESPONSE_TYPE_ACK[0],
                PLY_BOOT_PROTOCOL_RESPONSE_TYPE_ACK[0],
                PLY_BOOT_PROTOCOL_RESPONSE_TYPE_ACK[0],
                PLY_BOOT_PROTOCOL_RESPONSE_TYPE_ACK[0],
        };
        static const uint32_t expected_dispatches =
                DISPATCH_CHANGE_MODE |
                DISPATCH_DISPLAY_MESSAGE |
                DISPATCH_HIDE_MESSAGE |
                DISPATCH_IGNORE_KEYSTROKE |
                DISPATCH_PROGRESS_PAUSE |
                DISPATCH_PROGRESS_UNPAUSE |
                DISPATCH_HIDE_SPLASH |
                DISPATCH_NEWROOT |
                DISPATCH_ERROR |
                DISPATCH_REACTIVATE |
                DISPATCH_RELOAD;
        server_context_t context;

        PLY_TEST_ASSERT (initialize_server (&context, 0, true));
        PLY_TEST_ASSERT (write_bytes (context.peer_fd,
                                      request,
                                      sizeof(request)));
        watch_for_response (&context, sizeof(expected_response));

        PLY_TEST_ASSERT (ply_event_loop_run (context.loop) == 0);
        PLY_TEST_ASSERT (!context.timed_out);
        PLY_TEST_ASSERT (context.dispatches == expected_dispatches);
        PLY_TEST_ASSERT (strcmp (context.change_mode, "shutdown") == 0);
        PLY_TEST_ASSERT (strcmp (context.displayed_message, "starting") == 0);
        PLY_TEST_ASSERT (strcmp (context.hidden_message, "starting") == 0);
        PLY_TEST_ASSERT (strcmp (context.ignored_keys, "yn") == 0);
        PLY_TEST_ASSERT (strcmp (context.newroot, "/sysroot") == 0);
        PLY_TEST_ASSERT (context.response_size == sizeof(expected_response));
        PLY_TEST_ASSERT (memcmp (context.response,
                                 expected_response,
                                 sizeof(expected_response)) == 0);

        free_server_context (&context);
        return true;
}

static bool
run_active_vt_request (bool has_active_vt,
                       uint8_t expected_response)
{
        static const uint8_t request[] = {
                PLY_BOOT_PROTOCOL_REQUEST_TYPE_HAS_ACTIVE_VT[0], 0x00,
        };
        server_context_t context;
        bool passed;

        if (!initialize_server (&context, 0, true))
                return false;

        context.has_active_vt = has_active_vt;
        if (!write_bytes (context.peer_fd, request, sizeof(request))) {
                free_server_context (&context);
                return false;
        }
        watch_for_response (&context, 1);

        passed = ply_event_loop_run (context.loop) == 0 &&
                 !context.timed_out &&
                 context.active_vt_count == 1 &&
                 context.response_size == 1 &&
                 context.response[0] == expected_response;
        free_server_context (&context);
        return passed;
}

static bool
test_active_vt_response_follows_handler (void)
{
        PLY_TEST_ASSERT (run_active_vt_request (
                                 true,
                                 PLY_BOOT_PROTOCOL_RESPONSE_TYPE_ACK[0]));
        PLY_TEST_ASSERT (run_active_vt_request (
                                 false,
                                 PLY_BOOT_PROTOCOL_RESPONSE_TYPE_NAK[0]));
        return true;
}

static bool
test_question_trigger_sends_answer_payload (void)
{
        static const uint8_t request[] = {
                PLY_BOOT_PROTOCOL_REQUEST_TYPE_QUESTION[0],
                0x02, 0x07, 'R', 'e', 'a', 'd', 'y', '?', 0x00,
        };
        static const uint8_t expected_response[] = {
                PLY_BOOT_PROTOCOL_RESPONSE_TYPE_ANSWER[0],
                0x03, 0x00, 0x00, 0x00, 'y', 'e', 's',
        };
        server_context_t context;

        PLY_TEST_ASSERT (initialize_server (&context, 0, true));
        PLY_TEST_ASSERT (write_bytes (context.peer_fd,
                                      request,
                                      sizeof(request)));
        watch_for_response (&context, sizeof(expected_response));

        PLY_TEST_ASSERT (ply_event_loop_run (context.loop) == 0);
        PLY_TEST_ASSERT (!context.timed_out);
        PLY_TEST_ASSERT (context.question_count == 1);
        PLY_TEST_ASSERT (strcmp (context.question, "Ready?") == 0);
        PLY_TEST_ASSERT (context.response_size == sizeof(expected_response));
        PLY_TEST_ASSERT (memcmp (context.response,
                                 expected_response,
                                 sizeof(expected_response)) == 0);

        free_server_context (&context);
        return true;
}

static bool
run_rejected_frame (const uint8_t *request,
                    size_t         request_size,
                    bool           credentials_available)
{
        server_context_t context;
        bool passed;

        if (!initialize_server (&context, 0, credentials_available))
                return false;

        if (!write_bytes (context.peer_fd, request, request_size)) {
                free_server_context (&context);
                return false;
        }
        watch_for_disconnect (&context);

        passed = ply_event_loop_run (context.loop) == 0 &&
                 !context.timed_out &&
                 context.peer_disconnects == 1 &&
                 context.update_count == 0 &&
                 context.response_size == 0;
        free_server_context (&context);
        return passed;
}

static bool
test_malformed_and_uncredentialed_frames_disconnect (void)
{
        static const uint8_t truncated_header[] = {
                PLY_BOOT_PROTOCOL_REQUEST_TYPE_UPDATE[0],
        };
        static const uint8_t invalid_argument_marker[] = {
                PLY_BOOT_PROTOCOL_REQUEST_TYPE_UPDATE[0], 0x01,
        };
        static const uint8_t zero_length_argument[] = {
                PLY_BOOT_PROTOCOL_REQUEST_TYPE_UPDATE[0], 0x02, 0x00,
        };
        static const uint8_t unterminated_argument[] = {
                PLY_BOOT_PROTOCOL_REQUEST_TYPE_UPDATE[0],
                0x02, 0x03, 'b', 'a', 'd',
        };
        static const uint8_t valid_request[] = {
                PLY_BOOT_PROTOCOL_REQUEST_TYPE_UPDATE[0],
                0x02, 0x03, 'o', 'k', 0x00,
        };

        PLY_TEST_ASSERT (run_rejected_frame (truncated_header,
                                             sizeof(truncated_header),
                                             true));
        PLY_TEST_ASSERT (run_rejected_frame (invalid_argument_marker,
                                             sizeof(invalid_argument_marker),
                                             true));
        PLY_TEST_ASSERT (run_rejected_frame (zero_length_argument,
                                             sizeof(zero_length_argument),
                                             true));
        PLY_TEST_ASSERT (run_rejected_frame (unterminated_argument,
                                             sizeof(unterminated_argument),
                                             true));
        PLY_TEST_ASSERT (run_rejected_frame (valid_request,
                                             sizeof(valid_request),
                                             false));
        return true;
}

static const ply_test_case_t test_cases[] =
{
        PLY_TEST_CASE (test_update_argument_dispatches_and_acknowledges),
        PLY_TEST_CASE (test_pipelined_notifications_each_receive_ack),
        PLY_TEST_CASE (test_unknown_command_naks_without_breaking_pipeline),
        PLY_TEST_CASE (test_non_root_request_is_rejected),
        PLY_TEST_CASE (test_system_update_parses_valid_and_invalid_values),
        PLY_TEST_CASE (test_immediate_notifications_dispatch_and_acknowledge),
        PLY_TEST_CASE (test_active_vt_response_follows_handler),
        PLY_TEST_CASE (test_question_trigger_sends_answer_payload),
        PLY_TEST_CASE (test_malformed_and_uncredentialed_frames_disconnect),
};

PLY_TEST_MAIN (test_cases)
