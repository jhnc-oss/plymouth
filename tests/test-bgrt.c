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

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ply-bgrt-private.h"

static bool
write_metadata (const char *directory,
                const char *name,
                const char *contents)
{
        char *path = NULL;
        size_t size;
        bool written;
        int fd;

        asprintf (&path, "%s/%s", directory, name);
        fd = open (path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
        free (path);
        if (fd < 0)
                return false;

        size = strlen (contents);
        written = write (fd, contents, size) == (ssize_t) size;
        close (fd);
        return written;
}

static void
remove_metadata_directory (const char *directory)
{
        static const char *const names[] = {
                "status",
                "xoffset",
                "yoffset",
        };
        size_t i;

        for (i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
                char *path = NULL;

                asprintf (&path, "%s/%s", directory, names[i]);
                unlink (path);
                free (path);
        }

        rmdir (directory);
}

static bool
test_offsets_and_orientations_load (void)
{
        static const struct
        {
                const char                 *status;
                ply_pixel_buffer_rotation_t rotation;
        } cases[] = {
                { "1\n", PLY_PIXEL_BUFFER_ROTATE_UPRIGHT           },
                { "3\n", PLY_PIXEL_BUFFER_ROTATE_COUNTER_CLOCKWISE },
                { "5\n", PLY_PIXEL_BUFFER_ROTATE_UPSIDE_DOWN       },
                { "7\n", PLY_PIXEL_BUFFER_ROTATE_CLOCKWISE         },
        };
        char directory[] = "/tmp/plymouth-bgrt-test-XXXXXX";
        size_t i;

        PLY_TEST_ASSERT (mkdtemp (directory) != NULL);
        PLY_TEST_ASSERT (write_metadata (directory, "xoffset", " 120\n"));
        PLY_TEST_ASSERT (write_metadata (directory, "yoffset", "-9\t"));

        for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
                ply_bgrt_info_t info;

                PLY_TEST_ASSERT (write_metadata (directory,
                                                 "status",
                                                 cases[i].status));
                PLY_TEST_ASSERT (ply_bgrt_info_load (directory, &info));
                PLY_TEST_ASSERT (info.x_offset == 120);
                PLY_TEST_ASSERT (info.y_offset == -9);
                PLY_TEST_ASSERT (info.rotation == cases[i].rotation);
        }

        remove_metadata_directory (directory);
        return true;
}

static bool
test_malformed_integer_is_rejected (void)
{
        char directory[] = "/tmp/plymouth-bgrt-test-XXXXXX";
        ply_bgrt_info_t info;

        PLY_TEST_ASSERT (mkdtemp (directory) != NULL);
        PLY_TEST_ASSERT (write_metadata (directory, "status", "1 extra\n"));
        PLY_TEST_ASSERT (write_metadata (directory, "xoffset", "10\n"));
        PLY_TEST_ASSERT (write_metadata (directory, "yoffset", "20\n"));
        PLY_TEST_ASSERT (!ply_bgrt_info_load (directory, &info));

        PLY_TEST_ASSERT (write_metadata (directory,
                                         "status",
                                         "999999999999999999999\n"));
        PLY_TEST_ASSERT (!ply_bgrt_info_load (directory, &info));

        remove_metadata_directory (directory);
        return true;
}

static bool
test_missing_metadata_is_rejected (void)
{
        char directory[] = "/tmp/plymouth-bgrt-test-XXXXXX";
        ply_bgrt_info_t info;

        PLY_TEST_ASSERT (mkdtemp (directory) != NULL);
        PLY_TEST_ASSERT (write_metadata (directory, "status", "1\n"));
        PLY_TEST_ASSERT (write_metadata (directory, "xoffset", "10\n"));
        PLY_TEST_ASSERT (!ply_bgrt_info_load (directory, &info));

        remove_metadata_directory (directory);
        PLY_TEST_ASSERT (!ply_bgrt_info_load (directory, &info));
        return true;
}

static const ply_test_case_t test_cases[] =
{
        PLY_TEST_CASE (test_offsets_and_orientations_load),
        PLY_TEST_CASE (test_malformed_integer_is_rejected),
        PLY_TEST_CASE (test_missing_metadata_is_rejected),
};

PLY_TEST_MAIN (test_cases)
