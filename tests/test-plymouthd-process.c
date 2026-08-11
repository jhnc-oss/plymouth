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
#include <unistd.h>

#include "ply-utils.h"
#include "plymouthd-process-private.h"

static bool
test_pid_file_follows_process_lifetime (void)
{
        char path[] = "/tmp/plymouthd-process-test-XXXXXX";
        plymouthd_process_t *process;
        int fd;

        fd = mkstemp (path);
        PLY_TEST_ASSERT (fd >= 0);
        close (fd);
        unlink (path);

        ply_kernel_command_line_override ("");
        process = plymouthd_process_new (PLY_BOOT_SPLASH_MODE_BOOT_UP,
                                         "/dev/null",
                                         NULL,
                                         false,
                                         strdup (path));

        plymouthd_process_write_pid_file (process);
        PLY_TEST_ASSERT (access (path, F_OK) == 0);

        plymouthd_process_free (process);
        PLY_TEST_ASSERT (access (path, F_OK) < 0);

        plymouthd_process_free (NULL);
        return true;
}

static const ply_test_case_t test_cases[] =
{
        PLY_TEST_CASE (test_pid_file_follows_process_lifetime),
};

PLY_TEST_MAIN (test_cases)
