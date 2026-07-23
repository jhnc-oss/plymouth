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

#include "ply-list.h"

typedef struct
{
        int key;
        int sequence;
} sortable_item_t;

static int
compare_integers (void *element_a,
                  void *element_b)
{
        int *a = element_a;
        int *b = element_b;

        return *a - *b;
}

static int
compare_sortable_items (void *element_a,
                        void *element_b)
{
        sortable_item_t *a = element_a;
        sortable_item_t *b = element_b;

        return a->key - b->key;
}

static bool
test_new_list_is_empty (void)
{
        ply_list_t *list;

        list = ply_list_new ();

        PLY_TEST_ASSERT (ply_list_get_length (list) == 0);
        PLY_TEST_ASSERT (ply_list_get_first_node (list) == NULL);
        PLY_TEST_ASSERT (ply_list_get_last_node (list) == NULL);
        PLY_TEST_ASSERT (ply_list_get_nth_node (list, -1) == NULL);
        PLY_TEST_ASSERT (ply_list_get_nth_node (list, 0) == NULL);

        ply_list_free (list);
        return true;
}

static bool
test_insertions_preserve_expected_order (void)
{
        int first = 1;
        int second = 2;
        int third = 3;
        int fourth = 4;
        ply_list_t *list;
        ply_list_node_t *second_node;

        list = ply_list_new ();
        second_node = ply_list_append_data (list, &second);
        ply_list_prepend_data (list, &first);
        ply_list_insert_data (list, &third, second_node);
        ply_list_append_data (list, &fourth);

        PLY_TEST_ASSERT (ply_list_get_length (list) == 4);
        PLY_TEST_ASSERT (ply_list_node_get_data (ply_list_get_nth_node (list, 0)) == &first);
        PLY_TEST_ASSERT (ply_list_node_get_data (ply_list_get_nth_node (list, 1)) == &second);
        PLY_TEST_ASSERT (ply_list_node_get_data (ply_list_get_nth_node (list, 2)) == &third);
        PLY_TEST_ASSERT (ply_list_node_get_data (ply_list_get_nth_node (list, 3)) == &fourth);
        PLY_TEST_ASSERT (ply_list_get_nth_node (list, 4) == NULL);
        PLY_TEST_ASSERT (ply_list_get_last_node (list) ==
                         ply_list_get_nth_node (list, 3));

        ply_list_free (list);
        return true;
}

static bool
test_removal_updates_ends_and_search (void)
{
        int first = 1;
        int middle = 2;
        int last = 3;
        ply_list_t *list;

        list = ply_list_new ();
        ply_list_append_data (list, &first);
        ply_list_append_data (list, &middle);
        ply_list_append_data (list, &last);

        ply_list_remove_data (list, &first);
        PLY_TEST_ASSERT (ply_list_get_length (list) == 2);
        PLY_TEST_ASSERT (ply_list_node_get_data (ply_list_get_first_node (list)) == &middle);
        PLY_TEST_ASSERT (ply_list_find_node (list, &first) == NULL);

        ply_list_remove_node (list, ply_list_find_node (list, &last));
        PLY_TEST_ASSERT (ply_list_get_length (list) == 1);
        PLY_TEST_ASSERT (ply_list_get_first_node (list) ==
                         ply_list_get_last_node (list));

        ply_list_remove_all_nodes (list);
        PLY_TEST_ASSERT (ply_list_get_length (list) == 0);
        PLY_TEST_ASSERT (ply_list_get_first_node (list) == NULL);
        PLY_TEST_ASSERT (ply_list_get_last_node (list) == NULL);

        ply_list_free (list);
        return true;
}

static bool
test_sort_orders_elements (void)
{
        int values[] = { 4, 1, 3, 2 };
        ply_list_t *list;
        size_t i;

        list = ply_list_new ();
        for (i = 0; i < sizeof (values) / sizeof (values[0]); i++)
                ply_list_append_data (list, &values[i]);

        ply_list_sort (list, compare_integers);

        for (i = 0; i < sizeof (values) / sizeof (values[0]); i++) {
                int *value;

                value = ply_list_node_get_data (ply_list_get_nth_node (list, i));
                PLY_TEST_ASSERT (*value == (int) i + 1);
        }

        ply_list_free (list);
        return true;
}

static bool
test_stable_sort_retains_equal_element_order (void)
{
        sortable_item_t items[] = {
                { .key = 2, .sequence = 0 },
                { .key = 1, .sequence = 1 },
                { .key = 2, .sequence = 2 },
                { .key = 1, .sequence = 3 },
        };
        static const int expected_sequences[] = { 1, 3, 0, 2 };
        ply_list_t *list;
        size_t i;

        list = ply_list_new ();
        for (i = 0; i < sizeof (items) / sizeof (items[0]); i++)
                ply_list_append_data (list, &items[i]);

        ply_list_sort_stable (list, compare_sortable_items);

        for (i = 0;
             i < sizeof (expected_sequences) / sizeof (expected_sequences[0]);
             i++) {
                sortable_item_t *item;

                item = ply_list_node_get_data (ply_list_get_nth_node (list, i));
                PLY_TEST_ASSERT (item->sequence == expected_sequences[i]);
        }

        ply_list_free (list);
        return true;
}

static const ply_test_case_t test_cases[] =
{
        PLY_TEST_CASE (test_new_list_is_empty),
        PLY_TEST_CASE (test_insertions_preserve_expected_order),
        PLY_TEST_CASE (test_removal_updates_ends_and_search),
        PLY_TEST_CASE (test_sort_orders_elements),
        PLY_TEST_CASE (test_stable_sort_retains_equal_element_order),
};

PLY_TEST_MAIN (test_cases)
