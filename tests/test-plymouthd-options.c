/*
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#include "ply-test.h"

#include <stdlib.h>
#include <string.h>

#include "ply-event-loop.h"
#include "plymouthd-options-private.h"

static bool
test_defaults_preserve_boot_startup (void)
{
        char *arguments[] = { "plymouthd", NULL };
        plymouthd_options_t *options;
        ply_event_loop_t *loop;

        options = plymouthd_options_new ();
        loop = ply_event_loop_new ();

        PLY_TEST_ASSERT (plymouthd_options_parse (options, loop, arguments, 1));
        PLY_TEST_ASSERT (!plymouthd_options_should_help (options));
        PLY_TEST_ASSERT (!plymouthd_options_should_attach_to_session (options));
        PLY_TEST_ASSERT (plymouthd_options_should_daemonize (options));
        PLY_TEST_ASSERT (!plymouthd_options_should_debug (options));
        PLY_TEST_ASSERT (plymouthd_options_should_log_boot (options));
        PLY_TEST_ASSERT (plymouthd_options_get_mode (options) ==
                         PLY_BOOT_SPLASH_MODE_BOOT_UP);
        PLY_TEST_ASSERT (plymouthd_options_get_tty (options) == NULL);

        plymouthd_options_free (options);
        ply_event_loop_free (loop);
        return true;
}

static bool
test_explicit_options_are_owned_together (void)
{
        char *arguments[] = {
                "plymouthd",
                "--attach-to-session",
                "--no-daemon",
                "--debug",
                "--debug-file=/debug.log",
                "--mode=shutdown",
                "--pid-file=/run/test.pid",
                "--kernel-command-line=quiet",
                "--tty=tty9",
                "--no-boot-log",
                "--ignore-serial-consoles",
                "--graphical-boot",
                NULL,
        };
        plymouthd_options_t *options;
        ply_event_loop_t *loop;
        char *pid_file;

        options = plymouthd_options_new ();
        loop = ply_event_loop_new ();

        PLY_TEST_ASSERT (plymouthd_options_parse (options, loop, arguments, 12));
        PLY_TEST_ASSERT (plymouthd_options_should_attach_to_session (options));
        PLY_TEST_ASSERT (!plymouthd_options_should_daemonize (options));
        PLY_TEST_ASSERT (plymouthd_options_should_debug (options));
        PLY_TEST_ASSERT (!plymouthd_options_should_log_boot (options));
        PLY_TEST_ASSERT (plymouthd_options_should_ignore_serial_consoles (options));
        PLY_TEST_ASSERT (plymouthd_options_should_use_graphical_boot (options));
        PLY_TEST_ASSERT (plymouthd_options_get_mode (options) ==
                         PLY_BOOT_SPLASH_MODE_SHUTDOWN);
        PLY_TEST_ASSERT (strcmp (plymouthd_options_get_debug_path (options),
                                 "/debug.log") == 0);
        PLY_TEST_ASSERT (strcmp (
                                 plymouthd_options_get_kernel_command_line (options),
                                 "quiet") == 0);
        PLY_TEST_ASSERT (strcmp (plymouthd_options_get_tty (options), "tty9") == 0);

        pid_file = plymouthd_options_take_pid_file (options);
        PLY_TEST_ASSERT (strcmp (pid_file, "/run/test.pid") == 0);
        free (pid_file);

        plymouthd_options_free (options);
        ply_event_loop_free (loop);
        return true;
}

static bool
test_unknown_mode_keeps_boot_fallback (void)
{
        char *arguments[] = { "plymouthd", "--mode=unknown", NULL };
        plymouthd_options_t *options;
        ply_event_loop_t *loop;

        options = plymouthd_options_new ();
        loop = ply_event_loop_new ();

        PLY_TEST_ASSERT (plymouthd_options_parse (options, loop, arguments, 2));
        PLY_TEST_ASSERT (plymouthd_options_get_mode (options) ==
                         PLY_BOOT_SPLASH_MODE_BOOT_UP);

        plymouthd_options_free (options);
        ply_event_loop_free (loop);
        return true;
}

static const ply_test_case_t test_cases[] =
{
        PLY_TEST_CASE (test_defaults_preserve_boot_startup),
        PLY_TEST_CASE (test_explicit_options_are_owned_together),
        PLY_TEST_CASE (test_unknown_mode_keeps_boot_fallback),
};

PLY_TEST_MAIN (test_cases)
