/* ply-label-private.h - internal label construction
 *
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#ifndef PLY_LABEL_PRIVATE_H
#define PLY_LABEL_PRIVATE_H

#include "ply-label.h"
#include "ply-private.h"

PLY_PRIVATE ply_label_t *
ply_label_new_with_plugin_directory (const char *plugin_directory);

#endif /* PLY_LABEL_PRIVATE_H */
