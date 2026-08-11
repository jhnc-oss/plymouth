/* plymouthd-output.c - internal captured boot output
 *
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#include "plymouthd-output-private.h"

#include <stdlib.h>

#include "ply-boot-splash.h"
#include "ply-buffer.h"

struct _plymouthd_output
{
        ply_buffer_t *buffer;
};

plymouthd_output_t *
plymouthd_output_new (void)
{
        plymouthd_output_t *output;

        output = calloc (1, sizeof(plymouthd_output_t));
        output->buffer = ply_buffer_new ();

        return output;
}

void
plymouthd_output_free (plymouthd_output_t *output)
{
        if (output == NULL)
                return;

        ply_buffer_free (output->buffer);
        free (output);
}

ply_buffer_t *
plymouthd_output_get_buffer (plymouthd_output_t *output)
{
        return output->buffer;
}

void
plymouthd_output_append (plymouthd_output_t *output,
                         ply_boot_splash_t  *splash,
                         const char         *bytes,
                         size_t              size)
{
        ply_buffer_append_bytes (output->buffer, bytes, size);

        if (splash != NULL)
                ply_boot_splash_update_output (splash, bytes, size);
}
