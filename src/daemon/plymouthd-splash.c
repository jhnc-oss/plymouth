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

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

#include "ply-logger.h"
#include "ply-utils.h"
#include "plymouthd-policy-private.h"

struct _plymouthd_splash
{
        ply_boot_splash_t *boot_splash;

        uint32_t           is_shown : 1;
        uint32_t           is_showing_details : 1;
        uint32_t           should_force_details : 1;
        uint32_t           should_force_default : 1;
};

plymouthd_splash_t *
plymouthd_splash_new (void)
{
        return calloc (1, sizeof(plymouthd_splash_t));
}

void
plymouthd_splash_free (plymouthd_splash_t *splash)
{
        if (splash == NULL)
                return;

        plymouthd_splash_clear (splash);
        free (splash);
}

ply_boot_splash_t *
plymouthd_splash_get (const plymouthd_splash_t *splash)
{
        return splash->boot_splash;
}

void
plymouthd_splash_take (plymouthd_splash_t *splash,
                       ply_boot_splash_t  *boot_splash)
{
        assert (splash->boot_splash == NULL);
        assert (boot_splash != NULL);

        splash->boot_splash = boot_splash;
}

void
plymouthd_splash_clear (plymouthd_splash_t *splash)
{
        ply_boot_splash_free (splash->boot_splash);
        splash->boot_splash = NULL;
}

bool
plymouthd_splash_is_shown (const plymouthd_splash_t *splash)
{
        return splash->is_shown;
}

void
plymouthd_splash_set_shown (plymouthd_splash_t *splash,
                            bool                is_shown)
{
        splash->is_shown = is_shown;
}

bool
plymouthd_splash_is_showing_details (const plymouthd_splash_t *splash)
{
        return splash->is_showing_details;
}

void
plymouthd_splash_set_showing_details (plymouthd_splash_t *splash,
                                      bool                is_showing_details)
{
        splash->is_showing_details = is_showing_details;
}

void
plymouthd_splash_force_details (plymouthd_splash_t *splash)
{
        splash->should_force_details = true;
}

void
plymouthd_splash_force_default (plymouthd_splash_t *splash)
{
        splash->should_force_default = true;
}

bool
plymouthd_splash_should_show_default (const plymouthd_splash_t *splash)
{
        return plymouthd_should_show_default_splash (
                splash->should_force_details,
                splash->should_force_default);
}

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
