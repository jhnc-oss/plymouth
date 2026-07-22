/* plymouthd-policy-private.h - internal daemon decisions
 *
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#ifndef PLYMOUTHD_POLICY_PRIVATE_H
#define PLYMOUTHD_POLICY_PRIVATE_H

#include <stdbool.h>

#include "ply-boot-splash-plugin.h"
#include "ply-device-manager.h"
#include "ply-private.h"

PLY_PRIVATE ply_boot_splash_mode_t plymouthd_mode_from_string (const char *mode);
PLY_PRIVATE int plymouthd_select_simpledrm_config (int  current_setting,
                                                   int  direct_setting,
                                                   int  setting_without_encryption,
                                                   bool uses_disk_encryption);
PLY_PRIVATE int plymouthd_select_simpledrm_command_line (int  current_setting,
                                                         int  numeric_setting,
                                                         bool enable_argument,
                                                         bool disable_mode_setting);
PLY_PRIVATE ply_device_manager_flags_t
plymouthd_add_simpledrm_flags (ply_device_manager_flags_t flags,
                               int                        setting);

#endif /* PLYMOUTHD_POLICY_PRIVATE_H */
