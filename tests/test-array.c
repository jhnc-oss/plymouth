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
#include <stdlib.h>

#include "ply-array.h"

static bool
test_pointer_array_starts_empty (void)
{
        ply_array_t *array;
        void *const *elements;

        array = ply_array_new (PLY_ARRAY_ELEMENT_TYPE_POINTER);
        elements = ply_array_get_pointer_elements (array);

        PLY_TEST_ASSERT (ply_array_get_size (array) == 0);
        PLY_TEST_ASSERT (elements[0] == NULL);

        ply_array_free (array);
        return true;
}

static bool
test_pointer_array_preserves_order (void)
{
        int first = 1;
        int second = 2;
        ply_array_t *array;
        void *const *elements;

        array = ply_array_new (PLY_ARRAY_ELEMENT_TYPE_POINTER);
        ply_array_add_pointer_element (array, &first);
        ply_array_add_pointer_element (array, &second);
        elements = ply_array_get_pointer_elements (array);

        PLY_TEST_ASSERT (ply_array_get_size (array) == 2);
        PLY_TEST_ASSERT (elements[0] == &first);
        PLY_TEST_ASSERT (elements[1] == &second);
        PLY_TEST_ASSERT (elements[2] == NULL);

        ply_array_free (array);
        return true;
}

static bool
test_pointer_array_steal_resets_array (void)
{
        int value = 1;
        ply_array_t *array;
        void **stolen_elements;
        void *const *remaining_elements;

        array = ply_array_new (PLY_ARRAY_ELEMENT_TYPE_POINTER);
        ply_array_add_pointer_element (array, &value);
        stolen_elements = ply_array_steal_pointer_elements (array);
        remaining_elements = ply_array_get_pointer_elements (array);

        PLY_TEST_ASSERT (stolen_elements[0] == &value);
        PLY_TEST_ASSERT (stolen_elements[1] == NULL);
        PLY_TEST_ASSERT (ply_array_get_size (array) == 0);
        PLY_TEST_ASSERT (remaining_elements[0] == NULL);

        free (stolen_elements);
        ply_array_free (array);
        return true;
}

static bool
test_uint32_array_handles_zero_elements (void)
{
        ply_array_t *array;
        const uint32_t *elements;

        array = ply_array_new (PLY_ARRAY_ELEMENT_TYPE_UINT32);
        ply_array_add_uint32_element (array, 42);
        ply_array_add_uint32_element (array, 0);
        ply_array_add_uint32_element (array, UINT32_MAX);
        elements = ply_array_get_uint32_elements (array);

        PLY_TEST_ASSERT (ply_array_get_size (array) == 3);
        PLY_TEST_ASSERT (elements[0] == 42);
        PLY_TEST_ASSERT (elements[1] == 0);
        PLY_TEST_ASSERT (elements[2] == UINT32_MAX);
        PLY_TEST_ASSERT (elements[3] == 0);
        PLY_TEST_ASSERT (ply_array_contains_uint32_element (array, 0));
        PLY_TEST_ASSERT (ply_array_contains_uint32_element (array, UINT32_MAX));
        PLY_TEST_ASSERT (!ply_array_contains_uint32_element (array, 7));

        ply_array_free (array);
        return true;
}

static bool
test_uint32_array_steal_resets_array (void)
{
        ply_array_t *array;
        uint32_t *stolen_elements;

        array = ply_array_new (PLY_ARRAY_ELEMENT_TYPE_UINT32);
        ply_array_add_uint32_element (array, 10);
        ply_array_add_uint32_element (array, 20);
        stolen_elements = ply_array_steal_uint32_elements (array);

        PLY_TEST_ASSERT (stolen_elements[0] == 10);
        PLY_TEST_ASSERT (stolen_elements[1] == 20);
        PLY_TEST_ASSERT (stolen_elements[2] == 0);
        PLY_TEST_ASSERT (ply_array_get_size (array) == 0);

        free (stolen_elements);
        ply_array_free (array);
        return true;
}

static const ply_test_case_t test_cases[] =
{
        PLY_TEST_CASE (test_pointer_array_starts_empty),
        PLY_TEST_CASE (test_pointer_array_preserves_order),
        PLY_TEST_CASE (test_pointer_array_steal_resets_array),
        PLY_TEST_CASE (test_uint32_array_handles_zero_elements),
        PLY_TEST_CASE (test_uint32_array_steal_resets_array),
};

PLY_TEST_MAIN (test_cases)
