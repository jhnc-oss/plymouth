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

#include <string.h>

#include "ply-terminal-emulator.h"

#define FUZZ_ITERATION_COUNT 2048
#define MAX_FUZZ_INPUT_SIZE 256

typedef struct
{
        const uint8_t *bytes;
        size_t         size;
} terminal_input_example_t;

static const uint8_t empty_input[] = "";
static const uint8_t plain_input[] = "plain text\nsecond line\rchanged";
static const uint8_t styled_input[] = "\033[31;1mred\033[0m";
static const uint8_t cursor_input[] = "\033[2J\033[1;1Hhome\033[4Cright";
static const uint8_t malformed_input[] = "\033[3?1mN\033[32mG";
static const uint8_t incomplete_input[] = "\033[31;";
static const uint8_t utf8_input[] = "\342\230\203\342\230";
static const uint8_t control_input[] = "\0\a\b\t\n\r\033";

#define TERMINAL_INPUT_EXAMPLE(input) { input, sizeof(input) - 1 }

static const terminal_input_example_t terminal_input_examples[] = {
        TERMINAL_INPUT_EXAMPLE (empty_input),
        TERMINAL_INPUT_EXAMPLE (plain_input),
        TERMINAL_INPUT_EXAMPLE (styled_input),
        TERMINAL_INPUT_EXAMPLE (cursor_input),
        TERMINAL_INPUT_EXAMPLE (malformed_input),
        TERMINAL_INPUT_EXAMPLE (incomplete_input),
        TERMINAL_INPUT_EXAMPLE (utf8_input),
        TERMINAL_INPUT_EXAMPLE (control_input),
};

static size_t
generate_input (ply_test_seeded_random_t *random,
                uint8_t                  *bytes)
{
        const terminal_input_example_t *example;
        size_t input_size;
        size_t copied_size;
        size_t mutation_count;
        size_t i;

        if (ply_test_seeded_random_range (random, 3) == 0) {
                input_size = ply_test_seeded_random_range (
                        random,
                        MAX_FUZZ_INPUT_SIZE + 1);
                ply_test_seeded_random_fill (random, bytes, input_size);
                return input_size;
        }

        example = &terminal_input_examples[
                ply_test_seeded_random_range (
                        random,
                        sizeof(terminal_input_examples) /
                        sizeof(terminal_input_examples[0]))];

        if (ply_test_seeded_random_range (random, 2) == 0)
                input_size = example->size;
        else
                input_size = ply_test_seeded_random_range (
                        random,
                        MAX_FUZZ_INPUT_SIZE + 1);

        copied_size = input_size < example->size ? input_size : example->size;
        memcpy (bytes, example->bytes, copied_size);
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

static bool
styles_match (const ply_rich_text_character_style_t *first,
              const ply_rich_text_character_style_t *second)
{
        return first->foreground_color == second->foreground_color &&
               first->background_color == second->background_color &&
               first->bold_enabled == second->bold_enabled &&
               first->dim_enabled == second->dim_enabled &&
               first->italic_enabled == second->italic_enabled &&
               first->underline_enabled == second->underline_enabled &&
               first->reverse_enabled == second->reverse_enabled;
}

static bool
lines_match (ply_rich_text_t *first,
             ply_rich_text_t *second)
{
        ply_rich_text_character_t **first_characters;
        ply_rich_text_character_t **second_characters;
        size_t length;
        size_t i;

        length = ply_rich_text_get_length (first);
        if (length != ply_rich_text_get_length (second))
                return false;

        first_characters = ply_rich_text_get_characters (first);
        second_characters = ply_rich_text_get_characters (second);

        for (i = 0; i < length; i++) {
                if (first_characters[i] == NULL ||
                    second_characters[i] == NULL) {
                        if (first_characters[i] != second_characters[i])
                                return false;

                        continue;
                }

                if (first_characters[i]->length !=
                    second_characters[i]->length ||
                    memcmp (first_characters[i]->bytes,
                            second_characters[i]->bytes,
                            first_characters[i]->length) != 0 ||
                    !styles_match (&first_characters[i]->style,
                                   &second_characters[i]->style))
                        return false;
        }

        return true;
}

static bool
terminal_emulators_match (ply_terminal_emulator_t *first,
                          ply_terminal_emulator_t *second)
{
        int line_count;
        int line_number;

        line_count = ply_terminal_emulator_get_line_count (first);
        if (line_count < 1 ||
            line_count != ply_terminal_emulator_get_line_count (second))
                return false;

        for (line_number = 0; line_number < line_count; line_number++) {
                ply_rich_text_t *first_line;
                ply_rich_text_t *second_line;

                first_line = ply_terminal_emulator_get_nth_line (first,
                                                                 line_number);
                second_line = ply_terminal_emulator_get_nth_line (second,
                                                                  line_number);
                if (first_line == NULL || second_line == NULL ||
                    !lines_match (first_line, second_line))
                        return false;
        }

        return true;
}

static void
parse_in_chunks (ply_terminal_emulator_t  *terminal_emulator,
                 ply_test_seeded_random_t *random,
                 const uint8_t            *bytes,
                 size_t                    size)
{
        size_t offset = 0;

        while (offset < size) {
                size_t chunk_size;

                chunk_size = ply_test_seeded_random_range (random,
                                                           size - offset) + 1;
                ply_terminal_emulator_parse_lines (
                        terminal_emulator,
                        (const char *) bytes + offset,
                        chunk_size);
                offset += chunk_size;
        }
}

static bool
test_generated_streams_are_independent_of_chunking (void)
{
        ply_test_seeded_random_t random = {
                .state = UINT32_C (0x4ef1842b),
        };
        uint8_t input[MAX_FUZZ_INPUT_SIZE];
        size_t iteration;

        for (iteration = 0; iteration < FUZZ_ITERATION_COUNT; iteration++) {
                ply_terminal_emulator_t *whole_terminal;
                ply_terminal_emulator_t *chunked_terminal;
                size_t input_size;
                size_t rows;
                size_t columns;
                bool matched;

                input_size = generate_input (&random, input);
                rows = ply_test_seeded_random_range (&random, 8) + 1;
                columns = ply_test_seeded_random_range (&random, 32) + 1;

                whole_terminal = ply_terminal_emulator_new (rows, columns);
                chunked_terminal = ply_terminal_emulator_new (rows, columns);

                ply_terminal_emulator_parse_lines (
                        whole_terminal,
                        (const char *) input,
                        input_size);
                parse_in_chunks (chunked_terminal,
                                 &random,
                                 input,
                                 input_size);
                matched = terminal_emulators_match (whole_terminal,
                                                    chunked_terminal);

                ply_terminal_emulator_free (whole_terminal);
                ply_terminal_emulator_free (chunked_terminal);

                PLY_TEST_ASSERT (matched);
        }

        return true;
}

static const ply_test_case_t test_cases[] =
{
        PLY_TEST_CASE (test_generated_streams_are_independent_of_chunking),
};

PLY_TEST_MAIN (test_cases)
