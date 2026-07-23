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

#include "script-scan.h"

#define FUZZ_ITERATION_COUNT 2048
#define MAX_FUZZ_INPUT_SIZE 512

static const char *script_examples[] = {
        "",
        "name = 42;",
        "value = 3.14159;",
        "text = \"escaped\\nstring\";",
        "# line comment\nnext",
        "/* outer /* nested */ comment */ next",
        "\"unterminated",
        "/* unterminated",
        "9999999999999999999999999999999999999999999999999999999999999999",
};

static size_t
generate_input (ply_test_seeded_random_t *random,
                char                     *input)
{
        const char *example;
        size_t example_size;
        size_t input_size;
        size_t copied_size;
        size_t mutation_count;
        size_t i;

        if (ply_test_seeded_random_range (random, 3) == 0) {
                input_size = ply_test_seeded_random_range (
                        random,
                        MAX_FUZZ_INPUT_SIZE + 1);
                ply_test_seeded_random_fill (random,
                                             (uint8_t *) input,
                                             input_size);
                input[input_size] = '\0';
                return input_size;
        }

        example = script_examples[
                ply_test_seeded_random_range (
                        random,
                        sizeof(script_examples) / sizeof(script_examples[0]))];
        example_size = strlen (example);

        if (ply_test_seeded_random_range (random, 2) == 0)
                input_size = example_size;
        else
                input_size = ply_test_seeded_random_range (
                        random,
                        MAX_FUZZ_INPUT_SIZE + 1);

        copied_size = input_size < example_size ? input_size : example_size;
        memcpy (input, example, copied_size);
        ply_test_seeded_random_fill (random,
                                     (uint8_t *) input + copied_size,
                                     input_size - copied_size);

        mutation_count = ply_test_seeded_random_range (random, 9);
        for (i = 0; i < mutation_count && input_size > 0; i++) {
                input[ply_test_seeded_random_range (random, input_size)] =
                        (char) (ply_test_seeded_random_next (random) >> 24);
        }

        input[input_size] = '\0';
        return input_size;
}

static bool
token_is_valid (const script_scan_token_t *token)
{
        if (token->type < SCRIPT_SCAN_TOKEN_TYPE_EOF ||
            token->type > SCRIPT_SCAN_TOKEN_TYPE_ERROR)
                return false;

        if (token->location.line_index < 1 ||
            token->location.column_index < 0)
                return false;

        switch (token->type) {
        case SCRIPT_SCAN_TOKEN_TYPE_IDENTIFIER:
        case SCRIPT_SCAN_TOKEN_TYPE_STRING:
        case SCRIPT_SCAN_TOKEN_TYPE_ERROR:
                return token->data.string != NULL;
        case SCRIPT_SCAN_TOKEN_TYPE_EMPTY:
        case SCRIPT_SCAN_TOKEN_TYPE_EOF:
        case SCRIPT_SCAN_TOKEN_TYPE_INTEGER:
        case SCRIPT_SCAN_TOKEN_TYPE_FLOAT:
        case SCRIPT_SCAN_TOKEN_TYPE_SYMBOL:
        case SCRIPT_SCAN_TOKEN_TYPE_COMMENT:
                return true;
        }

        return false;
}

static bool
scan_input (const char *input,
            size_t      input_size)
{
        script_scan_t *scan;
        script_scan_token_t *token;
        size_t token_count;
        bool terminated = false;

        scan = script_scan_string (input, "generated.script");
        token = script_scan_get_current_token (scan);

        for (token_count = 0;
             token_count <= input_size + 1;
             token_count++) {
                if (!token_is_valid (token))
                        break;

                if (token->type == SCRIPT_SCAN_TOKEN_TYPE_EOF ||
                    token->type == SCRIPT_SCAN_TOKEN_TYPE_ERROR) {
                        terminated = true;
                        break;
                }

                token = script_scan_get_next_token (scan);
        }

        script_scan_free (scan);
        return terminated;
}

static bool
test_generated_scripts_reach_a_terminal_token (void)
{
        ply_test_seeded_random_t random = {
                .state = UINT32_C (0x73c915e2),
        };
        char input[MAX_FUZZ_INPUT_SIZE + 1];
        size_t iteration;

        for (iteration = 0; iteration < FUZZ_ITERATION_COUNT; iteration++) {
                size_t input_size;

                input_size = generate_input (&random, input);
                PLY_TEST_ASSERT (scan_input (input, input_size));
        }

        return true;
}

static const ply_test_case_t test_cases[] =
{
        PLY_TEST_CASE (test_generated_scripts_reach_a_terminal_token),
};

PLY_TEST_MAIN (test_cases)
