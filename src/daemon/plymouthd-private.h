/* plymouthd-private.h - internal daemon application
 *
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#ifndef PLYMOUTHD_PRIVATE_H
#define PLYMOUTHD_PRIVATE_H

#include "ply-private.h"
#include "ply-boot-server.h"

typedef struct _plymouthd plymouthd_t;
typedef struct _plymouthd_options plymouthd_options_t;

PLY_PRIVATE plymouthd_t *plymouthd_new (plymouthd_options_t *options,
                                        char                *program_name,
                                        int                 *exit_code);
PLY_PRIVATE int plymouthd_run (plymouthd_t *daemon);
PLY_PRIVATE void plymouthd_free (plymouthd_t *daemon);

PLY_PRIVATE void plymouthd_handle_update (plymouthd_t *daemon,
                                          const char  *status);
PLY_PRIVATE void plymouthd_handle_change_mode (plymouthd_t *daemon,
                                               const char  *mode);
PLY_PRIVATE void plymouthd_handle_system_update (plymouthd_t *daemon,
                                                 int          progress);
PLY_PRIVATE void plymouthd_handle_ask_for_password (plymouthd_t           *daemon,
                                                    const char            *prompt,
                                                    ply_trigger_t         *answer,
                                                    ply_boot_connection_t *connection);
PLY_PRIVATE void plymouthd_handle_ask_question (plymouthd_t           *daemon,
                                                const char            *prompt,
                                                ply_trigger_t         *answer,
                                                ply_boot_connection_t *connection);
PLY_PRIVATE void plymouthd_handle_display_message (plymouthd_t *daemon,
                                                   const char  *message);
PLY_PRIVATE void plymouthd_handle_hide_message (plymouthd_t *daemon,
                                                const char  *message);
PLY_PRIVATE void plymouthd_handle_watch_for_keystroke (plymouthd_t           *daemon,
                                                       const char            *keys,
                                                       ply_trigger_t         *trigger,
                                                       ply_boot_connection_t *connection);
PLY_PRIVATE void plymouthd_handle_ignore_keystroke (plymouthd_t *daemon,
                                                    const char  *keys);
PLY_PRIVATE void plymouthd_handle_connection_hangup (plymouthd_t           *daemon,
                                                     ply_boot_connection_t *connection);
PLY_PRIVATE void plymouthd_handle_progress_pause (plymouthd_t *daemon);
PLY_PRIVATE void plymouthd_handle_progress_unpause (plymouthd_t *daemon);
PLY_PRIVATE void plymouthd_handle_newroot (plymouthd_t *daemon,
                                           const char  *root_dir);
PLY_PRIVATE void plymouthd_handle_system_initialized (plymouthd_t *daemon);
PLY_PRIVATE void plymouthd_handle_error (plymouthd_t *daemon);
PLY_PRIVATE void plymouthd_handle_reload (plymouthd_t *daemon);
PLY_PRIVATE void plymouthd_handle_show_splash (plymouthd_t *daemon);
PLY_PRIVATE void plymouthd_handle_hide_splash (plymouthd_t *daemon);
PLY_PRIVATE void plymouthd_handle_deactivate (plymouthd_t   *daemon,
                                              ply_trigger_t *trigger);
PLY_PRIVATE void plymouthd_handle_reactivate (plymouthd_t *daemon);
PLY_PRIVATE void plymouthd_handle_quit (plymouthd_t   *daemon,
                                        bool           retain_splash,
                                        ply_trigger_t *trigger);
PLY_PRIVATE bool plymouthd_handle_has_active_vt (plymouthd_t *daemon);

#endif /* PLYMOUTHD_PRIVATE_H */
