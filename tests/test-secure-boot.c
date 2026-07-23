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

#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#include "ply-secure-boot-private.h"

static bool
check_fixture (const uint8_t *bytes,
               size_t         size,
               bool           expected_enabled)
{
        char path[] = "/tmp/plymouth-secure-boot-test-XXXXXX";
        bool enabled;
        int fd;

        fd = mkstemp (path);
        if (fd < 0)
                return false;

        if (write (fd, bytes, size) != (ssize_t) size) {
                close (fd);
                unlink (path);
                return false;
        }

        close (fd);
        enabled = ply_secure_boot_enabled_at_path (path);
        return unlink (path) == 0 && enabled == expected_enabled;
}

static bool
test_enabled_value_is_accepted (void)
{
        static const uint8_t bytes[] = { 0x07, 0x00, 0x00, 0x00, 0x01 };

        return check_fixture (bytes, sizeof(bytes), true);
}

static bool
test_disabled_and_unknown_values_are_rejected (void)
{
        static const uint8_t disabled[] = { 0x07, 0x00, 0x00, 0x00, 0x00 };
        static const uint8_t unknown[] = { 0x07, 0x00, 0x00, 0x00, 0x02 };

        PLY_TEST_ASSERT (check_fixture (disabled, sizeof(disabled), false));
        PLY_TEST_ASSERT (check_fixture (unknown, sizeof(unknown), false));
        return true;
}

static bool
test_short_and_long_values_are_rejected (void)
{
        static const uint8_t short_value[] = { 0x07, 0x00, 0x00, 0x00 };
        static const uint8_t long_value[] = {
                0x07, 0x00, 0x00, 0x00, 0x01, 0xff,
        };

        PLY_TEST_ASSERT (check_fixture (short_value,
                                        sizeof(short_value),
                                        false));
        PLY_TEST_ASSERT (check_fixture (long_value,
                                        sizeof(long_value),
                                        false));
        return true;
}

static bool
test_missing_value_is_rejected (void)
{
        char path[] = "/tmp/plymouth-secure-boot-test-XXXXXX";
        int fd;

        fd = mkstemp (path);
        PLY_TEST_ASSERT (fd >= 0);
        close (fd);
        PLY_TEST_ASSERT (unlink (path) == 0);

        PLY_TEST_ASSERT (!ply_secure_boot_enabled_at_path (path));
        return true;
}

static const ply_test_case_t test_cases[] =
{
        PLY_TEST_CASE (test_enabled_value_is_accepted),
        PLY_TEST_CASE (test_disabled_and_unknown_values_are_rejected),
        PLY_TEST_CASE (test_short_and_long_values_are_rejected),
        PLY_TEST_CASE (test_missing_value_is_rejected),
};

PLY_TEST_MAIN (test_cases)
