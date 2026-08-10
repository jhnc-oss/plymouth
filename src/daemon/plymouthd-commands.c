/* plymouthd-commands.c - internal daemon command adapter
 *
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#include "plymouthd-commands-private.h"

#include "ply-boot-server-private.h"
#include "ply-utils.h"
#include "plymouthd-private.h"

ply_boot_server_t *
plymouthd_start_commands (ply_event_loop_t *loop,
                          plymouthd_t      *daemon)
{
        const ply_boot_server_handlers_t handlers = {
                .update              = (ply_boot_server_update_handler_t) plymouthd_handle_update,
                .change_mode         = (ply_boot_server_change_mode_handler_t) plymouthd_handle_change_mode,
                .system_update       = (ply_boot_server_system_update_handler_t) plymouthd_handle_system_update,
                .ask_for_password    = (ply_boot_server_ask_for_password_handler_t) plymouthd_handle_ask_for_password,
                .ask_question        = (ply_boot_server_ask_question_handler_t) plymouthd_handle_ask_question,
                .display_message     = (ply_boot_server_display_message_handler_t) plymouthd_handle_display_message,
                .hide_message        = (ply_boot_server_hide_message_handler_t) plymouthd_handle_hide_message,
                .watch_for_keystroke = (ply_boot_server_watch_for_keystroke_handler_t) plymouthd_handle_watch_for_keystroke,
                .ignore_keystroke    = (ply_boot_server_ignore_keystroke_handler_t) plymouthd_handle_ignore_keystroke,
                .progress_pause      = (ply_boot_server_progress_pause_handler_t) plymouthd_handle_progress_pause,
                .progress_unpause    = (ply_boot_server_progress_unpause_handler_t) plymouthd_handle_progress_unpause,
                .show_splash         = (ply_boot_server_show_splash_handler_t) plymouthd_handle_show_splash,
                .hide_splash         = (ply_boot_server_hide_splash_handler_t) plymouthd_handle_hide_splash,
                .newroot             = (ply_boot_server_newroot_handler_t) plymouthd_handle_newroot,
                .system_initialized  = (ply_boot_server_system_initialized_handler_t) plymouthd_handle_system_initialized,
                .error               = (ply_boot_server_error_handler_t) plymouthd_handle_error,
                .deactivate          = (ply_boot_server_deactivate_handler_t) plymouthd_handle_deactivate,
                .reactivate          = (ply_boot_server_reactivate_handler_t) plymouthd_handle_reactivate,
                .quit                = (ply_boot_server_quit_handler_t) plymouthd_handle_quit,
                .has_active_vt       = (ply_boot_server_has_active_vt_handler_t) plymouthd_handle_has_active_vt,
                .reload              = (ply_boot_server_reload_handler_t) plymouthd_handle_reload,
                .connection_hangup   = (ply_boot_server_connection_hangup_handler_t) plymouthd_handle_connection_hangup,
        };
        ply_boot_server_t *server;

        server = ply_boot_server_new_with_handlers (&handlers, daemon);

        if (!ply_boot_server_listen (server)) {
                ply_save_errno ();
                ply_boot_server_free (server);
                ply_restore_errno ();
                return NULL;
        }

        ply_boot_server_attach_to_event_loop (server, loop);
        return server;
}
