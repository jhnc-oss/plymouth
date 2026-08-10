/* plymouthd-options.c - internal daemon startup options
 *
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#include "plymouthd-options-private.h"

#include <stdlib.h>

#include "ply-command-parser.h"
#include "plymouthd-policy-private.h"

struct _plymouthd_options
{
        ply_command_parser_t  *parser;
        char                  *boot_log_path;
        char                  *debug_path;
        char                  *kernel_command_line;
        char                  *pid_file;
        char                  *tty;
        ply_boot_splash_mode_t mode;

        bool                   should_help;
        bool                   attach_to_session;
        bool                   no_daemon;
        bool                   debug;
        bool                   no_boot_log;
        bool                   ignore_serial_consoles;
        bool                   graphical_boot;
};

plymouthd_options_t *
plymouthd_options_new (void)
{
        plymouthd_options_t *options;

        options = calloc (1, sizeof(plymouthd_options_t));
        options->parser = ply_command_parser_new ("plymouthd", "Splash server");
        options->mode = PLY_BOOT_SPLASH_MODE_BOOT_UP;

        return options;
}

void
plymouthd_options_free (plymouthd_options_t *options)
{
        if (options == NULL)
                return;

        ply_command_parser_free (options->parser);
        free (options->boot_log_path);
        free (options->debug_path);
        free (options->kernel_command_line);
        free (options->pid_file);
        free (options->tty);
        free (options);
}

bool
plymouthd_options_parse (plymouthd_options_t *options,
                         ply_event_loop_t    *loop,
                         char               **argv,
                         int                  argc)
{
        char *mode_string = NULL;

        ply_command_parser_add_options (
                options->parser,
                "help", "This help message", PLY_COMMAND_OPTION_TYPE_FLAG,
                "attach-to-session", "Redirect console messages from screen to log", PLY_COMMAND_OPTION_TYPE_FLAG,
                "no-daemon", "Do not daemonize", PLY_COMMAND_OPTION_TYPE_FLAG,
                "debug", "Output debugging information", PLY_COMMAND_OPTION_TYPE_FLAG,
                "debug-file", "File to output debugging information to", PLY_COMMAND_OPTION_TYPE_STRING,
                "mode", "Mode is one of: boot-up, shutdown, reboot, updates, "
                "system-upgrade, firmware-upgrade, system-reset", PLY_COMMAND_OPTION_TYPE_STRING,
                "pid-file", "Write the pid of the daemon to a file", PLY_COMMAND_OPTION_TYPE_STRING,
                "kernel-command-line", "Fake kernel command line to use", PLY_COMMAND_OPTION_TYPE_STRING,
                "tty", "TTY to use instead of default", PLY_COMMAND_OPTION_TYPE_STRING,
                "no-boot-log", "Do not write boot log file", PLY_COMMAND_OPTION_TYPE_FLAG,
                "ignore-serial-consoles", "Ignore serial consoles", PLY_COMMAND_OPTION_TYPE_FLAG,
                "graphical-boot", "Use graphical splashes even if the kernel console is not a VT", PLY_COMMAND_OPTION_TYPE_FLAG,
                NULL);

        if (!ply_command_parser_parse_arguments (options->parser,
                                                 loop,
                                                 argv,
                                                 argc))
                return false;

        ply_command_parser_get_options (
                options->parser,
                "help", &options->should_help,
                "attach-to-session", &options->attach_to_session,
                "mode", &mode_string,
                "no-boot-log", &options->no_boot_log,
                "no-daemon", &options->no_daemon,
                "debug", &options->debug,
                "ignore-serial-consoles", &options->ignore_serial_consoles,
                "graphical-boot", &options->graphical_boot,
                "debug-file", &options->debug_path,
                "boot-log", &options->boot_log_path,
                "pid-file", &options->pid_file,
                "tty", &options->tty,
                "kernel-command-line", &options->kernel_command_line,
                NULL);

        if (mode_string != NULL) {
                options->mode = plymouthd_mode_from_string (mode_string);
                if (options->mode == PLY_BOOT_SPLASH_MODE_INVALID)
                        options->mode = PLY_BOOT_SPLASH_MODE_BOOT_UP;
        }

        free (mode_string);
        return true;
}

char *
plymouthd_options_get_help_string (plymouthd_options_t *options)
{
        return ply_command_parser_get_help_string (options->parser);
}

bool
plymouthd_options_should_help (plymouthd_options_t *options)
{
        return options->should_help;
}

bool
plymouthd_options_should_attach_to_session (plymouthd_options_t *options)
{
        return options->attach_to_session;
}

bool
plymouthd_options_should_daemonize (plymouthd_options_t *options)
{
        return !options->no_daemon;
}

bool
plymouthd_options_should_debug (plymouthd_options_t *options)
{
        return options->debug;
}

bool
plymouthd_options_should_ignore_serial_consoles (plymouthd_options_t *options)
{
        return options->ignore_serial_consoles;
}

bool
plymouthd_options_should_use_graphical_boot (plymouthd_options_t *options)
{
        return options->graphical_boot;
}

bool
plymouthd_options_should_log_boot (plymouthd_options_t *options)
{
        return !options->no_boot_log;
}

ply_boot_splash_mode_t
plymouthd_options_get_mode (plymouthd_options_t *options)
{
        return options->mode;
}

const char *
plymouthd_options_get_debug_path (plymouthd_options_t *options)
{
        return options->debug_path;
}

const char *
plymouthd_options_get_boot_log_path (plymouthd_options_t *options)
{
        return options->boot_log_path;
}

const char *
plymouthd_options_get_kernel_command_line (plymouthd_options_t *options)
{
        return options->kernel_command_line;
}

const char *
plymouthd_options_get_tty (plymouthd_options_t *options)
{
        return options->tty;
}

char *
plymouthd_options_take_pid_file (plymouthd_options_t *options)
{
        char *pid_file;

        pid_file = options->pid_file;
        options->pid_file = NULL;
        return pid_file;
}
