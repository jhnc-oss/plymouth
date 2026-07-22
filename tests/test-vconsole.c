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

#include "ply-vconsole-private.h"

typedef bool (*fixture_check_t) (const char *path);

static bool
with_fixture (const char      *contents,
              fixture_check_t check)
{
        char path[] = "/tmp/plymouth-vconsole-test-XXXXXX";
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
fixture_loads_keyboard_settings (const char *path)
{
        ply_vconsole_settings_t settings;

        PLY_TEST_ASSERT (ply_vconsole_settings_load (path, &settings));
        PLY_TEST_ASSERT (strcmp (settings.keymap, "us") == 0);
        PLY_TEST_ASSERT (strcmp (settings.xkb_layout, "de") == 0);
        PLY_TEST_ASSERT (strcmp (settings.xkb_model, "pc105") == 0);
        PLY_TEST_ASSERT (strcmp (settings.xkb_variant, "nodeadkeys") == 0);
        PLY_TEST_ASSERT (strcmp (settings.xkb_options,
                                "grp:alt_shift_toggle") == 0);

        ply_vconsole_settings_clear (&settings);
        return true;
}

static bool
test_quoted_and_unquoted_settings_load (void)
{
        static const char contents[] =
                "KEYMAP=\"us\"\n"
                "XKBLAYOUT=de\n"
                "XKBMODEL=\"pc105\"\n"
                "XKBVARIANT=nodeadkeys\n"
                "XKBOPTIONS=\"grp:alt_shift_toggle\"\n";

        return with_fixture (contents, fixture_loads_keyboard_settings);
}

static bool
fixture_preserves_unmatched_quote (const char *path)
{
        ply_vconsole_settings_t settings;

        PLY_TEST_ASSERT (ply_vconsole_settings_load (path, &settings));
        PLY_TEST_ASSERT (strcmp (settings.keymap, "\"") == 0);
        PLY_TEST_ASSERT (settings.xkb_layout == NULL);
        PLY_TEST_ASSERT (settings.xkb_model == NULL);
        PLY_TEST_ASSERT (settings.xkb_variant == NULL);
        PLY_TEST_ASSERT (settings.xkb_options == NULL);

        ply_vconsole_settings_clear (&settings);
        return true;
}

static bool
test_missing_values_and_unmatched_quote_are_safe (void)
{
        return with_fixture ("KEYMAP=\"\n", fixture_preserves_unmatched_quote);
}

static bool
test_missing_file_is_rejected (void)
{
        char path[] = "/tmp/plymouth-vconsole-test-XXXXXX";
        ply_vconsole_settings_t settings;
        int fd;

        fd = mkstemp (path);
        PLY_TEST_ASSERT (fd >= 0);
        close (fd);
        PLY_TEST_ASSERT (unlink (path) == 0);

        PLY_TEST_ASSERT (!ply_vconsole_settings_load (path, &settings));
        PLY_TEST_ASSERT (settings.keymap == NULL);
        PLY_TEST_ASSERT (settings.xkb_layout == NULL);
        PLY_TEST_ASSERT (settings.xkb_model == NULL);
        PLY_TEST_ASSERT (settings.xkb_variant == NULL);
        PLY_TEST_ASSERT (settings.xkb_options == NULL);
        return true;
}

static const ply_test_case_t test_cases[] =
{
        PLY_TEST_CASE (test_quoted_and_unquoted_settings_load),
        PLY_TEST_CASE (test_missing_values_and_unmatched_quote_are_safe),
        PLY_TEST_CASE (test_missing_file_is_rejected),
};

PLY_TEST_MAIN (test_cases)
