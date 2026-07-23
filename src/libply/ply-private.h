/* ply-private.h - internal compiler annotations
 *
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#ifndef PLY_PRIVATE_H
#define PLY_PRIVATE_H

#if defined(__GNUC__)
#define PLY_PRIVATE __attribute__((visibility ("hidden")))
#else
#define PLY_PRIVATE
#endif

#endif /* PLY_PRIVATE_H */
