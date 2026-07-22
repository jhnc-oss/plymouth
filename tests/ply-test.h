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
#include <stdio.h>

#define PLY_TEST_ASSERT(expression)                                           \
        do {                                                                  \
                if (!(expression)) {                                          \
                        fprintf (stderr,                                      \
                                 "%s:%d: assertion failed: %s\n",            \
                                 __FILE__, __LINE__, #expression);             \
                        return false;                                         \
                }                                                             \
        } while (0)

#endif /* PLY_TEST_H */
