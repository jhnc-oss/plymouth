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

#include <stdlib.h>
#include <string.h>

#include "ply-command-parser.h"
#include "ply-event-loop.h"

#define FUZZ_ITERATION_COUNT 2048
#define MAX_FUZZ_ARGUMENT_COUNT 8
#define MAX_FUZZ_ARGUMENT_LENGTH 32

typedef char fuzz_argument_t[MAX_FUZZ_ARGUMENT_LENGTH + 1];

typedef struct
{
        bool  parsed;
        bool  flag;
        bool  flag_was_set;
        bool  enabled;
        bool  enabled_was_set;
        char *name;
        bool  name_was_set;
        int   count;
        bool  count_was_set;
} parse_result_t;

static const char *argument_examples[] = {
        "",
        "value",
        "--",
        "----",
        "--unknown",
        "--flag",
        "--flag=false",
        "--enabled",
        "--enabled=true",
        "--enabled=maybe",
        "--name",
        "--name=",
        "--name=spinner",
        "--count",
        "--count=0",
        "--count=-1",
        "--count=2147483648",
};

static const char argument_alphabet[] =
        "-=+_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

static void
generate_argument (ply_test_seeded_random_t *random,
                   char                     *argument)
{
        size_t length;
        size_t mutation_count;
        size_t i;

        if (ply_test_seeded_random_range (random, 3) != 0) {
                const char *example;

                example = argument_examples[
                        ply_test_seeded_random_range (
                                random,
                                sizeof(argument_examples) / sizeof(argument_examples[0]))];
                length = strlen (example);
                memcpy (argument, example, length + 1);
        } else {
                length = ply_test_seeded_random_range (
                        random,
                        MAX_FUZZ_ARGUMENT_LENGTH + 1);

                for (i = 0; i < length; i++) {
                        argument[i] = argument_alphabet[
                                ply_test_seeded_random_range (
                                        random,
                                        sizeof(argument_alphabet) - 1)];
                }
                argument[length] = '\0';
        }

        mutation_count = ply_test_seeded_random_range (random, 4);
        for (i = 0; i < mutation_count && length > 0; i++) {
                argument[ply_test_seeded_random_range (random, length)] =
                        argument_alphabet[
                                ply_test_seeded_random_range (
                                        random,
                                        sizeof(argument_alphabet) - 1)];
        }
}

static void
parse_arguments (char *const    *arguments,
                 int             argument_count,
                 parse_result_t *result)
{
        ply_command_parser_t *parser;
        ply_event_loop_t *loop;

        memset (result, 0, sizeof(*result));
        parser = ply_command_parser_new ("tool", "Generated argument test");
        loop = ply_event_loop_new ();

        ply_command_parser_add_options (
                parser,
                "flag", "Set a flag", PLY_COMMAND_OPTION_TYPE_FLAG,
                "enabled", "Set a boolean", PLY_COMMAND_OPTION_TYPE_BOOLEAN,
                "name", "Set a name", PLY_COMMAND_OPTION_TYPE_STRING,
                "count", "Set a count", PLY_COMMAND_OPTION_TYPE_INTEGER,
                NULL);

        result->parsed = ply_command_parser_parse_arguments (parser,
                                                             loop,
                                                             arguments,
                                                             argument_count);
        ply_command_parser_get_option (parser,
                                       "flag",
                                       &result->flag,
                                       &result->flag_was_set);
        ply_command_parser_get_option (parser,
                                       "enabled",
                                       &result->enabled,
                                       &result->enabled_was_set);
        ply_command_parser_get_option (parser,
                                       "name",
                                       &result->name,
                                       &result->name_was_set);
        ply_command_parser_get_option (parser,
                                       "count",
                                       &result->count,
                                       &result->count_was_set);

        ply_command_parser_stop_parsing_arguments (parser);
        ply_command_parser_free (parser);
        ply_event_loop_free (loop);
}

static bool
parse_results_match (const parse_result_t *first,
                     const parse_result_t *second)
{
        if (first->parsed != second->parsed ||
            first->flag != second->flag ||
            first->flag_was_set != second->flag_was_set ||
            first->enabled != second->enabled ||
            first->enabled_was_set != second->enabled_was_set ||
            first->name_was_set != second->name_was_set ||
            first->count != second->count ||
            first->count_was_set != second->count_was_set)
                return false;

        if (first->name == NULL || second->name == NULL)
                return first->name == second->name;

        return strcmp (first->name, second->name) == 0;
}

static bool
test_generated_arguments_parse_repeatably (void)
{
        ply_test_seeded_random_t random = {
                .state = UINT32_C (0x8db423a1),
        };
        fuzz_argument_t argument_storage[MAX_FUZZ_ARGUMENT_COUNT - 1];
        char *arguments[MAX_FUZZ_ARGUMENT_COUNT + 1];
        size_t iteration;

        arguments[0] = "tool";
        for (iteration = 0; iteration < FUZZ_ITERATION_COUNT; iteration++) {
                parse_result_t first;
                parse_result_t second;
                int argument_count;
                int i;

                argument_count = 1 + (int) ply_test_seeded_random_range (
                        &random,
                        MAX_FUZZ_ARGUMENT_COUNT);

                for (i = 1; i < argument_count; i++) {
                        generate_argument (&random, argument_storage[i - 1]);
                        arguments[i] = argument_storage[i - 1];
                }
                arguments[argument_count] = NULL;

                parse_arguments (arguments, argument_count, &first);
                parse_arguments (arguments, argument_count, &second);
                PLY_TEST_ASSERT (parse_results_match (&first, &second));

                free (first.name);
                free (second.name);
        }

        return true;
}

static const ply_test_case_t test_cases[] =
{
        PLY_TEST_CASE (test_generated_arguments_parse_repeatably),
};

PLY_TEST_MAIN (test_cases)
