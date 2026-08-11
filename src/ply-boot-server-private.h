/* ply-boot-server-private.h - internal boot server interfaces
 *
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#ifndef PLY_BOOT_SERVER_PRIVATE_H
#define PLY_BOOT_SERVER_PRIVATE_H

#include "ply-boot-server.h"
#include "ply-private.h"

typedef struct
{
        ply_boot_server_update_handler_t              update;
        ply_boot_server_change_mode_handler_t         change_mode;
        ply_boot_server_system_update_handler_t       system_update;
        ply_boot_server_newroot_handler_t             newroot;
        ply_boot_server_system_initialized_handler_t  system_initialized;
        ply_boot_server_error_handler_t               error;
        ply_boot_server_show_splash_handler_t         show_splash;
        ply_boot_server_hide_splash_handler_t         hide_splash;
        ply_boot_server_ask_for_password_handler_t    ask_for_password;
        ply_boot_server_ask_question_handler_t        ask_question;
        ply_boot_server_display_message_handler_t     display_message;
        ply_boot_server_hide_message_handler_t        hide_message;
        ply_boot_server_watch_for_keystroke_handler_t watch_for_keystroke;
        ply_boot_server_ignore_keystroke_handler_t    ignore_keystroke;
        ply_boot_server_progress_pause_handler_t      progress_pause;
        ply_boot_server_progress_unpause_handler_t    progress_unpause;
        ply_boot_server_deactivate_handler_t          deactivate;
        ply_boot_server_reactivate_handler_t          reactivate;
        ply_boot_server_quit_handler_t                quit;
        ply_boot_server_has_active_vt_handler_t       has_active_vt;
        ply_boot_server_reload_handler_t              reload;
        ply_boot_server_connection_hangup_handler_t   connection_hangup;
} ply_boot_server_handlers_t;

PLY_PRIVATE ply_boot_server_t *
ply_boot_server_new_with_handlers (const ply_boot_server_handlers_t *handlers,
                                   void                             *user_data);

bool ply_boot_server_attach_connection_to_event_loop (ply_boot_server_t *server,
                                                      ply_event_loop_t  *loop,
                                                      int                fd);

#endif /* PLY_BOOT_SERVER_PRIVATE_H */
