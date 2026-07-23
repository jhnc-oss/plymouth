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

#include "ply-rectangle.h"

static bool
test_contains_point_includes_outer_pixels (void)
{
        ply_rectangle_t rectangle = {
                .x = -2,
                .y = 3,
                .width = 4,
                .height = 3,
        };

        PLY_TEST_ASSERT (ply_rectangle_contains_point (&rectangle, -2, 3));
        PLY_TEST_ASSERT (ply_rectangle_contains_point (&rectangle, 1, 5));
        PLY_TEST_ASSERT (!ply_rectangle_contains_point (&rectangle, -3, 3));
        PLY_TEST_ASSERT (!ply_rectangle_contains_point (&rectangle, 2, 5));
        PLY_TEST_ASSERT (!ply_rectangle_contains_point (&rectangle, 1, 6));

        return true;
}

static bool
test_empty_requires_both_dimensions (void)
{
        ply_rectangle_t rectangle = { .width = 1, .height = 1 };

        PLY_TEST_ASSERT (!ply_rectangle_is_empty (&rectangle));

        rectangle.width = 0;
        PLY_TEST_ASSERT (ply_rectangle_is_empty (&rectangle));

        rectangle.width = 1;
        rectangle.height = 0;
        PLY_TEST_ASSERT (ply_rectangle_is_empty (&rectangle));

        return true;
}

static bool
test_overlap_classifies_relative_edges (void)
{
        ply_rectangle_t base = { .x = 10, .y = 10, .width = 10, .height = 10 };
        ply_rectangle_t inside = { .x = 12, .y = 12, .width = 3, .height = 3 };
        ply_rectangle_t top_left = { .x = 8, .y = 8, .width = 5, .height = 5 };
        ply_rectangle_t exact_top = { .x = 10, .y = 8, .width = 10, .height = 4 };
        ply_rectangle_t covering = { .x = 8, .y = 8, .width = 14, .height = 14 };
        ply_rectangle_t disjoint = { .x = 20, .y = 10, .width = 2, .height = 2 };

        PLY_TEST_ASSERT (ply_rectangle_find_overlap (&base, &inside) ==
                         PLY_RECTANGLE_OVERLAP_NO_EDGES);
        PLY_TEST_ASSERT (ply_rectangle_find_overlap (&base, &top_left) ==
                         PLY_RECTANGLE_OVERLAP_TOP_AND_LEFT_EDGES);
        PLY_TEST_ASSERT (ply_rectangle_find_overlap (&base, &exact_top) ==
                         PLY_RECTANGLE_OVERLAP_EXACT_TOP_EDGE);
        PLY_TEST_ASSERT (ply_rectangle_find_overlap (&base, &covering) ==
                         PLY_RECTANGLE_OVERLAP_ALL_EDGES);
        PLY_TEST_ASSERT (ply_rectangle_find_overlap (&base, &disjoint) ==
                         PLY_RECTANGLE_OVERLAP_NONE);

        return true;
}

static bool
test_intersection_returns_shared_pixels (void)
{
        ply_rectangle_t first = { .x = 1, .y = 2, .width = 5, .height = 4 };
        ply_rectangle_t second = { .x = 4, .y = 0, .width = 5, .height = 5 };
        ply_rectangle_t result;

        ply_rectangle_intersect (&first, &second, &result);

        PLY_TEST_ASSERT (result.x == 4);
        PLY_TEST_ASSERT (result.y == 2);
        PLY_TEST_ASSERT (result.width == 2);
        PLY_TEST_ASSERT (result.height == 3);

        return true;
}

static bool
test_disjoint_intersection_is_normalized (void)
{
        ply_rectangle_t first = { .x = 0, .y = 0, .width = 2, .height = 2 };
        ply_rectangle_t second = { .x = 4, .y = 5, .width = 2, .height = 2 };
        ply_rectangle_t result;

        ply_rectangle_intersect (&first, &second, &result);

        PLY_TEST_ASSERT (result.width == 0);
        PLY_TEST_ASSERT (result.height == 0);
        PLY_TEST_ASSERT (ply_rectangle_is_empty (&result));

        return true;
}

static bool
test_empty_intersection_retains_empty_operand (void)
{
        ply_rectangle_t empty = { .x = 7, .y = 8, .width = 0, .height = 9 };
        ply_rectangle_t filled = { .x = 1, .y = 2, .width = 3, .height = 4 };
        ply_rectangle_t result;

        ply_rectangle_intersect (&empty, &filled, &result);

        PLY_TEST_ASSERT (result.x == empty.x);
        PLY_TEST_ASSERT (result.y == empty.y);
        PLY_TEST_ASSERT (result.width == empty.width);
        PLY_TEST_ASSERT (result.height == empty.height);

        return true;
}

static const ply_test_case_t test_cases[] =
{
        PLY_TEST_CASE (test_contains_point_includes_outer_pixels),
        PLY_TEST_CASE (test_empty_requires_both_dimensions),
        PLY_TEST_CASE (test_overlap_classifies_relative_edges),
        PLY_TEST_CASE (test_intersection_returns_shared_pixels),
        PLY_TEST_CASE (test_disjoint_intersection_is_normalized),
        PLY_TEST_CASE (test_empty_intersection_retains_empty_operand),
};

PLY_TEST_MAIN (test_cases)
