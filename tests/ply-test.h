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

#ifndef PLY_TEST_H
#define PLY_TEST_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

typedef bool (*ply_test_function_t) (void);

typedef struct
{
        const char         *name;
        ply_test_function_t function;
} ply_test_case_t;

#define PLY_TEST_CASE(function) { #function, function }

#define PLY_TEST_ASSERT(expression)                                           \
        do {                                                                  \
                if (!(expression)) {                                          \
                        fprintf (stderr,                                      \
                                 "%s:%d: assertion failed: %s\n",            \
                                 __FILE__, __LINE__, #expression);             \
                        return false;                                         \
                }                                                             \
        } while (0)

static inline int
ply_test_run (const ply_test_case_t *test_cases,
              size_t                 number_of_test_cases)
{
        size_t number_of_failures = 0;

        printf ("TAP version 13\n");
        printf ("1..%zu\n", number_of_test_cases);

        for (size_t i = 0; i < number_of_test_cases; i++) {
                bool passed;

                passed = test_cases[i].function ();
                printf ("%s %zu - %s\n",
                        passed ? "ok" : "not ok",
                        i + 1,
                        test_cases[i].name);

                if (!passed)
                        number_of_failures++;
        }

        return number_of_failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

#define PLY_TEST_MAIN(test_cases)                                             \
        int                                                                   \
        main (void)                                                           \
        {                                                                     \
                return ply_test_run (test_cases,                              \
                                     sizeof(test_cases) /                     \
                                     sizeof((test_cases)[0]));                \
        }

#endif /* PLY_TEST_H */
