/*
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#include "ply-test.h"

#include <string.h>

#include "ply-utils.h"
#include "plymouthd-diagnostics-private.h"

static bool
test_diagnostics_stay_inactive_without_a_request (void)
{
        plymouthd_diagnostics_t *diagnostics;

        ply_kernel_command_line_override ("");
        diagnostics = plymouthd_diagnostics_new (PLY_BOOT_SPLASH_MODE_BOOT_UP,
                                                 "/dev/null",
                                                 NULL,
                                                 false);

        PLY_TEST_ASSERT (!plymouthd_diagnostics_has_buffer (diagnostics));
        PLY_TEST_ASSERT (plymouthd_diagnostics_get_path (diagnostics) == NULL);
        PLY_TEST_ASSERT (plymouthd_diagnostics_get_crash_fd (diagnostics) == -1);

        plymouthd_diagnostics_dump (diagnostics);
        plymouthd_diagnostics_free (diagnostics);
        plymouthd_diagnostics_free (NULL);
        return true;
}

static bool
test_modes_select_established_default_paths (void)
{
        PLY_TEST_ASSERT (strcmp (
                                 plymouthd_get_default_diagnostics_path (
                                         PLY_BOOT_SPLASH_MODE_BOOT_UP),
                                 "/var/log/plymouth-debug.log") == 0);
        PLY_TEST_ASSERT (strcmp (
                                 plymouthd_get_default_diagnostics_path (
                                         PLY_BOOT_SPLASH_MODE_SHUTDOWN),
                                 "/var/log/plymouth-shutdown-debug.log") == 0);
        PLY_TEST_ASSERT (strcmp (
                                 plymouthd_get_default_diagnostics_path (
                                         PLY_BOOT_SPLASH_MODE_REBOOT),
                                 "/var/log/plymouth-shutdown-debug.log") == 0);
        return true;
}

static const ply_test_case_t test_cases[] =
{
        PLY_TEST_CASE (test_diagnostics_stay_inactive_without_a_request),
        PLY_TEST_CASE (test_modes_select_established_default_paths),
};

PLY_TEST_MAIN (test_cases)
