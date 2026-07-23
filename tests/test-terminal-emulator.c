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

#include "ply-buffer.h"
#include "ply-terminal-emulator.h"

typedef struct
{
        int  count;
        char last_output[64];
} output_context_t;

static char *
get_line_string (ply_terminal_emulator_t *terminal_emulator,
                 int                      line_number)
{
        ply_rich_text_span_t span = { .offset = 0, .range = -1 };

        return ply_rich_text_get_string (
                ply_terminal_emulator_get_nth_line (terminal_emulator,
                                                    line_number),
                &span);
}

static void
on_output (void       *user_data,
           const char *output)
{
        output_context_t *context = user_data;

        context->count++;
        strncpy (context->last_output,
                 output,
                 sizeof(context->last_output) - 1);
}

static bool
test_plain_text_wraps_at_fixed_width (void)
{
        ply_terminal_emulator_t *terminal_emulator;
        char *first_line;
        char *second_line;

        terminal_emulator = ply_terminal_emulator_new (3, 5);
        ply_terminal_emulator_parse_lines (terminal_emulator, "abcdef", 6);

        PLY_TEST_ASSERT (ply_terminal_emulator_get_line_count (terminal_emulator) == 2);
        first_line = get_line_string (terminal_emulator, 0);
        second_line = get_line_string (terminal_emulator, 1);
        PLY_TEST_ASSERT (strcmp (first_line, "abcde") == 0);
        PLY_TEST_ASSERT (strcmp (second_line, "f") == 0);

        free (first_line);
        free (second_line);
        ply_terminal_emulator_free (terminal_emulator);
        return true;
}

static bool
test_control_characters_move_cursor_and_line (void)
{
        static const char input[] = "abc\rZ\bY\nnext";
        ply_terminal_emulator_t *terminal_emulator;
        char *first_line;
        char *second_line;

        terminal_emulator = ply_terminal_emulator_new (3, 10);
        ply_terminal_emulator_parse_lines (terminal_emulator,
                                           input,
                                           sizeof(input) - 1);

        first_line = get_line_string (terminal_emulator, 0);
        second_line = get_line_string (terminal_emulator, 1);
        PLY_TEST_ASSERT (strcmp (first_line, "Ybc") == 0);
        PLY_TEST_ASSERT (strcmp (second_line, "next") == 0);

        free (first_line);
        free (second_line);
        ply_terminal_emulator_free (terminal_emulator);
        return true;
}

static bool
test_graphic_attributes_apply_per_character (void)
{
        static const char input[] = "\033[31;1mR\033[0mN";
        ply_terminal_emulator_t *terminal_emulator;
        ply_rich_text_t *line;
        ply_rich_text_character_t **characters;

        terminal_emulator = ply_terminal_emulator_new (2, 10);
        ply_terminal_emulator_parse_lines (terminal_emulator,
                                           input,
                                           sizeof(input) - 1);

        line = ply_terminal_emulator_get_nth_line (terminal_emulator, 0);
        characters = ply_rich_text_get_characters (line);

        PLY_TEST_ASSERT (characters[0] != NULL);
        PLY_TEST_ASSERT (characters[0]->style.foreground_color ==
                         PLY_TERMINAL_COLOR_RED);
        PLY_TEST_ASSERT (characters[0]->style.bold_enabled);
        PLY_TEST_ASSERT (characters[1] != NULL);
        PLY_TEST_ASSERT (characters[1]->style.foreground_color ==
                         PLY_TERMINAL_COLOR_DEFAULT);
        PLY_TEST_ASSERT (!characters[1]->style.bold_enabled);

        ply_terminal_emulator_free (terminal_emulator);
        return true;
}

static bool
test_graphic_attributes_can_be_disabled_individually (void)
{
        static const char input[] =
                "\033[1;2;3;4;7;32;44mA\033[22;23;24;27;39;49mB";
        ply_terminal_emulator_t *terminal_emulator;
        ply_rich_text_t *line;
        ply_rich_text_character_t **characters;

        terminal_emulator = ply_terminal_emulator_new (2, 10);
        ply_terminal_emulator_parse_lines (terminal_emulator,
                                           input,
                                           sizeof(input) - 1);

        line = ply_terminal_emulator_get_nth_line (terminal_emulator, 0);
        characters = ply_rich_text_get_characters (line);

        PLY_TEST_ASSERT (characters[0] != NULL);
        PLY_TEST_ASSERT (characters[0]->style.foreground_color ==
                         PLY_TERMINAL_COLOR_GREEN);
        PLY_TEST_ASSERT (characters[0]->style.background_color ==
                         PLY_TERMINAL_COLOR_BLUE);
        PLY_TEST_ASSERT (characters[0]->style.bold_enabled);
        PLY_TEST_ASSERT (characters[0]->style.dim_enabled);
        PLY_TEST_ASSERT (characters[0]->style.italic_enabled);
        PLY_TEST_ASSERT (characters[0]->style.underline_enabled);
        PLY_TEST_ASSERT (characters[0]->style.reverse_enabled);

        PLY_TEST_ASSERT (characters[1] != NULL);
        PLY_TEST_ASSERT (characters[1]->style.foreground_color ==
                         PLY_TERMINAL_COLOR_DEFAULT);
        PLY_TEST_ASSERT (characters[1]->style.background_color ==
                         PLY_TERMINAL_COLOR_DEFAULT);
        PLY_TEST_ASSERT (!characters[1]->style.bold_enabled);
        PLY_TEST_ASSERT (!characters[1]->style.dim_enabled);
        PLY_TEST_ASSERT (!characters[1]->style.italic_enabled);
        PLY_TEST_ASSERT (!characters[1]->style.underline_enabled);
        PLY_TEST_ASSERT (!characters[1]->style.reverse_enabled);

        ply_terminal_emulator_free (terminal_emulator);
        return true;
}

static bool
test_split_control_sequence_is_reassembled (void)
{
        static const char first_part[] = "\033[3";
        static const char second_part[] = "1mR";
        ply_terminal_emulator_t *terminal_emulator;
        ply_rich_text_character_t **characters;

        terminal_emulator = ply_terminal_emulator_new (2, 10);
        ply_terminal_emulator_parse_lines (terminal_emulator,
                                           first_part,
                                           sizeof(first_part) - 1);
        ply_terminal_emulator_parse_lines (terminal_emulator,
                                           second_part,
                                           sizeof(second_part) - 1);

        characters = ply_rich_text_get_characters (
                ply_terminal_emulator_get_nth_line (terminal_emulator, 0));
        PLY_TEST_ASSERT (characters[0] != NULL);
        PLY_TEST_ASSERT (characters[0]->style.foreground_color ==
                         PLY_TERMINAL_COLOR_RED);

        ply_terminal_emulator_free (terminal_emulator);
        return true;
}

static bool
test_malformed_control_sequence_is_ignored (void)
{
        static const char input[] = "\033[3?1mN\033[32mG";
        ply_terminal_emulator_t *terminal_emulator;
        ply_rich_text_character_t **characters;

        terminal_emulator = ply_terminal_emulator_new (2, 10);
        ply_terminal_emulator_parse_lines (terminal_emulator,
                                           input,
                                           sizeof(input) - 1);

        characters = ply_rich_text_get_characters (
                ply_terminal_emulator_get_nth_line (terminal_emulator, 0));
        PLY_TEST_ASSERT (characters[0] != NULL);
        PLY_TEST_ASSERT (characters[0]->style.foreground_color ==
                         PLY_TERMINAL_COLOR_DEFAULT);
        PLY_TEST_ASSERT (characters[1] != NULL);
        PLY_TEST_ASSERT (characters[1]->style.foreground_color ==
                         PLY_TERMINAL_COLOR_GREEN);

        ply_terminal_emulator_free (terminal_emulator);
        return true;
}

static bool
test_cursor_and_erase_sequences_update_line (void)
{
        static const char input[] = "abcde\033[2DZ\033[0K";
        ply_terminal_emulator_t *terminal_emulator;
        char *line;

        terminal_emulator = ply_terminal_emulator_new (2, 10);
        ply_terminal_emulator_parse_lines (terminal_emulator,
                                           input,
                                           sizeof(input) - 1);

        line = get_line_string (terminal_emulator, 0);
        PLY_TEST_ASSERT (strcmp (line, "abcZ") == 0);

        free (line);
        ply_terminal_emulator_free (terminal_emulator);
        return true;
}

static bool
test_split_utf8_input_is_reassembled (void)
{
        static const char first_byte[] = { '\xe2' };
        static const char remaining_bytes[] = { '\x82', '\xac' };
        ply_terminal_emulator_t *terminal_emulator;
        ply_rich_text_character_t **characters;
        char *line;

        terminal_emulator = ply_terminal_emulator_new (2, 10);
        ply_terminal_emulator_parse_lines (terminal_emulator,
                                           first_byte,
                                           sizeof(first_byte));
        line = get_line_string (terminal_emulator, 0);
        PLY_TEST_ASSERT (strcmp (line, "") == 0);
        free (line);

        ply_terminal_emulator_parse_lines (terminal_emulator,
                                           remaining_bytes,
                                           sizeof(remaining_bytes));
        characters = ply_rich_text_get_characters (
                ply_terminal_emulator_get_nth_line (terminal_emulator, 0));
        PLY_TEST_ASSERT (characters[0] != NULL);
        PLY_TEST_ASSERT (characters[0]->length == 3);
        PLY_TEST_ASSERT (memcmp (characters[0]->bytes,
                                 "\xe2\x82\xac",
                                 3) == 0);

        ply_terminal_emulator_free (terminal_emulator);
        return true;
}

static bool
test_invalid_utf8_in_escape_sequence_is_ignored (void)
{
        static const char input[] = { '\033', '\x80', '\x80' };
        ply_terminal_emulator_t *terminal_emulator;
        char *line;

        terminal_emulator = ply_terminal_emulator_new (3, 1);
        ply_terminal_emulator_parse_lines (terminal_emulator,
                                           input,
                                           sizeof(input));

        PLY_TEST_ASSERT (ply_terminal_emulator_get_line_count (
                                 terminal_emulator) == 1);
        line = get_line_string (terminal_emulator, 0);
        PLY_TEST_ASSERT (strcmp (line, "") == 0);

        free (line);
        ply_terminal_emulator_free (terminal_emulator);
        return true;
}

static bool
test_recent_lines_survive_ring_rotation (void)
{
        static const char input[] = "one\ntwo\nthree";
        ply_terminal_emulator_t *terminal_emulator;
        char *second_line;
        char *third_line;

        terminal_emulator = ply_terminal_emulator_new (2, 10);
        ply_terminal_emulator_parse_lines (terminal_emulator,
                                           input,
                                           sizeof(input) - 1);

        PLY_TEST_ASSERT (ply_terminal_emulator_get_line_count (terminal_emulator) == 3);
        second_line = get_line_string (terminal_emulator, 1);
        third_line = get_line_string (terminal_emulator, 2);
        PLY_TEST_ASSERT (strcmp (second_line, "two") == 0);
        PLY_TEST_ASSERT (strcmp (third_line, "three") == 0);

        free (second_line);
        free (third_line);
        ply_terminal_emulator_free (terminal_emulator);
        return true;
}

static bool
test_upward_cursor_movement_stops_at_first_line (void)
{
        static const char input[] = "one\ntwo\nthree\r\033[3AZ";
        ply_terminal_emulator_t *terminal_emulator;
        char *first_line;
        char *last_line;

        terminal_emulator = ply_terminal_emulator_new (3, 10);
        ply_terminal_emulator_parse_lines (terminal_emulator,
                                           input,
                                           sizeof(input) - 1);

        first_line = get_line_string (terminal_emulator, 0);
        last_line = get_line_string (terminal_emulator, 2);
        PLY_TEST_ASSERT (strcmp (first_line, "Zne") == 0);
        PLY_TEST_ASSERT (strcmp (last_line, "three") == 0);

        free (first_line);
        free (last_line);
        ply_terminal_emulator_free (terminal_emulator);
        return true;
}

static bool
test_boot_buffer_notifies_output_watcher (void)
{
        output_context_t context = { 0 };
        ply_terminal_emulator_t *terminal_emulator;
        ply_buffer_t *buffer;
        char *line;

        terminal_emulator = ply_terminal_emulator_new (2, 10);
        ply_terminal_emulator_watch_for_output (terminal_emulator,
                                                on_output,
                                                &context);
        buffer = ply_buffer_new ();
        ply_buffer_append (buffer, "boot");

        ply_terminal_emulator_convert_boot_buffer (terminal_emulator, buffer);

        PLY_TEST_ASSERT (context.count == 1);
        PLY_TEST_ASSERT (strcmp (context.last_output, "boot") == 0);
        line = get_line_string (terminal_emulator, 0);
        PLY_TEST_ASSERT (strcmp (line, "boot") == 0);

        free (line);
        ply_buffer_free (buffer);
        ply_terminal_emulator_free (terminal_emulator);
        return true;
}

static bool
test_incomplete_escape_can_be_destroyed (void)
{
        static const char input[] = "\033[31;";
        ply_terminal_emulator_t *terminal_emulator;

        terminal_emulator = ply_terminal_emulator_new (2, 10);
        ply_terminal_emulator_parse_lines (terminal_emulator,
                                           input,
                                           sizeof(input) - 1);
        ply_terminal_emulator_free (terminal_emulator);

        return true;
}

static const ply_test_case_t test_cases[] =
{
        PLY_TEST_CASE (test_plain_text_wraps_at_fixed_width),
        PLY_TEST_CASE (test_control_characters_move_cursor_and_line),
        PLY_TEST_CASE (test_graphic_attributes_apply_per_character),
        PLY_TEST_CASE (test_graphic_attributes_can_be_disabled_individually),
        PLY_TEST_CASE (test_split_control_sequence_is_reassembled),
        PLY_TEST_CASE (test_malformed_control_sequence_is_ignored),
        PLY_TEST_CASE (test_cursor_and_erase_sequences_update_line),
        PLY_TEST_CASE (test_split_utf8_input_is_reassembled),
        PLY_TEST_CASE (test_invalid_utf8_in_escape_sequence_is_ignored),
        PLY_TEST_CASE (test_recent_lines_survive_ring_rotation),
        PLY_TEST_CASE (test_upward_cursor_movement_stops_at_first_line),
        PLY_TEST_CASE (test_boot_buffer_notifies_output_watcher),
        PLY_TEST_CASE (test_incomplete_escape_can_be_destroyed),
};

PLY_TEST_MAIN (test_cases)
