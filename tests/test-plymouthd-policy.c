/*
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include "ply-test.h"

#include "plymouthd-policy-private.h"

static bool
test_mode_names_map_to_splash_modes (void)
{
        static const struct
        {
                const char            *name;
                ply_boot_splash_mode_t mode;
        } cases[] = {
                { "boot-up",          PLY_BOOT_SPLASH_MODE_BOOT_UP          },
                { "shutdown",         PLY_BOOT_SPLASH_MODE_SHUTDOWN         },
                { "reboot",           PLY_BOOT_SPLASH_MODE_REBOOT           },
                { "updates",          PLY_BOOT_SPLASH_MODE_UPDATES          },
                { "system-upgrade",   PLY_BOOT_SPLASH_MODE_SYSTEM_UPGRADE   },
                { "firmware-upgrade", PLY_BOOT_SPLASH_MODE_FIRMWARE_UPGRADE },
                { "system-reset",     PLY_BOOT_SPLASH_MODE_SYSTEM_RESET     },
        };
        size_t i;

        for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
                PLY_TEST_ASSERT (plymouthd_mode_from_string (cases[i].name) ==
                                 cases[i].mode);
        }

        PLY_TEST_ASSERT (plymouthd_mode_from_string (NULL) ==
                         PLY_BOOT_SPLASH_MODE_INVALID);
        PLY_TEST_ASSERT (plymouthd_mode_from_string ("") ==
                         PLY_BOOT_SPLASH_MODE_INVALID);
        PLY_TEST_ASSERT (plymouthd_mode_from_string ("boot") ==
                         PLY_BOOT_SPLASH_MODE_INVALID);
        PLY_TEST_ASSERT (plymouthd_mode_from_string ("Shutdown") ==
                         PLY_BOOT_SPLASH_MODE_INVALID);
        return true;
}

static bool
test_configuration_precedence_respects_encryption (void)
{
        PLY_TEST_ASSERT (plymouthd_select_simpledrm_config (2,
                                                            0,
                                                            0,
                                                            true) == 2);
        PLY_TEST_ASSERT (plymouthd_select_simpledrm_config (-1,
                                                            0,
                                                            2,
                                                            true) == 0);
        PLY_TEST_ASSERT (plymouthd_select_simpledrm_config (-1,
                                                            1,
                                                            -1,
                                                            true) == 1);
        PLY_TEST_ASSERT (plymouthd_select_simpledrm_config (-1,
                                                            -1,
                                                            1,
                                                            false) == 1);
        PLY_TEST_ASSERT (plymouthd_select_simpledrm_config (-1,
                                                            -1,
                                                            1,
                                                            true) == -1);
        PLY_TEST_ASSERT (plymouthd_select_simpledrm_config (-1,
                                                            -1,
                                                            -1,
                                                            false) == -1);
        return true;
}

static bool
test_kernel_argument_precedence_is_stable (void)
{
        PLY_TEST_ASSERT (plymouthd_select_simpledrm_command_line (0,
                                                                  2,
                                                                  true,
                                                                  true) == 0);
        PLY_TEST_ASSERT (plymouthd_select_simpledrm_command_line (-1,
                                                                  0,
                                                                  true,
                                                                  true) == 0);
        PLY_TEST_ASSERT (plymouthd_select_simpledrm_command_line (-1,
                                                                  -1,
                                                                  true,
                                                                  true) == 1);
        PLY_TEST_ASSERT (plymouthd_select_simpledrm_command_line (-1,
                                                                  -1,
                                                                  false,
                                                                  true) == 2);
        PLY_TEST_ASSERT (plymouthd_select_simpledrm_command_line (-1,
                                                                  -1,
                                                                  false,
                                                                  false) == -1);
        return true;
}

static bool
test_setting_adds_expected_device_flags (void)
{
        ply_device_manager_flags_t existing;
        ply_device_manager_flags_t flags;

        existing = PLY_DEVICE_MANAGER_FLAGS_IGNORE_UDEV;

        flags = plymouthd_add_simpledrm_flags (existing, -1);
        PLY_TEST_ASSERT (flags == existing);

        flags = plymouthd_add_simpledrm_flags (existing, 0);
        PLY_TEST_ASSERT (flags == existing);

        flags = plymouthd_add_simpledrm_flags (existing, 1);
        PLY_TEST_ASSERT (flags ==
                         (existing | PLY_DEVICE_MANAGER_FLAGS_USE_SIMPLEDRM));

        flags = plymouthd_add_simpledrm_flags (existing, 2);
        PLY_TEST_ASSERT (flags ==
                         (existing |
                          PLY_DEVICE_MANAGER_FLAGS_USE_SIMPLEDRM |
                          PLY_DEVICE_MANAGER_FLAGS_FORCE_OPEN));
        return true;
}

static const ply_test_case_t test_cases[] =
{
        PLY_TEST_CASE (test_mode_names_map_to_splash_modes),
        PLY_TEST_CASE (test_configuration_precedence_respects_encryption),
        PLY_TEST_CASE (test_kernel_argument_precedence_is_stable),
        PLY_TEST_CASE (test_setting_adds_expected_device_flags),
};

PLY_TEST_MAIN (test_cases)
