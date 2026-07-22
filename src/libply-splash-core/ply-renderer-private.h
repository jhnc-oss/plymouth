/* ply-renderer-private.h - internal renderer construction
 *
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#ifndef PLY_RENDERER_PRIVATE_H
#define PLY_RENDERER_PRIVATE_H

#include "ply-private.h"
#include "ply-renderer.h"

PLY_PRIVATE ply_renderer_t *
ply_renderer_new_with_plugin_directory (ply_renderer_type_t renderer_type,
                                        const char         *plugin_directory,
                                        const char         *device_name,
                                        ply_terminal_t     *terminal,
                                        ply_terminal_t     *local_console_terminal);

#endif /* PLY_RENDERER_PRIVATE_H */
