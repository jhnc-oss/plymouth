/* plymouthd-theme-resolver-private.h - internal theme path resolution
 *
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#ifndef PLYMOUTHD_THEME_RESOLVER_PRIVATE_H
#define PLYMOUTHD_THEME_RESOLVER_PRIVATE_H

#include <stdbool.h>
#include <stddef.h>

#include "ply-private.h"

PLY_PRIVATE bool plymouthd_resolve_theme_path (const char *theme_name,
                                               const char *configured_theme_dir,
                                               char      **theme_path);
PLY_PRIVATE bool plymouthd_resolve_theme_path_in_directories (const char  *theme_name,
                                                              const char **directories,
                                                              size_t       number_of_directories,
                                                              char       **theme_path);

#endif /* PLYMOUTHD_THEME_RESOLVER_PRIVATE_H */
