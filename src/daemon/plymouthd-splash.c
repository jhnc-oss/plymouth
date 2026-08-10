/* plymouthd-splash.c - internal splash construction
 *
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#include "plymouthd-splash-private.h"

#include "ply-logger.h"
#include "ply-utils.h"

ply_boot_splash_t *
plymouthd_load_splash (const char       *theme_path,
                       const char       *plugin_directory,
                       ply_buffer_t     *boot_buffer,
                       ply_event_loop_t *loop)
{
        ply_boot_splash_t *splash;
        bool is_loaded;

        if (theme_path == NULL)
                ply_trace ("Loading built-in theme");
        else
                ply_trace ("Loading boot splash theme '%s'", theme_path);

        splash = ply_boot_splash_new (theme_path != NULL ? theme_path : "",
                                      plugin_directory,
                                      boot_buffer);

        if (theme_path == NULL)
                is_loaded = ply_boot_splash_load_built_in (splash);
        else
                is_loaded = ply_boot_splash_load (splash);

        if (!is_loaded) {
                ply_save_errno ();
                ply_boot_splash_free (splash);
                ply_restore_errno ();
                return NULL;
        }

        ply_trace ("attaching plugin to event loop");
        ply_boot_splash_attach_to_event_loop (splash, loop);

        return splash;
}
