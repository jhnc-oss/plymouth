/*
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#include "ply-test.h"

#include <paths.h>
#include <string.h>

#include "ply-utils.h"
#include "plymouthd-logging-private.h"

static bool
test_boot_logging_uses_default_paths (void)
{
        plymouthd_logging_t *logging;

        ply_kernel_command_line_override ("");
        logging = plymouthd_logging_new (PLY_BOOT_SPLASH_MODE_BOOT_UP,
                                         NULL,
                                         false);

        PLY_TEST_ASSERT (plymouthd_logging_is_enabled (logging));
        PLY_TEST_ASSERT (strcmp (plymouthd_logging_get_log_file (logging),
                                 "/var/log/boot.log") == 0);
        PLY_TEST_ASSERT (strcmp (plymouthd_logging_get_spool_file (logging),
                                 "/var/spool/plymouth/boot.log") == 0);

        plymouthd_logging_free (logging);
        return true;
}

static bool
test_configured_log_path_precedes_kernel_path (void)
{
        plymouthd_logging_t *logging;

        ply_kernel_command_line_override ("plymouth.boot-log=/kernel.log");
        logging = plymouthd_logging_new (PLY_BOOT_SPLASH_MODE_BOOT_UP,
                                         "/configured.log",
                                         false);

        PLY_TEST_ASSERT (strcmp (plymouthd_logging_get_log_file (logging),
                                 "/configured.log") == 0);

        plymouthd_logging_free (logging);
        return true;
}

static bool
test_kernel_log_path_is_used_as_fallback (void)
{
        plymouthd_logging_t *logging;

        ply_kernel_command_line_override ("plymouth.boot-log=/kernel.log");
        logging = plymouthd_logging_new (PLY_BOOT_SPLASH_MODE_BOOT_UP,
                                         NULL,
                                         false);

        PLY_TEST_ASSERT (strcmp (plymouthd_logging_get_log_file (logging),
                                 "/kernel.log") == 0);

        plymouthd_logging_free (logging);
        return true;
}

static bool
test_disable_options_suppress_boot_logging (void)
{
        plymouthd_logging_t *logging;

        ply_kernel_command_line_override ("");
        logging = plymouthd_logging_new (PLY_BOOT_SPLASH_MODE_BOOT_UP,
                                         NULL,
                                         true);
        PLY_TEST_ASSERT (!plymouthd_logging_is_enabled (logging));
        PLY_TEST_ASSERT (plymouthd_logging_get_log_file (logging) == NULL);
        plymouthd_logging_free (logging);

        ply_kernel_command_line_override ("plymouth.nolog");
        logging = plymouthd_logging_new (PLY_BOOT_SPLASH_MODE_BOOT_UP,
                                         NULL,
                                         false);
        PLY_TEST_ASSERT (!plymouthd_logging_is_enabled (logging));
        PLY_TEST_ASSERT (plymouthd_logging_get_log_file (logging) == NULL);
        plymouthd_logging_free (logging);
        return true;
}

static bool
test_non_boot_modes_log_to_dev_null_without_spooling (void)
{
        plymouthd_logging_t *logging;

        ply_kernel_command_line_override ("");
        logging = plymouthd_logging_new (PLY_BOOT_SPLASH_MODE_SHUTDOWN,
                                         NULL,
                                         false);

        PLY_TEST_ASSERT (strcmp (plymouthd_logging_get_log_file (logging),
                                 _PATH_DEVNULL) == 0);
        PLY_TEST_ASSERT (plymouthd_logging_get_spool_file (logging) == NULL);

        plymouthd_logging_set_mode (logging, PLY_BOOT_SPLASH_MODE_BOOT_UP);
        PLY_TEST_ASSERT (strcmp (plymouthd_logging_get_log_file (logging),
                                 "/var/log/boot.log") == 0);

        plymouthd_logging_free (logging);
        return true;
}

static bool
test_initialization_state_does_not_require_a_session (void)
{
        plymouthd_logging_t *logging;

        ply_kernel_command_line_override ("");
        logging = plymouthd_logging_new (PLY_BOOT_SPLASH_MODE_BOOT_UP,
                                         NULL,
                                         false);

        PLY_TEST_ASSERT (!plymouthd_logging_is_initialized (logging));
        plymouthd_logging_system_initialized (logging, NULL);
        PLY_TEST_ASSERT (plymouthd_logging_is_initialized (logging));

        plymouthd_logging_free (logging);
        return true;
}

static const ply_test_case_t test_cases[] =
{
        PLY_TEST_CASE (test_boot_logging_uses_default_paths),
        PLY_TEST_CASE (test_configured_log_path_precedes_kernel_path),
        PLY_TEST_CASE (test_kernel_log_path_is_used_as_fallback),
        PLY_TEST_CASE (test_disable_options_suppress_boot_logging),
        PLY_TEST_CASE (test_non_boot_modes_log_to_dev_null_without_spooling),
        PLY_TEST_CASE (test_initialization_state_does_not_require_a_session),
};

PLY_TEST_MAIN (test_cases)
