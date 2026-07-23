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

#include "script-parse.h"

static bool
test_parser_rejects_incomplete_constructs (void)
{
        script_op_t *op;

        op = script_parse_string ("value = ;", "missing-value.script");
        PLY_TEST_ASSERT (op == NULL);

        op = script_parse_string ("if (1) { value = 2;",
                                  "missing-brace.script");
        PLY_TEST_ASSERT (op == NULL);

        op = script_parse_string ("fun broken(a b) { return a; }",
                                  "bad-parameters.script");
        PLY_TEST_ASSERT (op == NULL);
        return true;
}

static const ply_test_case_t test_cases[] =
{
        PLY_TEST_CASE (test_parser_rejects_incomplete_constructs),
};

PLY_TEST_MAIN (test_cases)
