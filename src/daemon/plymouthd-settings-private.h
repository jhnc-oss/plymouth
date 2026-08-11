/* plymouthd-settings-private.h - internal daemon settings
 *
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#ifndef PLYMOUTHD_SETTINGS_PRIVATE_H
#define PLYMOUTHD_SETTINGS_PRIVATE_H

#include <stdbool.h>

#include <xkbcommon/xkbcommon.h>

#include "ply-private.h"

typedef struct
{
        double       splash_delay;
        double       device_timeout;
        int          device_scale;
        xkb_keysym_t extra_esc_key;
        int          use_simpledrm;

        char        *override_splash_path;
        char        *system_default_splash_path;
        char        *distribution_default_splash_path;
} plymouthd_settings_t;

PLY_PRIVATE void plymouthd_settings_init (plymouthd_settings_t *settings);
PLY_PRIVATE void plymouthd_settings_free (plymouthd_settings_t *settings);
PLY_PRIVATE bool plymouthd_settings_apply_config_file (plymouthd_settings_t *settings,
                                                       const char           *path,
                                                       char                **theme_path);
PLY_PRIVATE void plymouthd_settings_apply_kernel_command_line (plymouthd_settings_t *settings);
PLY_PRIVATE void plymouthd_settings_load (plymouthd_settings_t *settings);
PLY_PRIVATE void plymouthd_settings_reload_theme_paths (plymouthd_settings_t *settings);

#endif /* PLYMOUTHD_SETTINGS_PRIVATE_H */
