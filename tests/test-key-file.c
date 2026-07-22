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

#include "ply-key-file.h"

typedef struct
{
        bool saw_theme;
        bool saw_enabled;
        bool saw_other;
        int count;
} foreach_result_t;

static bool
write_fixture (const char *contents,
               char       *path)
{
        size_t length;
        int fd;

        strcpy (path, "/tmp/plymouth-key-file-XXXXXX");
        fd = mkstemp (path);
        if (fd < 0)
                return false;

        length = strlen (contents);
        if (write (fd, contents, length) != (ssize_t) length) {
                close (fd);
                unlink (path);
                return false;
        }

        close (fd);
        return true;
}

static void
record_entry (const char *group_name,
              const char *key,
              const char *value,
              void       *user_data)
{
        foreach_result_t *result = user_data;

        result->count++;
        if (strcmp (group_name, "Daemon") == 0 &&
            strcmp (key, "Theme") == 0 &&
            strcmp (value, "spinner") == 0)
                result->saw_theme = true;

        if (strcmp (group_name, "Daemon") == 0 &&
            strcmp (key, "Enabled") == 0 &&
            strcmp (value, "YES") == 0)
                result->saw_enabled = true;

        if (strcmp (group_name, "Other") == 0 &&
            strcmp (key, "Value") == 0 &&
            strcmp (value, "second group") == 0)
                result->saw_other = true;
}

static bool
test_missing_file_fails_to_load (void)
{
        ply_key_file_t *key_file;

        key_file = ply_key_file_new ("/tmp/plymouth-key-file-does-not-exist");

        PLY_TEST_ASSERT (!ply_key_file_load (key_file));
        PLY_TEST_ASSERT (!ply_key_file_has_key (key_file, "Daemon", "Theme"));

        ply_key_file_free (key_file);
        return true;
}

static bool
test_grouped_file_loads_values (void)
{
        static const char contents[] =
                "# leading comment\n"
                "\n"
                "[Daemon]\n"
                "Theme=spinner\n"
                "Enabled=YES\n"
                "\n"
                "[Other]\n"
                "Value=second group\n";
        char path[64];
        ply_key_file_t *key_file;
        char *value;

        PLY_TEST_ASSERT (write_fixture (contents, path));
        key_file = ply_key_file_new (path);

        PLY_TEST_ASSERT (ply_key_file_load (key_file));
        PLY_TEST_ASSERT (ply_key_file_has_key (key_file, "Daemon", "Theme"));
        PLY_TEST_ASSERT (!ply_key_file_has_key (key_file, "Other", "Theme"));

        value = ply_key_file_get_value (key_file, "Daemon", "Theme");
        PLY_TEST_ASSERT (value != NULL);
        PLY_TEST_ASSERT (strcmp (value, "spinner") == 0);
        free (value);

        PLY_TEST_ASSERT (ply_key_file_get_value (key_file,
                                                 "Missing",
                                                 "Theme") == NULL);

        ply_key_file_free (key_file);
        unlink (path);
        return true;
}

static bool
test_duplicate_key_keeps_first_value (void)
{
        static const char contents[] =
                "[Daemon]\n"
                "Theme=spinner\n"
                "Theme=bgrt\n";
        char path[64];
        ply_key_file_t *key_file;
        char *value;

        PLY_TEST_ASSERT (write_fixture (contents, path));
        key_file = ply_key_file_new (path);
        PLY_TEST_ASSERT (ply_key_file_load (key_file));

        value = ply_key_file_get_value (key_file, "Daemon", "Theme");
        PLY_TEST_ASSERT (value != NULL);
        PLY_TEST_ASSERT (strcmp (value, "spinner") == 0);

        free (value);
        ply_key_file_free (key_file);
        unlink (path);
        return true;
}

static bool
test_leading_empty_lines_are_skipped (void)
{
        static const char contents[] =
                "\n"
                "\n"
                "[Daemon]\n"
                "Theme=spinner\n";
        char path[64];
        ply_key_file_t *key_file;
        char *value;

        PLY_TEST_ASSERT (write_fixture (contents, path));
        key_file = ply_key_file_new (path);
        PLY_TEST_ASSERT (ply_key_file_load (key_file));

        value = ply_key_file_get_value (key_file, "Daemon", "Theme");
        PLY_TEST_ASSERT (value != NULL);
        PLY_TEST_ASSERT (strcmp (value, "spinner") == 0);

        free (value);
        ply_key_file_free (key_file);
        unlink (path);
        return true;
}

static bool
test_typed_accessors_handle_defaults (void)
{
        static const char contents[] =
                "[Values]\n"
                "TrueValue=TrUe\n"
                "FalseValue=no\n"
                "Ratio=1.25\n"
                "Count=0x20\n"
                "BadCount=12tail\n";
        char path[64];
        ply_key_file_t *key_file;

        PLY_TEST_ASSERT (write_fixture (contents, path));
        key_file = ply_key_file_new (path);
        PLY_TEST_ASSERT (ply_key_file_load (key_file));

        PLY_TEST_ASSERT (ply_key_file_get_bool (key_file, "Values", "TrueValue"));
        PLY_TEST_ASSERT (!ply_key_file_get_bool (key_file, "Values", "FalseValue"));
        PLY_TEST_ASSERT (!ply_key_file_get_bool (key_file, "Values", "Missing"));
        PLY_TEST_ASSERT (ply_key_file_get_double (key_file,
                                                  "Values",
                                                  "Ratio",
                                                  9.0) == 1.25);
        PLY_TEST_ASSERT (ply_key_file_get_double (key_file,
                                                  "Values",
                                                  "Missing",
                                                  9.0) == 9.0);
        PLY_TEST_ASSERT (ply_key_file_get_ulong (key_file,
                                                 "Values",
                                                 "Count",
                                                 7) == 32);
        PLY_TEST_ASSERT (ply_key_file_get_ulong (key_file,
                                                 "Values",
                                                 "BadCount",
                                                 7) == 7);

        ply_key_file_free (key_file);
        unlink (path);
        return true;
}

static bool
test_groupless_file_uses_null_group (void)
{
        static const char contents[] =
                "# environment style\n"
                "NAME=Plymouth\n"
                "ENABLED=1\n";
        char path[64];
        ply_key_file_t *key_file;
        char *value;

        PLY_TEST_ASSERT (write_fixture (contents, path));
        key_file = ply_key_file_new (path);

        PLY_TEST_ASSERT (ply_key_file_load_groupless_file (key_file));
        PLY_TEST_ASSERT (ply_key_file_has_key (key_file, NULL, "NAME"));
        PLY_TEST_ASSERT (ply_key_file_get_bool (key_file, NULL, "ENABLED"));
        PLY_TEST_ASSERT (!ply_key_file_has_key (key_file, "NONE", "NAME"));

        value = ply_key_file_get_value (key_file, NULL, "NAME");
        PLY_TEST_ASSERT (value != NULL);
        PLY_TEST_ASSERT (strcmp (value, "Plymouth") == 0);
        free (value);

        ply_key_file_free (key_file);
        unlink (path);
        return true;
}

static bool
test_foreach_visits_grouped_entries (void)
{
        static const char contents[] =
                "[Daemon]\n"
                "Theme=spinner\n"
                "Enabled=YES\n"
                "[Other]\n"
                "Value=second group\n";
        foreach_result_t result = { 0 };
        char path[64];
        ply_key_file_t *key_file;

        PLY_TEST_ASSERT (write_fixture (contents, path));
        key_file = ply_key_file_new (path);
        PLY_TEST_ASSERT (ply_key_file_load (key_file));

        ply_key_file_foreach_entry (key_file, record_entry, &result);

        PLY_TEST_ASSERT (result.count == 3);
        PLY_TEST_ASSERT (result.saw_theme);
        PLY_TEST_ASSERT (result.saw_enabled);
        PLY_TEST_ASSERT (result.saw_other);

        ply_key_file_free (key_file);
        unlink (path);
        return true;
}

static const ply_test_case_t test_cases[] =
{
        PLY_TEST_CASE (test_missing_file_fails_to_load),
        PLY_TEST_CASE (test_grouped_file_loads_values),
        PLY_TEST_CASE (test_duplicate_key_keeps_first_value),
        PLY_TEST_CASE (test_leading_empty_lines_are_skipped),
        PLY_TEST_CASE (test_typed_accessors_handle_defaults),
        PLY_TEST_CASE (test_groupless_file_uses_null_group),
        PLY_TEST_CASE (test_foreach_visits_grouped_entries),
};

PLY_TEST_MAIN (test_cases)
