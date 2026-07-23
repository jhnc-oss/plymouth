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
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ply-command-parser.h"
#include "ply-event-loop.h"

typedef struct
{
        ply_event_loop_t *loop;
        const char *commands[4];
        int command_count;
        int expected_command_count;
        bool timed_out;
} dispatch_context_t;

static void
on_command (void       *user_data,
            const char *command)
{
        dispatch_context_t *context = user_data;

        context->commands[context->command_count++] = command;
        if (context->command_count == context->expected_command_count)
                ply_event_loop_exit (context->loop, 0);
}

static void
on_watchdog_timeout (void             *user_data,
                     ply_event_loop_t *loop)
{
        dispatch_context_t *context = user_data;

        context->timed_out = true;
        ply_event_loop_exit (loop, 1);
}

static void
add_main_options (ply_command_parser_t *parser)
{
        ply_command_parser_add_options (
                parser,
                "verbose", "Show additional output", PLY_COMMAND_OPTION_TYPE_FLAG,
                "enabled", "Enable the operation", PLY_COMMAND_OPTION_TYPE_BOOLEAN,
                "name", "Select a name", PLY_COMMAND_OPTION_TYPE_STRING,
                "count", "Set a count", PLY_COMMAND_OPTION_TYPE_INTEGER,
                NULL);
}

static bool
parse_integer_argument (const char *argument,
                        int        *value,
                        bool       *was_set)
{
        ply_command_parser_t *parser;
        ply_event_loop_t *loop;
        char *arguments[] = { "tool", "--count", (char *) argument, NULL };
        bool parsed;

        parser = ply_command_parser_new ("tool", "Test tool");
        ply_command_parser_add_options (
                parser,
                "count", "Set a count", PLY_COMMAND_OPTION_TYPE_INTEGER,
                NULL);
        loop = ply_event_loop_new ();

        parsed = ply_command_parser_parse_arguments (parser, loop, arguments, 3);
        ply_command_parser_get_option (parser, "count", value, was_set);

        ply_command_parser_stop_parsing_arguments (parser);
        ply_command_parser_free (parser);
        ply_event_loop_free (loop);
        return parsed;
}

static bool
test_main_options_parse_each_type (void)
{
        char *arguments[] = {
                "tool",
                "--verbose",
                "--enabled=false",
                "--name=spinner",
                "--count",
                "0x2a",
                NULL,
        };
        ply_command_parser_t *parser;
        ply_event_loop_t *loop;
        bool verbose = false;
        bool enabled = true;
        bool was_set = false;
        char *name = NULL;
        int count = 0;

        parser = ply_command_parser_new ("tool", "Test tool");
        add_main_options (parser);
        loop = ply_event_loop_new ();

        PLY_TEST_ASSERT (ply_command_parser_parse_arguments (parser,
                                                             loop,
                                                             arguments,
                                                             6));

        ply_command_parser_get_option (parser, "verbose", &verbose, &was_set);
        PLY_TEST_ASSERT (verbose);
        PLY_TEST_ASSERT (was_set);

        ply_command_parser_get_options (parser,
                                        "enabled", &enabled,
                                        "name", &name,
                                        "count", &count,
                                        NULL);
        PLY_TEST_ASSERT (!enabled);
        PLY_TEST_ASSERT (name != NULL);
        PLY_TEST_ASSERT (strcmp (name, "spinner") == 0);
        PLY_TEST_ASSERT (count == 42);

        free (name);
        ply_command_parser_stop_parsing_arguments (parser);
        ply_command_parser_free (parser);
        ply_event_loop_free (loop);
        return true;
}

static bool
test_integer_options_enforce_int_range (void)
{
        char below_minimum[64];
        char above_maximum[64];
        char minimum[64];
        char maximum[64];
        int value;
        bool was_set;

        snprintf (below_minimum,
                  sizeof (below_minimum),
                  "%ld",
                  (long) INT_MIN - 1);
        snprintf (above_maximum,
                  sizeof (above_maximum),
                  "%ld",
                  (long) INT_MAX + 1);
        snprintf (minimum, sizeof (minimum), "%d", INT_MIN);
        snprintf (maximum, sizeof (maximum), "%d", INT_MAX);

        PLY_TEST_ASSERT (parse_integer_argument (below_minimum, &value, &was_set));
        PLY_TEST_ASSERT (!was_set);
        PLY_TEST_ASSERT (parse_integer_argument (above_maximum, &value, &was_set));
        PLY_TEST_ASSERT (!was_set);

        errno = ERANGE;
        PLY_TEST_ASSERT (parse_integer_argument (minimum, &value, &was_set));
        PLY_TEST_ASSERT (was_set);
        PLY_TEST_ASSERT (value == INT_MIN);

        PLY_TEST_ASSERT (parse_integer_argument (maximum, &value, &was_set));
        PLY_TEST_ASSERT (was_set);
        PLY_TEST_ASSERT (value == INT_MAX);

        return true;
}

static bool
test_subcommand_alias_dispatches_in_order (void)
{
        char *arguments[] = {
                "tool",
                "s",
                "--message=hello",
                "--times",
                "3",
                "--force",
                "hide",
                NULL,
        };
        dispatch_context_t context = {
                .expected_command_count = 2,
        };
        ply_command_parser_t *parser;
        ply_event_loop_t *loop;
        char *message = NULL;
        int times = 0;
        bool force = false;
        bool was_set = false;

        parser = ply_command_parser_new ("tool", "Test tool");
        loop = ply_event_loop_new ();
        context.loop = loop;

        ply_command_parser_add_command (
                parser,
                "show", "Show the splash", on_command, &context,
                "message", "Set the message", PLY_COMMAND_OPTION_TYPE_STRING,
                "times", "Set repetitions", PLY_COMMAND_OPTION_TYPE_INTEGER,
                "force", "Force display", PLY_COMMAND_OPTION_TYPE_FLAG,
                NULL);
        ply_command_parser_add_command (
                parser,
                "hide", "Hide the splash", on_command, &context,
                NULL);
        ply_command_parser_add_command_alias (parser, "show", "s");

        PLY_TEST_ASSERT (ply_command_parser_parse_arguments (parser,
                                                             loop,
                                                             arguments,
                                                             7));

        ply_command_parser_get_command_option (parser,
                                               "show",
                                               "message",
                                               &message,
                                               &was_set);
        PLY_TEST_ASSERT (was_set);
        PLY_TEST_ASSERT (message != NULL);
        PLY_TEST_ASSERT (strcmp (message, "hello") == 0);

        ply_command_parser_get_command_options (parser,
                                                "show",
                                                "times", &times,
                                                "force", &force,
                                                NULL);
        PLY_TEST_ASSERT (times == 3);
        PLY_TEST_ASSERT (force);

        ply_event_loop_watch_for_timeout (loop,
                                          1.0,
                                          on_watchdog_timeout,
                                          &context);
        PLY_TEST_ASSERT (ply_event_loop_run (loop) == 0);
        PLY_TEST_ASSERT (!context.timed_out);
        PLY_TEST_ASSERT (context.command_count == 2);
        PLY_TEST_ASSERT (strcmp (context.commands[0], "show") == 0);
        PLY_TEST_ASSERT (strcmp (context.commands[1], "hide") == 0);

        free (message);
        ply_command_parser_free (parser);
        ply_event_loop_free (loop);
        return true;
}

static bool
test_help_describes_options_and_commands (void)
{
        ply_command_parser_t *parser;
        char *help;

        parser = ply_command_parser_new ("tool", "Test tool description");
        add_main_options (parser);
        ply_command_parser_add_command (
                parser,
                "show", "Show the splash", NULL, NULL,
                "message", "Set the message", PLY_COMMAND_OPTION_TYPE_STRING,
                NULL);

        help = ply_command_parser_get_help_string (parser);

        PLY_TEST_ASSERT (help != NULL);
        PLY_TEST_ASSERT (strstr (help, "Test tool description") != NULL);
        PLY_TEST_ASSERT (strstr (help, "USAGE: tool [OPTION...]") != NULL);
        PLY_TEST_ASSERT (strstr (help, "--enabled={true|false}") != NULL);
        PLY_TEST_ASSERT (strstr (help, "show") != NULL);
        PLY_TEST_ASSERT (strstr (help, "--message=<string>") != NULL);

        free (help);
        ply_command_parser_free (parser);
        return true;
}

static const ply_test_case_t test_cases[] =
{
        PLY_TEST_CASE (test_main_options_parse_each_type),
        PLY_TEST_CASE (test_integer_options_enforce_int_range),
        PLY_TEST_CASE (test_subcommand_alias_dispatches_in_order),
        PLY_TEST_CASE (test_help_describes_options_and_commands),
};

PLY_TEST_MAIN (test_cases)
