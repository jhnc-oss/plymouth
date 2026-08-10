/* main.c - plymouth daemon entry point
 *
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>
#include <unistd.h>

#include "ply-event-loop.h"
#include "ply-logger.h"
#include "ply-utils.h"
#include "plymouthd-options-private.h"
#include "plymouthd-private.h"

int
main (int    argc,
      char **argv)
{
        plymouthd_options_t *options;
        plymouthd_t *daemon;
        ply_event_loop_t *loop;
        int exit_code;

        loop = ply_event_loop_get_default ();
        options = plymouthd_options_new ();

        /* Initialize the translations if they are available (!initrd) */
        if (ply_file_exists (PLYMOUTH_LOCALE_DIRECTORY
                             "/nl/LC_MESSAGES/plymouth.mo"))
                setlocale (LC_ALL, "");

        if (!plymouthd_options_parse (options, loop, argv, argc)) {
                char *help_string;

                help_string = plymouthd_options_get_help_string (options);
                ply_error_without_new_line ("%s", help_string);
                free (help_string);
                plymouthd_options_free (options);
                return EX_USAGE;
        }

        if (plymouthd_options_should_help (options)) {
                char *help_string;

                help_string = plymouthd_options_get_help_string (options);
                if (argc < 2)
                        fprintf (stderr, "%s", help_string);
                else
                        printf ("%s", help_string);

                free (help_string);
                plymouthd_options_free (options);
                return EX_OK;
        }

        if (plymouthd_options_should_debug (options) && !ply_is_tracing ())
                ply_toggle_tracing ();

        if (plymouthd_options_get_kernel_command_line (options) != NULL)
                ply_kernel_command_line_override (
                        plymouthd_options_get_kernel_command_line (options));

        if (geteuid () != 0) {
                ply_error ("plymouthd must be run as root user");
                plymouthd_options_free (options);
                return EX_OSERR;
        }

        daemon = plymouthd_new (options, argv[0], &exit_code);
        if (daemon != NULL) {
                exit_code = plymouthd_run (daemon);
                plymouthd_free (daemon);
        }

        plymouthd_options_free (options);
        return exit_code;
}
