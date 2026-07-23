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

#ifndef PLY_TEST_SEEDED_RANDOM_H
#define PLY_TEST_SEEDED_RANDOM_H

#include <stddef.h>
#include <stdint.h>

typedef struct
{
        uint32_t state;
} ply_test_seeded_random_t;

static inline uint32_t
ply_test_seeded_random_next (ply_test_seeded_random_t *random)
{
        random->state = random->state * UINT32_C (1664525) +
                        UINT32_C (1013904223);
        return random->state;
}

static inline size_t
ply_test_seeded_random_range (ply_test_seeded_random_t *random,
                              size_t                    upper_bound)
{
        return ply_test_seeded_random_next (random) % upper_bound;
}

static inline void
ply_test_seeded_random_fill (ply_test_seeded_random_t *random,
                             uint8_t                  *bytes,
                             size_t                    size)
{
        size_t i;

        for (i = 0; i < size; i++) {
                bytes[i] = (uint8_t) (ply_test_seeded_random_next (random) >> 24);
        }
}

#endif /* PLY_TEST_SEEDED_RANDOM_H */
