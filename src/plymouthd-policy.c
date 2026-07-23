/* plymouthd-policy.c - internal daemon decisions
 *
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#include "plymouthd-policy-private.h"

#include <string.h>

ply_boot_splash_mode_t
plymouthd_mode_from_string (const char *mode)
{
        if (mode == NULL)
                return PLY_BOOT_SPLASH_MODE_INVALID;

        if (strcmp (mode, "boot-up") == 0)
                return PLY_BOOT_SPLASH_MODE_BOOT_UP;
        if (strcmp (mode, "shutdown") == 0)
                return PLY_BOOT_SPLASH_MODE_SHUTDOWN;
        if (strcmp (mode, "reboot") == 0)
                return PLY_BOOT_SPLASH_MODE_REBOOT;
        if (strcmp (mode, "updates") == 0)
                return PLY_BOOT_SPLASH_MODE_UPDATES;
        if (strcmp (mode, "system-upgrade") == 0)
                return PLY_BOOT_SPLASH_MODE_SYSTEM_UPGRADE;
        if (strcmp (mode, "firmware-upgrade") == 0)
                return PLY_BOOT_SPLASH_MODE_FIRMWARE_UPGRADE;
        if (strcmp (mode, "system-reset") == 0)
                return PLY_BOOT_SPLASH_MODE_SYSTEM_RESET;

        return PLY_BOOT_SPLASH_MODE_INVALID;
}

int
plymouthd_select_simpledrm_config (int  current_setting,
                                   int  direct_setting,
                                   int  setting_without_encryption,
                                   bool uses_disk_encryption)
{
        if (current_setting != -1)
                return current_setting;

        if (direct_setting != -1)
                return direct_setting;

        if (setting_without_encryption != -1 && !uses_disk_encryption)
                return setting_without_encryption;

        return -1;
}

int
plymouthd_select_simpledrm_command_line (int  current_setting,
                                         int  numeric_setting,
                                         bool enable_argument,
                                         bool disable_mode_setting)
{
        if (current_setting != -1)
                return current_setting;

        if (numeric_setting != -1)
                return numeric_setting;

        if (enable_argument)
                return 1;

        if (disable_mode_setting)
                return 2;

        return -1;
}

ply_device_manager_flags_t
plymouthd_add_simpledrm_flags (ply_device_manager_flags_t flags,
                               int                        setting)
{
        if (setting >= 1)
                flags |= PLY_DEVICE_MANAGER_FLAGS_USE_SIMPLEDRM;

        if (setting >= 2)
                flags |= PLY_DEVICE_MANAGER_FLAGS_FORCE_OPEN;

        return flags;
}
