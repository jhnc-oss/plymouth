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

#include "ply-array.h"
#include "ply-bitarray.h"

static bool
test_new_bitarray_is_clear (void)
{
        ply_bitarray_t *bitarray;
        int i;

        bitarray = ply_bitarray_new (70);
        PLY_TEST_ASSERT (bitarray != NULL);

        for (i = 0; i < 70; i++) {
                PLY_TEST_ASSERT (ply_bitarray_lookup (bitarray, i) == 0);
        }

        PLY_TEST_ASSERT (ply_bitarray_count (bitarray, 70) == 0);

        ply_bitarray_free (bitarray);
        return true;
}

static bool
test_set_handles_word_boundaries (void)
{
        static const int indexes[] = { 0, 31, 32, 69 };
        ply_bitarray_t *bitarray;
        size_t i;

        bitarray = ply_bitarray_new (70);
        PLY_TEST_ASSERT (bitarray != NULL);

        for (i = 0; i < sizeof(indexes) / sizeof(indexes[0]); i++) {
                ply_bitarray_set (bitarray, indexes[i]);
        }

        for (i = 0; i < sizeof(indexes) / sizeof(indexes[0]); i++) {
                PLY_TEST_ASSERT (ply_bitarray_lookup (bitarray, indexes[i]) == 1);
        }

        PLY_TEST_ASSERT (ply_bitarray_lookup (bitarray, 1) == 0);
        PLY_TEST_ASSERT (ply_bitarray_lookup (bitarray, 30) == 0);
        PLY_TEST_ASSERT (ply_bitarray_lookup (bitarray, 33) == 0);
        PLY_TEST_ASSERT (ply_bitarray_count (bitarray, 70) == 4);

        ply_bitarray_free (bitarray);
        return true;
}

static bool
test_clear_only_removes_selected_bit (void)
{
        ply_bitarray_t *bitarray;

        bitarray = ply_bitarray_new (64);
        PLY_TEST_ASSERT (bitarray != NULL);

        ply_bitarray_set (bitarray, 31);
        ply_bitarray_set (bitarray, 32);
        ply_bitarray_set (bitarray, 63);
        ply_bitarray_clear (bitarray, 32);

        PLY_TEST_ASSERT (ply_bitarray_lookup (bitarray, 31) == 1);
        PLY_TEST_ASSERT (ply_bitarray_lookup (bitarray, 32) == 0);
        PLY_TEST_ASSERT (ply_bitarray_lookup (bitarray, 63) == 1);
        PLY_TEST_ASSERT (ply_bitarray_count (bitarray, 64) == 2);

        ply_bitarray_free (bitarray);
        return true;
}

static bool
test_toggle_round_trip (void)
{
        ply_bitarray_t *bitarray;

        bitarray = ply_bitarray_new (33);
        PLY_TEST_ASSERT (bitarray != NULL);

        ply_bitarray_toggle (bitarray, 32);
        PLY_TEST_ASSERT (ply_bitarray_lookup (bitarray, 32) == 1);

        ply_bitarray_toggle (bitarray, 32);
        PLY_TEST_ASSERT (ply_bitarray_lookup (bitarray, 32) == 0);
        PLY_TEST_ASSERT (ply_bitarray_count (bitarray, 33) == 0);

        ply_bitarray_free (bitarray);
        return true;
}

static const ply_test_case_t test_cases[] =
{
        PLY_TEST_CASE (test_new_bitarray_is_clear),
        PLY_TEST_CASE (test_set_handles_word_boundaries),
        PLY_TEST_CASE (test_clear_only_removes_selected_bit),
        PLY_TEST_CASE (test_toggle_round_trip),
};

PLY_TEST_MAIN (test_cases)
