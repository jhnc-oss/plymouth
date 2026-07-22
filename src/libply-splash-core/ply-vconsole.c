/* ply-vconsole.c - internal virtual console settings reader
 *
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#include "ply-vconsole-private.h"

#include <stdlib.h>
#include <string.h>

#include "ply-key-file.h"

static char *
strip_quotes (char *value)
{
        char *unquoted_value;
        size_t length;

        if (value == NULL)
                return NULL;

        length = strlen (value);
        if (length < 2 || value[0] != '"' || value[length - 1] != '"')
                return value;

        unquoted_value = strndup (value + 1, length - 2);
        free (value);
        return unquoted_value;
}

bool
ply_vconsole_settings_load (const char              *path,
                            ply_vconsole_settings_t *settings)
{
        ply_key_file_t *key_file;

        memset (settings, 0, sizeof(*settings));

        key_file = ply_key_file_new (path);
        if (!ply_key_file_load_groupless_file (key_file)) {
                ply_key_file_free (key_file);
                return false;
        }

        settings->keymap = strip_quotes (
                ply_key_file_get_value (key_file, NULL, "KEYMAP"));
        settings->xkb_layout = strip_quotes (
                ply_key_file_get_value (key_file, NULL, "XKBLAYOUT"));
        settings->xkb_model = strip_quotes (
                ply_key_file_get_value (key_file, NULL, "XKBMODEL"));
        settings->xkb_variant = strip_quotes (
                ply_key_file_get_value (key_file, NULL, "XKBVARIANT"));
        settings->xkb_options = strip_quotes (
                ply_key_file_get_value (key_file, NULL, "XKBOPTIONS"));

        ply_key_file_free (key_file);
        return true;
}

void
ply_vconsole_settings_clear (ply_vconsole_settings_t *settings)
{
        free (settings->keymap);
        free (settings->xkb_layout);
        free (settings->xkb_model);
        free (settings->xkb_variant);
        free (settings->xkb_options);
        memset (settings, 0, sizeof(*settings));
}
