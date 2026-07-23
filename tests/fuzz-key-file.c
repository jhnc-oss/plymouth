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
#include "ply-test-seeded-random.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ply-key-file.h"

#define FUZZ_ITERATION_COUNT 512
#define MAX_FUZZ_INPUT_SIZE 512

static const char *key_file_examples[] = {
        "",
        "# comment\n",
        "[Daemon]\nTheme=spinner\nEnabled=true\nCount=42\n",
        "[Daemon]\nTheme=spinner\n[Other]\nValue=second group\n",
        "NAME=Plymouth\nENABLED=1\nCOUNT=0x20\n",
        "[unfinished",
        "[Group]\nkey without a value\n",
};

static bool
write_all (int            fd,
           const uint8_t *bytes,
           size_t         size)
{
        size_t offset = 0;

        while (offset < size) {
                ssize_t bytes_written;

                bytes_written = write (fd, bytes + offset, size - offset);
                if (bytes_written < 0) {
                        if (errno == EINTR)
                                continue;

                        return false;
                }

                if (bytes_written == 0)
                        return false;

                offset += (size_t) bytes_written;
        }

        return true;
}

static bool
write_input (const char    *path,
             const uint8_t *bytes,
             size_t         size)
{
        bool written;
        int fd;

        fd = open (path, O_WRONLY | O_TRUNC | O_CLOEXEC);
        if (fd < 0)
                return false;

        written = write_all (fd, bytes, size);
        if (close (fd) < 0)
                written = false;

        return written;
}

static size_t
generate_input (ply_test_seeded_random_t *random,
                uint8_t                  *bytes)
{
        const char *example;
        size_t example_size;
        size_t input_size;
        size_t mutation_count;
        size_t copied_size;
        size_t i;

        if (ply_test_seeded_random_range (random, 3) == 0) {
                input_size = ply_test_seeded_random_range (
                        random,
                        MAX_FUZZ_INPUT_SIZE + 1);
                ply_test_seeded_random_fill (random, bytes, input_size);
                return input_size;
        }

        example = key_file_examples[
                ply_test_seeded_random_range (
                        random,
                        sizeof(key_file_examples) / sizeof(key_file_examples[0]))];
        example_size = strlen (example);

        if (ply_test_seeded_random_range (random, 2) == 0)
                input_size = example_size;
        else
                input_size = ply_test_seeded_random_range (
                        random,
                        MAX_FUZZ_INPUT_SIZE + 1);

        copied_size = input_size < example_size ? input_size : example_size;
        memcpy (bytes, example, copied_size);
        ply_test_seeded_random_fill (random,
                                     bytes + copied_size,
                                     input_size - copied_size);

        mutation_count = ply_test_seeded_random_range (random, 9);
        for (i = 0; i < mutation_count && input_size > 0; i++) {
                bytes[ply_test_seeded_random_range (random, input_size)] =
                        (uint8_t) (ply_test_seeded_random_next (random) >> 24);
        }

        return input_size;
}

static void
count_entry (const char *group_name,
             const char *key,
             const char *value,
             void       *user_data)
{
        size_t *entry_count = user_data;

        (*entry_count)++;
}

static bool
exercise_grouped_file (const char *path,
                       size_t      input_size)
{
        ply_key_file_t *key_file;
        size_t entry_count = 0;
        char *value;

        key_file = ply_key_file_new (path);
        ply_key_file_load (key_file);
        ply_key_file_has_key (key_file, "Daemon", "Theme");
        value = ply_key_file_get_value (key_file, "Daemon", "Theme");
        free (value);
        ply_key_file_get_bool (key_file, "Daemon", "Enabled");
        ply_key_file_get_double (key_file, "Daemon", "Ratio", 1.0);
        ply_key_file_get_ulong (key_file, "Daemon", "Count", 1);
        ply_key_file_foreach_entry (key_file, count_entry, &entry_count);
        ply_key_file_free (key_file);

        return entry_count <= input_size;
}

static void
exercise_groupless_file (const char *path)
{
        ply_key_file_t *key_file;
        char *value;

        key_file = ply_key_file_new (path);
        ply_key_file_load_groupless_file (key_file);
        ply_key_file_has_key (key_file, NULL, "NAME");
        value = ply_key_file_get_value (key_file, NULL, "NAME");
        free (value);
        ply_key_file_get_bool (key_file, NULL, "ENABLED");
        ply_key_file_get_double (key_file, NULL, "RATIO", 1.0);
        ply_key_file_get_ulong (key_file, NULL, "COUNT", 1);
        ply_key_file_free (key_file);
}

static bool
test_generated_key_files_complete_parsing (void)
{
        ply_test_seeded_random_t random = {
                .state = UINT32_C (0x318f64c7),
        };
        uint8_t input[MAX_FUZZ_INPUT_SIZE];
        char path[] = "/tmp/plymouth-key-file-fuzz-XXXXXX";
        size_t iteration;
        bool passed = true;
        int fd;

        fd = mkstemp (path);
        PLY_TEST_ASSERT (fd >= 0);
        PLY_TEST_ASSERT (close (fd) == 0);

        for (iteration = 0; iteration < FUZZ_ITERATION_COUNT; iteration++) {
                size_t input_size;

                input_size = generate_input (&random, input);
                if (!write_input (path, input, input_size) ||
                    !exercise_grouped_file (path, input_size)) {
                        passed = false;
                        break;
                }

                exercise_groupless_file (path);
        }

        if (unlink (path) < 0)
                passed = false;

        return passed;
}

static const ply_test_case_t test_cases[] =
{
        PLY_TEST_CASE (test_generated_key_files_complete_parsing),
};

PLY_TEST_MAIN (test_cases)
