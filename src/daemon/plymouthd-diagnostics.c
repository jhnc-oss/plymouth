/* plymouthd-diagnostics.c - internal debug capture lifecycle
 *
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#include "plymouthd-diagnostics-private.h"

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ply-buffer.h"
#include "ply-logger.h"
#include "ply-utils.h"

struct _plymouthd_diagnostics
{
        ply_buffer_t *buffer;
        char         *path;
        int           crash_fd;
};

const char *
plymouthd_get_default_diagnostics_path (ply_boot_splash_mode_t mode)
{
        if (mode == PLY_BOOT_SPLASH_MODE_SHUTDOWN ||
            mode == PLY_BOOT_SPLASH_MODE_REBOOT)
                return PLYMOUTH_LOG_DIRECTORY "/plymouth-shutdown-debug.log";

        return PLYMOUTH_LOG_DIRECTORY "/plymouth-debug.log";
}

static void
on_error_message (ply_buffer_t *buffer,
                  const void   *bytes,
                  size_t        number_of_bytes)
{
        ply_buffer_append_bytes (buffer, bytes, number_of_bytes);
}

static void
configure_debug_output (plymouthd_diagnostics_t *diagnostics,
                        const char              *default_tty,
                        const char              *stream)
{
        int fd;

        if (stream != NULL) {
                ply_trace ("streaming debug output to %s instead of screen", stream);
                fd = open (stream, O_RDWR | O_NOCTTY | O_CREAT, 0600);

                if (fd < 0) {
                        ply_trace ("could not stream output to %s: %m", stream);
                        return;
                }

                ply_logger_set_output_fd (ply_logger_get_error_default (), fd);
                diagnostics->crash_fd = fd;
                return;
        }

        char *file;

        ply_trace ("redirecting debug output to %s", default_tty);

        if (strncmp (default_tty, "/dev/", strlen ("/dev/")) == 0)
                file = strdup (default_tty);
        else
                asprintf (&file, "/dev/%s", default_tty);

        fd = open (file, O_RDWR | O_APPEND);

        if (fd < 0)
                ply_trace ("could not redirected debug output to %s: %m", default_tty);
        else
                ply_logger_set_output_fd (ply_logger_get_error_default (), fd);

        free (file);
}

plymouthd_diagnostics_t *
plymouthd_diagnostics_new (ply_boot_splash_mode_t mode,
                           const char            *default_tty,
                           const char            *configured_path,
                           bool                   capture_requested)
{
        plymouthd_diagnostics_t *diagnostics;
        char *stream;
        bool kernel_debug_requested;

        diagnostics = calloc (1, sizeof(plymouthd_diagnostics_t));
        diagnostics->crash_fd = -1;

        if (configured_path != NULL)
                diagnostics->path = strdup (configured_path);
        else
                diagnostics->path =
                        ply_kernel_command_line_get_key_value ("plymouth.debug=file:");

        stream = ply_kernel_command_line_get_key_value ("plymouth.debug=stream:");
        kernel_debug_requested = stream != NULL || diagnostics->path != NULL ||
                                 ply_kernel_command_line_has_argument ("plymouth.debug");

        if (kernel_debug_requested) {
                ply_trace ("tracing should be enabled!");
                if (!ply_is_tracing ())
                        ply_toggle_tracing ();

                configure_debug_output (diagnostics, default_tty, stream);
        } else {
                ply_trace ("tracing shouldn't be enabled!");
        }

        if (capture_requested || kernel_debug_requested)
                diagnostics->buffer = ply_buffer_new ();

        if (diagnostics->buffer != NULL) {
                if (diagnostics->path == NULL) {
                        diagnostics->path = strdup (
                                plymouthd_get_default_diagnostics_path (mode));
                }

                ply_logger_add_filter (ply_logger_get_error_default (),
                                       (ply_logger_filter_handler_t)
                                       on_error_message,
                                       diagnostics->buffer);
        }

        free (stream);
        return diagnostics;
}

void
plymouthd_diagnostics_free (plymouthd_diagnostics_t *diagnostics)
{
        if (diagnostics == NULL)
                return;

        ply_buffer_free (diagnostics->buffer);
        free (diagnostics->path);
        free (diagnostics);
}

bool
plymouthd_diagnostics_has_buffer (plymouthd_diagnostics_t *diagnostics)
{
        return diagnostics != NULL && diagnostics->buffer != NULL;
}

const char *
plymouthd_diagnostics_get_path (plymouthd_diagnostics_t *diagnostics)
{
        return diagnostics->path;
}

int
plymouthd_diagnostics_get_crash_fd (plymouthd_diagnostics_t *diagnostics)
{
        if (diagnostics == NULL)
                return -1;

        return diagnostics->crash_fd;
}

void
plymouthd_diagnostics_dump (plymouthd_diagnostics_t *diagnostics)
{
        const char *bytes;
        size_t size;
        int fd;

        if (!plymouthd_diagnostics_has_buffer (diagnostics))
                return;

        fd = open (diagnostics->path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
        if (fd < 0)
                return;

        size = ply_buffer_get_size (diagnostics->buffer);
        bytes = ply_buffer_get_bytes (diagnostics->buffer);
        ply_write (fd, bytes, size);
        close (fd);
}
