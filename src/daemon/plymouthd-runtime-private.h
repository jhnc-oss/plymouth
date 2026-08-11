/* plymouthd-runtime-private.h - daemon runtime callbacks
 *
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#ifndef PLYMOUTHD_RUNTIME_PRIVATE_H
#define PLYMOUTHD_RUNTIME_PRIVATE_H

#include <stdbool.h>
#include <stddef.h>

#include "ply-kmsg-reader.h"
#include "ply-private.h"

typedef struct _plymouthd plymouthd_t;

PLY_PRIVATE bool plymouthd_initialize_session (plymouthd_t *daemon,
                                               bool         should_attach);
PLY_PRIVATE void plymouthd_handle_session_output (plymouthd_t *daemon,
                                                  const char  *output,
                                                  size_t       size);
PLY_PRIVATE void plymouthd_handle_session_hangup (plymouthd_t *daemon);
PLY_PRIVATE void plymouthd_handle_kmsg (plymouthd_t    *daemon,
                                        kmsg_message_t *message);
PLY_PRIVATE bool plymouthd_attach_session (plymouthd_t *daemon);
PLY_PRIVATE void plymouthd_handle_term_signal (plymouthd_t *daemon);

#endif /* PLYMOUTHD_RUNTIME_PRIVATE_H */
