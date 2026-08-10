/* plymouthd-theme-resolver.c - internal theme path resolution
 *
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#include "plymouthd-theme-resolver-private.h"

#include <stdlib.h>

#include "ply-logger.h"
#include "ply-utils.h"

bool
plymouthd_resolve_theme_path_in_directories (const char  *theme_name,
                                             const char **directories,
                                             size_t       number_of_directories,
                                             char       **theme_path)
{
        size_t i;

        for (i = 0; i < number_of_directories; ++i) {
                if (directories[i] == NULL)
                        continue;

                asprintf (theme_path,
                          "%s/%s/%s.plymouth",
                          directories[i], theme_name, theme_name);
                if (ply_file_exists (*theme_path)) {
                        ply_trace ("Theme is %s", *theme_path);
                        return true;
                }
                ply_trace ("Theme %s not found", *theme_path);
                free (*theme_path);
                *theme_path = NULL;
        }

        return false;
}

bool
plymouthd_resolve_theme_path (const char *theme_name,
                              const char *configured_theme_dir,
                              char      **theme_path)
{
        const char *directories[] = {
                PLYMOUTH_RUNTIME_THEME_PATH,
                configured_theme_dir,
                PLYMOUTH_THEME_PATH,
        };

        return plymouthd_resolve_theme_path_in_directories (
                theme_name,
                directories,
                sizeof(directories) / sizeof(directories[0]),
                theme_path);
}
