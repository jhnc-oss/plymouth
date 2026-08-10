/*
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#include "config.h"
#include "ply-test.h"

#include <string.h>

#include "plymouthd-progress-private.h"

static bool
test_boot_and_shutdown_modes_select_established_caches (void)
{
        plymouthd_progress_t *progress;

        progress = plymouthd_progress_new (PLY_BOOT_SPLASH_MODE_BOOT_UP);
        PLY_TEST_ASSERT (strcmp (plymouthd_progress_get_cache_file (progress),
                                 PLYMOUTH_TIME_DIRECTORY "/boot-duration") == 0);

        plymouthd_progress_set_mode (progress,
                                     PLY_BOOT_SPLASH_MODE_SHUTDOWN);
        PLY_TEST_ASSERT (strcmp (plymouthd_progress_get_cache_file (progress),
                                 PLYMOUTH_TIME_DIRECTORY "/shutdown-duration") == 0);

        plymouthd_progress_set_mode (progress, PLY_BOOT_SPLASH_MODE_REBOOT);
        PLY_TEST_ASSERT (strcmp (plymouthd_progress_get_cache_file (progress),
                                 PLYMOUTH_TIME_DIRECTORY "/shutdown-duration") == 0);

        plymouthd_progress_free (progress);
        return true;
}

static bool
test_update_modes_do_not_use_duration_caches (void)
{
        static const ply_boot_splash_mode_t modes[] = {
                PLY_BOOT_SPLASH_MODE_UPDATES,
                PLY_BOOT_SPLASH_MODE_SYSTEM_UPGRADE,
                PLY_BOOT_SPLASH_MODE_FIRMWARE_UPGRADE,
                PLY_BOOT_SPLASH_MODE_SYSTEM_RESET,
        };
        plymouthd_progress_t *progress;
        size_t i;

        progress = plymouthd_progress_new (modes[0]);

        for (i = 0; i < sizeof(modes) / sizeof(modes[0]); i++) {
                plymouthd_progress_set_mode (progress, modes[i]);
                PLY_TEST_ASSERT (plymouthd_progress_get_cache_file (progress) == NULL);
        }

        plymouthd_progress_free (progress);
        plymouthd_progress_free (NULL);
        return true;
}

static const ply_test_case_t test_cases[] =
{
        PLY_TEST_CASE (test_boot_and_shutdown_modes_select_established_caches),
        PLY_TEST_CASE (test_update_modes_do_not_use_duration_caches),
};

PLY_TEST_MAIN (test_cases)
