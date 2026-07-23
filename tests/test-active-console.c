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

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ply-active-console-private.h"

typedef bool (*fixture_check_t) (const char *path);

static bool
with_fixture (const char      *contents,
              fixture_check_t check)
{
        char path[] = "/tmp/plymouth-active-console-test-XXXXXX";
        size_t contents_length;
        bool passed;
        int fd;

        fd = mkstemp (path);
        if (fd < 0)
                return false;

        contents_length = strlen (contents);
        if (write (fd, contents, contents_length) != (ssize_t) contents_length) {
                close (fd);
                unlink (path);
                return false;
        }

        close (fd);
        passed = check (path);
        return unlink (path) == 0 && passed;
}

static bool
fixture_splits_console_names (const char *path)
{
        ply_active_console_list_t consoles;

        PLY_TEST_ASSERT (ply_active_console_list_load (path, &consoles));
        PLY_TEST_ASSERT (consoles.count == 4);
        PLY_TEST_ASSERT (strcmp (consoles.names[0], "tty0") == 0);
        PLY_TEST_ASSERT (strcmp (consoles.names[1], "ttyS0") == 0);
        PLY_TEST_ASSERT (strcmp (consoles.names[2], "hvc0") == 0);
        PLY_TEST_ASSERT (strcmp (consoles.names[3], "ttyAMA0") == 0);

        ply_active_console_list_clear (&consoles);
        return true;
}

static bool
test_whitespace_separates_console_names (void)
{
        return with_fixture ("  tty0 ttyS0\nhvc0\tttyAMA0\v",
                             fixture_splits_console_names);
}

static bool
fixture_keeps_final_name (const char *path)
{
        ply_active_console_list_t consoles;

        PLY_TEST_ASSERT (ply_active_console_list_load (path, &consoles));
        PLY_TEST_ASSERT (consoles.count == 1);
        PLY_TEST_ASSERT (strcmp (consoles.names[0], "ttyS1") == 0);

        ply_active_console_list_clear (&consoles);
        return true;
}

static bool
test_final_name_needs_no_trailing_whitespace (void)
{
        return with_fixture ("ttyS1", fixture_keeps_final_name);
}

static bool
fixture_accepts_whitespace_only_file (const char *path)
{
        ply_active_console_list_t consoles;

        PLY_TEST_ASSERT (ply_active_console_list_load (path, &consoles));
        PLY_TEST_ASSERT (consoles.count == 0);
        PLY_TEST_ASSERT (consoles.names == NULL);

        ply_active_console_list_clear (&consoles);
        return true;
}

static bool
test_whitespace_only_file_has_no_consoles (void)
{
        return with_fixture (" \n\t\v", fixture_accepts_whitespace_only_file);
}

static bool
test_empty_and_missing_files_are_rejected (void)
{
        char path[] = "/tmp/plymouth-active-console-test-XXXXXX";
        ply_active_console_list_t consoles;
        int fd;

        fd = mkstemp (path);
        PLY_TEST_ASSERT (fd >= 0);
        close (fd);

        PLY_TEST_ASSERT (!ply_active_console_list_load (path, &consoles));
        PLY_TEST_ASSERT (consoles.count == 0);
        PLY_TEST_ASSERT (consoles.names == NULL);
        PLY_TEST_ASSERT (unlink (path) == 0);

        PLY_TEST_ASSERT (!ply_active_console_list_load (path, &consoles));
        PLY_TEST_ASSERT (consoles.count == 0);
        PLY_TEST_ASSERT (consoles.names == NULL);
        return true;
}

static const ply_test_case_t test_cases[] =
{
        PLY_TEST_CASE (test_whitespace_separates_console_names),
        PLY_TEST_CASE (test_final_name_needs_no_trailing_whitespace),
        PLY_TEST_CASE (test_whitespace_only_file_has_no_consoles),
        PLY_TEST_CASE (test_empty_and_missing_files_are_rejected),
};

PLY_TEST_MAIN (test_cases)
