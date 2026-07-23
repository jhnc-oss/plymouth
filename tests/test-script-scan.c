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

#include <string.h>

#include "script-scan.h"

static bool
test_scans_identifiers_numbers_strings_and_symbols (void)
{
        script_scan_t *scan;
        script_scan_token_t *token;

        scan = script_scan_string (
                "alpha_2 42 3.25 \"line\\nquote\\\"\" += != &&",
                "tokens.script");
        PLY_TEST_ASSERT (scan != NULL);

        token = script_scan_get_current_token (scan);
        PLY_TEST_ASSERT (token->type == SCRIPT_SCAN_TOKEN_TYPE_IDENTIFIER);
        PLY_TEST_ASSERT (strcmp (token->data.string, "alpha_2") == 0);
        PLY_TEST_ASSERT (token->location.line_index == 1);
        PLY_TEST_ASSERT (token->location.column_index == 0);
        PLY_TEST_ASSERT (strcmp (token->location.name, "tokens.script") == 0);

        token = script_scan_get_next_token (scan);
        PLY_TEST_ASSERT (token->type == SCRIPT_SCAN_TOKEN_TYPE_INTEGER);
        PLY_TEST_ASSERT (token->data.integer == 42);
        PLY_TEST_ASSERT (token->whitespace == 1);

        token = script_scan_get_next_token (scan);
        PLY_TEST_ASSERT (token->type == SCRIPT_SCAN_TOKEN_TYPE_FLOAT);
        PLY_TEST_ASSERT (token->data.floatpoint == 3.25);

        token = script_scan_get_next_token (scan);
        PLY_TEST_ASSERT (token->type == SCRIPT_SCAN_TOKEN_TYPE_STRING);
        PLY_TEST_ASSERT (strcmp (token->data.string, "line\nquote\"") == 0);

        token = script_scan_get_next_token (scan);
        PLY_TEST_ASSERT (script_scan_token_is_symbol_of_value (token, '+'));
        token = script_scan_get_next_token (scan);
        PLY_TEST_ASSERT (script_scan_token_is_symbol_of_value (token, '='));
        token = script_scan_get_next_token (scan);
        PLY_TEST_ASSERT (script_scan_token_is_symbol_of_value (token, '!'));
        token = script_scan_get_next_token (scan);
        PLY_TEST_ASSERT (script_scan_token_is_symbol_of_value (token, '='));
        token = script_scan_get_next_token (scan);
        PLY_TEST_ASSERT (script_scan_token_is_symbol_of_value (token, '&'));
        token = script_scan_get_next_token (scan);
        PLY_TEST_ASSERT (script_scan_token_is_symbol_of_value (token, '&'));
        token = script_scan_get_next_token (scan);
        PLY_TEST_ASSERT (token->type == SCRIPT_SCAN_TOKEN_TYPE_EOF);

        script_scan_free (scan);
        return true;
}

static bool
test_skips_line_and_nested_block_comments (void)
{
        static const char input[] =
                "# first line\n"
                "// second line\n"
                "/* outer /* nested */ tail */ value";
        script_scan_t *scan;
        script_scan_token_t *token;

        scan = script_scan_string (input, "comments.script");
        PLY_TEST_ASSERT (scan != NULL);
        token = script_scan_get_current_token (scan);

        PLY_TEST_ASSERT (token->type == SCRIPT_SCAN_TOKEN_TYPE_IDENTIFIER);
        PLY_TEST_ASSERT (strcmp (token->data.string, "value") == 0);
        PLY_TEST_ASSERT (token->location.line_index == 3);

        token = script_scan_get_next_token (scan);
        PLY_TEST_ASSERT (token->type == SCRIPT_SCAN_TOKEN_TYPE_EOF);

        script_scan_free (scan);
        return true;
}

static bool
test_peek_preserves_current_token (void)
{
        script_scan_t *scan;
        script_scan_token_t *current;
        script_scan_token_t *peeked;

        scan = script_scan_string ("first second", "peek.script");
        PLY_TEST_ASSERT (scan != NULL);

        current = script_scan_get_current_token (scan);
        peeked = script_scan_peek_next_token (scan);
        PLY_TEST_ASSERT (strcmp (current->data.string, "first") == 0);
        PLY_TEST_ASSERT (strcmp (peeked->data.string, "second") == 0);
        PLY_TEST_ASSERT (script_scan_get_current_token (scan) == current);

        current = script_scan_get_next_token (scan);
        PLY_TEST_ASSERT (current->type == SCRIPT_SCAN_TOKEN_TYPE_IDENTIFIER);
        PLY_TEST_ASSERT (strcmp (current->data.string, "second") == 0);

        script_scan_free (scan);
        return true;
}

static bool
test_reports_unterminated_string (void)
{
        script_scan_t *scan;
        script_scan_token_t *token;

        scan = script_scan_string ("\"unfinished", "string-error.script");
        PLY_TEST_ASSERT (scan != NULL);
        token = script_scan_get_current_token (scan);

        PLY_TEST_ASSERT (token->type == SCRIPT_SCAN_TOKEN_TYPE_ERROR);
        PLY_TEST_ASSERT (strcmp (token->data.string,
                                 "End of file before end of string") == 0);
        PLY_TEST_ASSERT (token->location.line_index == 1);
        PLY_TEST_ASSERT (token->location.column_index == 0);

        script_scan_free (scan);
        return true;
}

static bool
test_reports_line_break_inside_string (void)
{
        script_scan_t *scan;
        script_scan_token_t *token;

        scan = script_scan_string ("\"first\nsecond\"", "line-error.script");
        PLY_TEST_ASSERT (scan != NULL);
        token = script_scan_get_current_token (scan);

        PLY_TEST_ASSERT (token->type == SCRIPT_SCAN_TOKEN_TYPE_ERROR);
        PLY_TEST_ASSERT (strcmp (token->data.string,
                                 "Line terminator before end of string") == 0);

        script_scan_free (scan);
        return true;
}

static bool
test_reports_unterminated_block_comment (void)
{
        script_scan_t *scan;
        script_scan_token_t *token;

        scan = script_scan_string ("/* unfinished", "comment-error.script");
        PLY_TEST_ASSERT (scan != NULL);
        token = script_scan_get_current_token (scan);

        PLY_TEST_ASSERT (token->type == SCRIPT_SCAN_TOKEN_TYPE_ERROR);
        PLY_TEST_ASSERT (strcmp (token->data.string,
                                 "End of file before end of comment") == 0);

        script_scan_free (scan);
        return true;
}

static bool
test_reports_out_of_range_integer (void)
{
        script_scan_t *scan;
        script_scan_token_t *token;

        scan = script_scan_string ("9223372036854775808",
                                   "integer-error.script");
        PLY_TEST_ASSERT (scan != NULL);
        token = script_scan_get_current_token (scan);

        PLY_TEST_ASSERT (token->type == SCRIPT_SCAN_TOKEN_TYPE_ERROR);
        PLY_TEST_ASSERT (strcmp (token->data.string,
                                 "Integer literal is out of range") == 0);

        token = script_scan_get_next_token (scan);
        PLY_TEST_ASSERT (token->type == SCRIPT_SCAN_TOKEN_TYPE_EOF);

        script_scan_free (scan);
        return true;
}

static const ply_test_case_t test_cases[] =
{
        PLY_TEST_CASE (test_scans_identifiers_numbers_strings_and_symbols),
        PLY_TEST_CASE (test_skips_line_and_nested_block_comments),
        PLY_TEST_CASE (test_peek_preserves_current_token),
        PLY_TEST_CASE (test_reports_unterminated_string),
        PLY_TEST_CASE (test_reports_line_break_inside_string),
        PLY_TEST_CASE (test_reports_unterminated_block_comment),
        PLY_TEST_CASE (test_reports_out_of_range_integer),
};

PLY_TEST_MAIN (test_cases)
