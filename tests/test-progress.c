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

#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ply-progress.h"

static double fake_timestamp;

double
__wrap_ply_get_timestamp (void)
{
        return fake_timestamp;
}

static bool
read_file (const char *path,
           char       *contents,
           size_t      capacity)
{
        size_t offset = 0;
        int fd;

        fd = open (path, O_RDONLY | O_CLOEXEC);
        if (fd < 0)
                return false;

        while (offset + 1 < capacity) {
                ssize_t bytes_read;

                bytes_read = read (fd,
                                   contents + offset,
                                   capacity - offset - 1);
                if (bytes_read < 0) {
                        if (errno == EINTR)
                                continue;

                        close (fd);
                        return false;
                }

                if (bytes_read == 0)
                        break;

                offset += (size_t) bytes_read;
        }

        contents[offset] = '\0';
        close (fd);
        return true;
}

static bool
test_elapsed_time_excludes_pauses (void)
{
        ply_progress_t *progress;

        fake_timestamp = 100.0;
        progress = ply_progress_new ();

        PLY_TEST_ASSERT (ply_progress_get_time (progress) == 0.0);

        fake_timestamp = 112.5;
        PLY_TEST_ASSERT (ply_progress_get_time (progress) == 12.5);

        ply_progress_pause (progress);
        fake_timestamp = 140.0;
        PLY_TEST_ASSERT (ply_progress_get_time (progress) == 12.5);

        ply_progress_unpause (progress);
        PLY_TEST_ASSERT (ply_progress_get_time (progress) == 12.5);

        fake_timestamp = 141.0;
        PLY_TEST_ASSERT (ply_progress_get_time (progress) == 13.5);

        ply_progress_free (progress);
        return true;
}

static bool
test_percentage_stays_fixed_while_paused (void)
{
        ply_progress_t *progress;
        double before;
        double after;

        fake_timestamp = 0.0;
        progress = ply_progress_new ();
        fake_timestamp = 10.0;
        ply_progress_pause (progress);

        before = ply_progress_get_percentage (progress);
        fake_timestamp = 50.0;
        after = ply_progress_get_percentage (progress);

        PLY_TEST_ASSERT (fabs (before - (1.0 / 6.0)) < 0.000001);
        PLY_TEST_ASSERT (after == before);

        ply_progress_free (progress);
        return true;
}

static bool
test_percentage_hint_blends_with_elapsed_estimate (void)
{
        ply_progress_t *progress;
        double percentage;

        fake_timestamp = 0.0;
        progress = ply_progress_new ();
        fake_timestamp = 10.0;

        ply_progress_set_percentage (progress, 0.5);
        percentage = ply_progress_get_percentage (progress);

        PLY_TEST_ASSERT (fabs (percentage - (1.0 / 3.0)) < 0.000001);

        ply_progress_free (progress);
        return true;
}

static bool
test_percentage_is_clamped_at_completion (void)
{
        ply_progress_t *progress;

        fake_timestamp = 0.0;
        progress = ply_progress_new ();
        fake_timestamp = 1000.0;

        PLY_TEST_ASSERT (ply_progress_get_percentage (progress) == 1.0);

        fake_timestamp = 2000.0;
        PLY_TEST_ASSERT (ply_progress_get_percentage (progress) == 1.0);

        ply_progress_free (progress);
        return true;
}

static bool
test_cache_round_trip_guides_percentage (void)
{
        char path[] = "/tmp/plymouth-progress-test-XXXXXX";
        char status[161];
        char contents[256];
        ply_progress_t *recording;
        ply_progress_t *replay;
        double percentage;
        int fd;

        memset (status, 's', sizeof(status) - 1);
        status[sizeof(status) - 1] = '\0';

        fd = mkstemp (path);
        PLY_TEST_ASSERT (fd >= 0);
        close (fd);

        fake_timestamp = 0.0;
        recording = ply_progress_new ();
        fake_timestamp = 10.0;
        ply_progress_status_update (recording, status);
        fake_timestamp = 20.0;
        ply_progress_save_cache (recording, path);
        ply_progress_free (recording);

        PLY_TEST_ASSERT (read_file (path, contents, sizeof(contents)));
        PLY_TEST_ASSERT (strncmp (contents, "0.500:", 6) == 0);
        PLY_TEST_ASSERT (strlen (contents) == strlen (status) + 7);
        PLY_TEST_ASSERT (memcmp (contents + 6, status, strlen (status)) == 0);
        PLY_TEST_ASSERT (contents[strlen (contents) - 1] == '\n');

        fake_timestamp = 100.0;
        replay = ply_progress_new ();
        ply_progress_load_cache (replay, path);
        fake_timestamp = 110.0;
        ply_progress_status_update (replay, status);
        percentage = ply_progress_get_percentage (replay);

        PLY_TEST_ASSERT (fabs (percentage - (1.0 / 3.0)) < 0.000001);

        ply_progress_free (replay);
        PLY_TEST_ASSERT (unlink (path) == 0);
        return true;
}

static bool
test_repeated_status_is_excluded_from_cache (void)
{
        char path[] = "/tmp/plymouth-progress-test-XXXXXX";
        char contents[128];
        ply_progress_t *progress;
        int fd;

        fd = mkstemp (path);
        PLY_TEST_ASSERT (fd >= 0);
        close (fd);

        fake_timestamp = 100.0;
        progress = ply_progress_new ();
        fake_timestamp = 110.0;
        ply_progress_status_update (progress, "repeated");
        fake_timestamp = 120.0;
        ply_progress_status_update (progress, "kept");
        fake_timestamp = 125.0;
        ply_progress_status_update (progress, "repeated");
        fake_timestamp = 140.0;
        ply_progress_save_cache (progress, path);

        PLY_TEST_ASSERT (read_file (path, contents, sizeof(contents)));
        PLY_TEST_ASSERT (strcmp (contents, "0.500:kept\n") == 0);

        ply_progress_free (progress);
        PLY_TEST_ASSERT (unlink (path) == 0);
        return true;
}

static const ply_test_case_t test_cases[] =
{
        PLY_TEST_CASE (test_elapsed_time_excludes_pauses),
        PLY_TEST_CASE (test_percentage_stays_fixed_while_paused),
        PLY_TEST_CASE (test_percentage_hint_blends_with_elapsed_estimate),
        PLY_TEST_CASE (test_percentage_is_clamped_at_completion),
        PLY_TEST_CASE (test_cache_round_trip_guides_percentage),
        PLY_TEST_CASE (test_repeated_status_is_excluded_from_cache),
};

PLY_TEST_MAIN (test_cases)
