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

#include <stdint.h>
#include <string.h>

#include "ply-list.h"
#include "ply-rectangle.h"
#include "ply-region.h"

#define GRID_SIZE 32
#define RANDOM_RECTANGLE_COUNT 200

static uint32_t
next_random (uint32_t *state)
{
        *state = *state * UINT32_C (1664525) + UINT32_C (1013904223);
        return *state;
}

static bool
test_new_region_is_empty (void)
{
        ply_region_t *region;

        region = ply_region_new ();

        PLY_TEST_ASSERT (ply_region_is_empty (region));
        PLY_TEST_ASSERT (ply_list_get_length (ply_region_get_rectangle_list (region)) == 0);

        ply_region_free (region);
        return true;
}

static bool
test_region_copies_input_and_clears (void)
{
        ply_rectangle_t input = { .x = 1, .y = 2, .width = 3, .height = 4 };
        ply_region_t *region;
        ply_rectangle_t *stored;

        region = ply_region_new ();
        ply_region_add_rectangle (region, &input);
        input.x = 100;

        PLY_TEST_ASSERT (!ply_region_is_empty (region));
        PLY_TEST_ASSERT (ply_list_get_length (ply_region_get_rectangle_list (region)) == 1);

        stored = ply_list_node_get_data (
                ply_list_get_first_node (ply_region_get_rectangle_list (region)));
        PLY_TEST_ASSERT (stored->x == 1);
        PLY_TEST_ASSERT (stored->y == 2);
        PLY_TEST_ASSERT (stored->width == 3);
        PLY_TEST_ASSERT (stored->height == 4);

        ply_region_clear (region);
        PLY_TEST_ASSERT (ply_region_is_empty (region));

        ply_region_free (region);
        return true;
}

static bool
test_sorted_rectangles_have_monotonic_rows (void)
{
        ply_rectangle_t rectangles[] = {
                { .x = 0,  .y = 20, .width = 2, .height = 2 },
                { .x = 5,  .y = 2,  .width = 2, .height = 2 },
                { .x = 10, .y = 11, .width = 2, .height = 2 },
        };
        ply_region_t *region;
        ply_list_t *list;
        ply_list_node_t *node;
        long previous_y = -1;
        size_t i;

        region = ply_region_new ();
        for (i = 0; i < sizeof(rectangles) / sizeof(rectangles[0]); i++) {
                ply_region_add_rectangle (region, &rectangles[i]);
        }

        list = ply_region_get_sorted_rectangle_list (region);
        ply_list_foreach (list, node) {
                ply_rectangle_t *rectangle = ply_list_node_get_data (node);

                PLY_TEST_ASSERT (rectangle->y >= previous_y);
                previous_y = rectangle->y;
        }

        ply_region_free (region);
        return true;
}

static bool
test_random_union_matches_cell_oracle (void)
{
        bool expected[GRID_SIZE][GRID_SIZE];
        unsigned char observed[GRID_SIZE][GRID_SIZE];
        uint32_t random_state = UINT32_C (0x4a17b39d);
        ply_region_t *region;
        ply_list_t *list;
        ply_list_node_t *node;
        int i;
        int x;
        int y;

        memset (expected, 0, sizeof(expected));
        memset (observed, 0, sizeof(observed));
        region = ply_region_new ();

        for (i = 0; i < RANDOM_RECTANGLE_COUNT; i++) {
                ply_rectangle_t rectangle;
                long right_edge;
                long bottom_edge;

                rectangle.x = next_random (&random_state) % 24;
                rectangle.y = next_random (&random_state) % 24;
                rectangle.width = next_random (&random_state) % 9;
                rectangle.height = next_random (&random_state) % 9;
                right_edge = rectangle.x + rectangle.width;
                bottom_edge = rectangle.y + rectangle.height;

                for (y = rectangle.y; y < bottom_edge; y++) {
                        for (x = rectangle.x; x < right_edge; x++) {
                                expected[y][x] = true;
                        }
                }

                ply_region_add_rectangle (region, &rectangle);
        }

        list = ply_region_get_rectangle_list (region);
        ply_list_foreach (list, node) {
                ply_rectangle_t *rectangle = ply_list_node_get_data (node);
                long right_edge = rectangle->x + rectangle->width;
                long bottom_edge = rectangle->y + rectangle->height;

                PLY_TEST_ASSERT (!ply_rectangle_is_empty (rectangle));
                PLY_TEST_ASSERT (rectangle->x >= 0);
                PLY_TEST_ASSERT (rectangle->y >= 0);
                PLY_TEST_ASSERT (right_edge <= GRID_SIZE);
                PLY_TEST_ASSERT (bottom_edge <= GRID_SIZE);

                for (y = rectangle->y; y < bottom_edge; y++) {
                        for (x = rectangle->x; x < right_edge; x++) {
                                observed[y][x]++;
                        }
                }
        }

        for (y = 0; y < GRID_SIZE; y++) {
                for (x = 0; x < GRID_SIZE; x++) {
                        PLY_TEST_ASSERT (observed[y][x] <= 1);
                        PLY_TEST_ASSERT ((observed[y][x] == 1) == expected[y][x]);
                }
        }

        ply_region_free (region);
        return true;
}

static const ply_test_case_t test_cases[] =
{
        PLY_TEST_CASE (test_new_region_is_empty),
        PLY_TEST_CASE (test_region_copies_input_and_clears),
        PLY_TEST_CASE (test_sorted_rectangles_have_monotonic_rows),
        PLY_TEST_CASE (test_random_union_matches_cell_oracle),
};

PLY_TEST_MAIN (test_cases)
