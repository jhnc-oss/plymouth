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

#include "ply-hashtable.h"

typedef struct
{
        int count;
        int sum;
} foreach_result_t;

static unsigned int
constant_hash (void *key)
{
        return 0;
}

static void
count_entry (void *key,
             void *data,
             void *user_data)
{
        foreach_result_t *result = user_data;

        result->count++;
        result->sum += *(int *) data;
}

static bool
test_new_table_is_empty (void)
{
        ply_hashtable_t *table;

        table = ply_hashtable_new (NULL, NULL);

        PLY_TEST_ASSERT (ply_hashtable_get_size (table) == 0);
        PLY_TEST_ASSERT (ply_hashtable_lookup (table, (void *) 1) == NULL);
        PLY_TEST_ASSERT (ply_hashtable_remove (table, (void *) 1) == NULL);

        ply_hashtable_free (table);
        return true;
}

static bool
test_direct_table_tracks_insert_and_remove (void)
{
        int first_value = 10;
        int second_value = 20;
        ply_hashtable_t *table;

        table = ply_hashtable_new (NULL, NULL);
        ply_hashtable_insert (table, (void *) 1, &first_value);
        ply_hashtable_insert (table, (void *) 2, &second_value);

        PLY_TEST_ASSERT (ply_hashtable_get_size (table) == 2);
        PLY_TEST_ASSERT (ply_hashtable_lookup (table, (void *) 1) == &first_value);
        PLY_TEST_ASSERT (ply_hashtable_lookup (table, (void *) 2) == &second_value);
        PLY_TEST_ASSERT (ply_hashtable_remove (table, (void *) 1) == &first_value);
        PLY_TEST_ASSERT (ply_hashtable_lookup (table, (void *) 1) == NULL);
        PLY_TEST_ASSERT (ply_hashtable_get_size (table) == 1);

        ply_hashtable_free (table);
        return true;
}

static bool
test_string_table_returns_stored_key (void)
{
        char stored_key[] = "display";
        char lookup_key[] = "display";
        int value = 7;
        ply_hashtable_t *table;
        void *returned_key = NULL;
        void *returned_value = NULL;

        table = ply_hashtable_new (ply_hashtable_string_hash,
                                   ply_hashtable_string_compare);
        ply_hashtable_insert (table, stored_key, &value);

        PLY_TEST_ASSERT (ply_hashtable_lookup_full (table,
                                                    lookup_key,
                                                    &returned_key,
                                                    &returned_value));
        PLY_TEST_ASSERT (returned_key == stored_key);
        PLY_TEST_ASSERT (returned_value == &value);

        ply_hashtable_free (table);
        return true;
}

static bool
test_collision_chain_survives_removal (void)
{
        int keys[] = { 1, 2, 3 };
        int values[] = { 10, 20, 30 };
        ply_hashtable_t *table;

        table = ply_hashtable_new (constant_hash, NULL);
        ply_hashtable_insert (table, &keys[0], &values[0]);
        ply_hashtable_insert (table, &keys[1], &values[1]);
        ply_hashtable_insert (table, &keys[2], &values[2]);

        PLY_TEST_ASSERT (ply_hashtable_remove (table, &keys[1]) == &values[1]);
        PLY_TEST_ASSERT (ply_hashtable_lookup (table, &keys[0]) == &values[0]);
        PLY_TEST_ASSERT (ply_hashtable_lookup (table, &keys[2]) == &values[2]);
        PLY_TEST_ASSERT (ply_hashtable_get_size (table) == 2);

        ply_hashtable_free (table);
        return true;
}

static bool
test_resize_preserves_live_entries (void)
{
        int keys[64];
        int values[64];
        ply_hashtable_t *table;
        int i;

        table = ply_hashtable_new (constant_hash, NULL);
        for (i = 0; i < 64; i++) {
                keys[i] = i;
                values[i] = i * 3;
                ply_hashtable_insert (table, &keys[i], &values[i]);
        }

        for (i = 0; i < 64; i += 2)
                PLY_TEST_ASSERT (ply_hashtable_remove (table, &keys[i]) == &values[i]);

        ply_hashtable_resize (table);

        PLY_TEST_ASSERT (ply_hashtable_get_size (table) == 32);
        for (i = 0; i < 64; i++) {
                if (i % 2 == 0)
                        PLY_TEST_ASSERT (ply_hashtable_lookup (table, &keys[i]) == NULL);
                else
                        PLY_TEST_ASSERT (ply_hashtable_lookup (table, &keys[i]) == &values[i]);
        }

        ply_hashtable_free (table);
        return true;
}

static bool
test_foreach_visits_each_live_entry (void)
{
        int keys[] = { 1, 2, 3 };
        int values[] = { 4, 5, 6 };
        foreach_result_t result = { 0 };
        ply_hashtable_t *table;
        size_t i;

        table = ply_hashtable_new (NULL, NULL);
        for (i = 0; i < sizeof (keys) / sizeof (keys[0]); i++)
                ply_hashtable_insert (table, &keys[i], &values[i]);

        ply_hashtable_foreach (table, count_entry, &result);

        PLY_TEST_ASSERT (result.count == 3);
        PLY_TEST_ASSERT (result.sum == 15);

        ply_hashtable_free (table);
        return true;
}

static const ply_test_case_t test_cases[] =
{
        PLY_TEST_CASE (test_new_table_is_empty),
        PLY_TEST_CASE (test_direct_table_tracks_insert_and_remove),
        PLY_TEST_CASE (test_string_table_returns_stored_key),
        PLY_TEST_CASE (test_collision_chain_survives_removal),
        PLY_TEST_CASE (test_resize_preserves_live_entries),
        PLY_TEST_CASE (test_foreach_visits_each_live_entry),
};

PLY_TEST_MAIN (test_cases)
