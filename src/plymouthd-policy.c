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

#include <stdlib.h>
#include <string.h>

#include "ply-logger.h"
#include "ply-utils.h"

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

bool
plymouthd_should_ignore_show_splash_calls (ply_boot_splash_mode_t mode)
{
        ply_trace ("checking if plymouth should be running");
        if (mode != PLY_BOOT_SPLASH_MODE_BOOT_UP ||
            ply_kernel_command_line_has_argument ("plymouth.force-splash"))
                return false;

        return ply_kernel_command_line_has_argument ("plymouth.ignore-show-splash");
}

bool
plymouthd_shell_is_init (void)
{
        char *init_string;
        bool result = false;
        size_t length;

        init_string = ply_kernel_command_line_get_key_value ("init=");
        if (init_string != NULL) {
                length = strlen (init_string);
                if (length > 2 && init_string[length - 2] == 's' &&
                    init_string[length - 1] == 'h')
                        result = true;

                free (init_string);
        }

        return result;
}

bool
plymouthd_console_type_is_virtual (const char *console_type)
{
        return strcmp (console_type, "ttynull") == 0;
}

bool
plymouthd_kernel_console_is_ttynull (void)
{
        char *kernel_console;

        kernel_console = ply_get_primary_kernel_console_type ();

        /* If the primary console is ttynull, the kernel console is virtual. */
        return plymouthd_console_type_is_virtual (kernel_console);
}

bool
plymouthd_should_show_default_splash (bool force_details,
                                      bool force_default_splash)
{
        static const char * const detail_arguments[] = {
                "single", "1", "s", "S", "-S", NULL
        };
        int i;

        ply_trace ("checking if plymouth should show default splash");

        if (force_details)
                return false;

        for (i = 0; detail_arguments[i] != NULL; i++) {
                if (ply_kernel_command_line_has_argument (detail_arguments[i])) {
                        ply_trace ("no default splash because kernel command line has option \"%s\"",
                                   detail_arguments[i]);
                        return false;
                }
        }

        if (ply_kernel_command_line_has_argument ("splash=verbose")) {
                ply_trace ("no default splash because kernel command line has option \"splash=verbose\"");
                return false;
        }

        if (ply_kernel_command_line_has_argument ("rhgb")) {
                ply_trace ("using default splash because kernel command line has option \"rhgb\"");
                return true;
        }

        if (ply_kernel_command_line_has_argument ("splash")) {
                ply_trace ("using default splash because kernel command line has option \"splash\"");
                return true;
        }

        if (ply_kernel_command_line_has_argument ("splash=silent")) {
                ply_trace ("using default splash because kernel command line has option \"splash=silent\"");
                return true;
        }

        if (force_default_splash) {
                ply_trace ("using default splash because forced by \"plymouth.graphical\" or no active kernel console");
                return true;
        }

        ply_trace ("no default splash because kernel command line lacks \"splash\" or \"rhgb\"");
        return false;
}
