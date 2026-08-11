/* plymouthd-options-private.h - internal daemon startup options
 *
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#ifndef PLYMOUTHD_OPTIONS_PRIVATE_H
#define PLYMOUTHD_OPTIONS_PRIVATE_H

#include <stdbool.h>

#include "ply-boot-splash-plugin.h"
#include "ply-event-loop.h"
#include "ply-private.h"

typedef struct _plymouthd_options plymouthd_options_t;

PLY_PRIVATE plymouthd_options_t *plymouthd_options_new (void);
PLY_PRIVATE void plymouthd_options_free (plymouthd_options_t *options);
PLY_PRIVATE bool plymouthd_options_parse (plymouthd_options_t *options,
                                          ply_event_loop_t    *loop,
                                          char               **argv,
                                          int                  argc);
PLY_PRIVATE char *plymouthd_options_get_help_string (plymouthd_options_t *options);

PLY_PRIVATE bool plymouthd_options_should_help (plymouthd_options_t *options);
PLY_PRIVATE bool plymouthd_options_should_attach_to_session (plymouthd_options_t *options);
PLY_PRIVATE bool plymouthd_options_should_daemonize (plymouthd_options_t *options);
PLY_PRIVATE bool plymouthd_options_should_debug (plymouthd_options_t *options);
PLY_PRIVATE bool plymouthd_options_should_ignore_serial_consoles (plymouthd_options_t *options);
PLY_PRIVATE bool plymouthd_options_should_use_graphical_boot (plymouthd_options_t *options);
PLY_PRIVATE bool plymouthd_options_should_log_boot (plymouthd_options_t *options);
PLY_PRIVATE ply_boot_splash_mode_t plymouthd_options_get_mode (plymouthd_options_t *options);
PLY_PRIVATE const char *plymouthd_options_get_debug_path (plymouthd_options_t *options);
PLY_PRIVATE const char *plymouthd_options_get_boot_log_path (plymouthd_options_t *options);
PLY_PRIVATE const char *plymouthd_options_get_kernel_command_line (plymouthd_options_t *options);
PLY_PRIVATE const char *plymouthd_options_get_tty (plymouthd_options_t *options);
PLY_PRIVATE char *plymouthd_options_take_pid_file (plymouthd_options_t *options);

#endif /* PLYMOUTHD_OPTIONS_PRIVATE_H */
