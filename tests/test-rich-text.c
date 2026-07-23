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

#include "ply-rich-text.h"

static bool
test_style_initializes_to_terminal_defaults (void)
{
        ply_rich_text_character_style_t style;

        memset (&style, 0xff, sizeof(style));
        ply_rich_text_character_style_initialize (&style);

        PLY_TEST_ASSERT (style.foreground_color == PLY_TERMINAL_COLOR_DEFAULT);
        PLY_TEST_ASSERT (style.background_color == PLY_TERMINAL_COLOR_DEFAULT);
        PLY_TEST_ASSERT (!style.bold_enabled);
        PLY_TEST_ASSERT (!style.dim_enabled);
        PLY_TEST_ASSERT (!style.italic_enabled);
        PLY_TEST_ASSERT (!style.underline_enabled);
        PLY_TEST_ASSERT (!style.reverse_enabled);

        return true;
}

static bool
test_character_storage_preserves_bytes_and_style (void)
{
        static const char euro[] = "\xe2\x82\xac";
        static const char bounded_bytes[] = { 'x', '\0', 'y' };
        static const char expected[] = {
                'A', '\xe2', '\x82', '\xac', 'x', '\0', 'y'
        };
        ply_rich_text_character_style_t style;
        ply_rich_text_character_t **characters;
        ply_rich_text_span_t mutable_span = { .offset = 0, .range = 8 };
        ply_rich_text_span_t full_span = { .offset = 0, .range = -1 };
        ply_rich_text_t *rich_text;
        char *string;

        ply_rich_text_character_style_initialize (&style);
        style.foreground_color = PLY_TERMINAL_COLOR_RED;
        style.bold_enabled = true;

        rich_text = ply_rich_text_new ();
        ply_rich_text_set_mutable_span (rich_text, &mutable_span);
        ply_rich_text_set_character (rich_text, style, 0, "A", 1);
        ply_rich_text_set_character (rich_text, style, 1, euro, 3);
        ply_rich_text_set_character (rich_text,
                                     style,
                                     2,
                                     bounded_bytes,
                                     sizeof(bounded_bytes));

        PLY_TEST_ASSERT (ply_rich_text_get_length (rich_text) == 3);
        characters = ply_rich_text_get_characters (rich_text);
        PLY_TEST_ASSERT (characters[1]->length == 3);
        PLY_TEST_ASSERT (memcmp (characters[1]->bytes, euro, 3) == 0);
        PLY_TEST_ASSERT (characters[2]->length == sizeof(bounded_bytes));
        PLY_TEST_ASSERT (memcmp (characters[2]->bytes,
                                 bounded_bytes,
                                 sizeof(bounded_bytes)) == 0);
        PLY_TEST_ASSERT (characters[2]->bytes[sizeof(bounded_bytes)] == '\0');
        PLY_TEST_ASSERT (characters[2]->style.foreground_color ==
                         PLY_TERMINAL_COLOR_RED);
        PLY_TEST_ASSERT (characters[2]->style.bold_enabled);

        string = ply_rich_text_get_string (rich_text, &full_span);
        PLY_TEST_ASSERT (memcmp (string, expected, sizeof(expected)) == 0);
        PLY_TEST_ASSERT (string[sizeof(expected)] == '\0');

        free (string);
        ply_rich_text_free (rich_text);
        return true;
}

static bool
test_mutable_span_rejects_outside_writes (void)
{
        ply_rich_text_character_style_t style;
        ply_rich_text_character_t **characters;
        ply_rich_text_span_t mutable_span = { .offset = 2, .range = 2 };
        ply_rich_text_span_t returned_span;
        ply_rich_text_t *rich_text;

        ply_rich_text_character_style_initialize (&style);
        rich_text = ply_rich_text_new ();
        ply_rich_text_set_mutable_span (rich_text, &mutable_span);

        ply_rich_text_set_character (rich_text, style, 1, "a", 1);
        ply_rich_text_set_character (rich_text, style, 2, "b", 1);
        ply_rich_text_set_character (rich_text, style, 3, "c", 1);
        ply_rich_text_set_character (rich_text, style, 4, "d", 1);

        characters = ply_rich_text_get_characters (rich_text);
        PLY_TEST_ASSERT (characters[1] == NULL);
        PLY_TEST_ASSERT (characters[2] != NULL);
        PLY_TEST_ASSERT (characters[3] != NULL);
        PLY_TEST_ASSERT (characters[4] == NULL);

        ply_rich_text_get_mutable_span (rich_text, &returned_span);
        PLY_TEST_ASSERT (returned_span.offset == mutable_span.offset);
        PLY_TEST_ASSERT (returned_span.range == mutable_span.range);

        ply_rich_text_free (rich_text);
        return true;
}

static bool
test_move_remove_and_reset_update_characters (void)
{
        ply_rich_text_character_style_t style;
        ply_rich_text_span_t mutable_span = { .offset = 0, .range = 4 };
        ply_rich_text_span_t full_span = { .offset = 0, .range = -1 };
        ply_rich_text_t *rich_text;
        char *string;

        ply_rich_text_character_style_initialize (&style);
        rich_text = ply_rich_text_new ();
        ply_rich_text_set_mutable_span (rich_text, &mutable_span);
        ply_rich_text_set_character (rich_text, style, 0, "a", 1);
        ply_rich_text_set_character (rich_text, style, 1, "b", 1);
        ply_rich_text_set_character (rich_text, style, 2, "c", 1);
        ply_rich_text_set_character (rich_text, style, 3, "d", 1);

        ply_rich_text_remove_character (rich_text, 1);
        ply_rich_text_move_character (rich_text, 3, 1);

        PLY_TEST_ASSERT (ply_rich_text_get_length (rich_text) == 3);
        string = ply_rich_text_get_string (rich_text, &full_span);
        PLY_TEST_ASSERT (strcmp (string, "adc") == 0);
        free (string);

        ply_rich_text_remove_characters (rich_text);
        PLY_TEST_ASSERT (ply_rich_text_get_length (rich_text) == 0);

        ply_rich_text_set_character (rich_text, style, 0, "z", 1);
        PLY_TEST_ASSERT (ply_rich_text_get_length (rich_text) == 1);

        ply_rich_text_free (rich_text);
        return true;
}

static bool
test_iterator_honors_requested_span (void)
{
        ply_rich_text_character_style_t style;
        ply_rich_text_character_t *character;
        ply_rich_text_iterator_t iterator;
        ply_rich_text_span_t mutable_span = { .offset = 0, .range = 4 };
        ply_rich_text_span_t iterator_span = { .offset = 1, .range = 2 };
        ply_rich_text_t *rich_text;

        ply_rich_text_character_style_initialize (&style);
        rich_text = ply_rich_text_new ();
        ply_rich_text_set_mutable_span (rich_text, &mutable_span);
        ply_rich_text_set_character (rich_text, style, 0, "a", 1);
        ply_rich_text_set_character (rich_text, style, 1, "b", 1);
        ply_rich_text_set_character (rich_text, style, 2, "c", 1);
        ply_rich_text_set_character (rich_text, style, 3, "d", 1);

        ply_rich_text_iterator_initialize (&iterator, rich_text, &iterator_span);
        PLY_TEST_ASSERT (ply_rich_text_iterator_next (&iterator, &character));
        PLY_TEST_ASSERT (memcmp (character->bytes, "b", 1) == 0);
        PLY_TEST_ASSERT (ply_rich_text_iterator_next (&iterator, &character));
        PLY_TEST_ASSERT (memcmp (character->bytes, "c", 1) == 0);
        PLY_TEST_ASSERT (!ply_rich_text_iterator_next (&iterator, &character));

        ply_rich_text_take_reference (rich_text);
        ply_rich_text_drop_reference (rich_text);
        PLY_TEST_ASSERT (ply_rich_text_get_length (rich_text) == 4);
        ply_rich_text_drop_reference (rich_text);
        return true;
}

static const ply_test_case_t test_cases[] =
{
        PLY_TEST_CASE (test_style_initializes_to_terminal_defaults),
        PLY_TEST_CASE (test_character_storage_preserves_bytes_and_style),
        PLY_TEST_CASE (test_mutable_span_rejects_outside_writes),
        PLY_TEST_CASE (test_move_remove_and_reset_update_characters),
        PLY_TEST_CASE (test_iterator_honors_requested_span),
};

PLY_TEST_MAIN (test_cases)
