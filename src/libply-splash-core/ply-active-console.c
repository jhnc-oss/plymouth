/* ply-active-console.c - internal active console reader
 *
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#include "ply-active-console-private.h"

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

bool
ply_active_console_list_load (const char                *path,
                              ply_active_console_list_t *consoles)
{
        char contents[512] = "";
        const char *cursor;
        ssize_t contents_length;
        int fd;

        memset (consoles, 0, sizeof(*consoles));

        fd = open (path, O_RDONLY | O_CLOEXEC);
        if (fd < 0)
                return false;

        contents_length = read (fd, contents, sizeof(contents) - 1);
        close (fd);

        if (contents_length <= 0)
                return false;

        cursor = contents;
        while (*cursor != '\0') {
                size_t name_length;

                cursor += strspn (cursor, " \n\t\v");
                if (*cursor == '\0')
                        break;

                name_length = strcspn (cursor, " \n\t\v");
                consoles->names = reallocarray (consoles->names,
                                                consoles->count + 1,
                                                sizeof(char *));
                consoles->names[consoles->count] = strndup (cursor,
                                                            name_length);
                consoles->count++;
                cursor += name_length;
        }

        return true;
}

void
ply_active_console_list_clear (ply_active_console_list_t *consoles)
{
        size_t i;

        for (i = 0; i < consoles->count; i++) {
                free (consoles->names[i]);
        }

        free (consoles->names);
        memset (consoles, 0, sizeof(*consoles));
}
