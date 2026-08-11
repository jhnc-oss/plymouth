/* plymouthd-process.c - internal process resource lifecycle
 *
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#include "plymouthd-process-private.h"

#include <execinfo.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include <linux/kd.h>

#include "ply-logger.h"
#include "plymouthd-diagnostics-private.h"

#define BACKTRACE_SIZE 1024
#define MAPS_SIZE 8192
#define BACKTRACE_FRAMES_TO_SKIP 2

struct _plymouthd_process
{
        plymouthd_diagnostics_t *diagnostics;
        char                    *pid_file;
};

static plymouthd_process_t *active_process;

static void
write_maps (int output_fd)
{
        char maps_buffer[MAPS_SIZE];
        ssize_t bytes_read;
        ssize_t line_start = 0, buffer_end = 0;
        int fd;

        write (output_fd, "maps:\n", strlen ("maps:\n"));
        fd = open ("/proc/self/maps", O_RDONLY);

        if (fd < 0)
                return;

        while ((bytes_read = read (fd,
                                   maps_buffer + buffer_end,
                                   MAPS_SIZE - buffer_end)) > 0) {
                ssize_t i;

                bytes_read += buffer_end;
                buffer_end = 0;

                for (i = line_start; i < bytes_read; ++i) {
                        if (maps_buffer[i] == '\n') {
                                write (output_fd,
                                       maps_buffer + line_start,
                                       i - line_start + 1);
                                line_start = i + 1;
                        }
                }

                if (line_start < bytes_read) {
                        memmove (maps_buffer,
                                 maps_buffer + line_start,
                                 bytes_read - line_start);
                        buffer_end = bytes_read - line_start;
                        line_start = 0;
                } else {
                        line_start = 0;
                }
        }

        if (buffer_end > 0)
                write (output_fd, maps_buffer, buffer_end);

        close (fd);
}

static void
write_backtrace (int output_fd)
{
        void *addresses[BACKTRACE_SIZE];
        int number_of_addresses;

        write (output_fd, "backtrace:\n", strlen ("backtrace:\n"));
        number_of_addresses = backtrace (addresses, BACKTRACE_SIZE);

        if (number_of_addresses <= BACKTRACE_FRAMES_TO_SKIP)
                return;

        backtrace_symbols_fd (addresses + BACKTRACE_FRAMES_TO_SKIP,
                              number_of_addresses - BACKTRACE_FRAMES_TO_SKIP,
                              output_fd);
}

void
plymouthd_process_remove_pid_file (plymouthd_process_t *process)
{
        if (process == NULL || process->pid_file == NULL)
                return;

        unlink (process->pid_file);
        free (process->pid_file);
        process->pid_file = NULL;
}

static void
on_crash (int signum)
{
        struct termios term_attributes;
        plymouthd_process_t *process;
        int fd;
        static const char *show_cursor_sequence = "\033[?25h";

        process = active_process;
        fd = -1;

        if (process != NULL)
                fd = plymouthd_diagnostics_get_crash_fd (process->diagnostics);

        if (fd == -1) {
                fd = open ("/dev/tty1", O_RDWR | O_NOCTTY);
                if (fd < 0)
                        fd = open ("/dev/hvc0", O_RDWR | O_NOCTTY);
        }

        if (fd >= 0) {
                ioctl (fd, KDSETMODE, KD_TEXT);
                write (fd,
                       show_cursor_sequence,
                       sizeof("\033[?25h") - 1);

                tcgetattr (fd, &term_attributes);
                term_attributes.c_iflag |= BRKINT | IGNPAR | ICRNL | IXON;
                term_attributes.c_oflag |= OPOST;
                term_attributes.c_lflag |= ECHO | ICANON | ISIG | IEXTEN;
                tcsetattr (fd, TCSAFLUSH, &term_attributes);

                write_maps (fd);
                write_backtrace (fd);
        }

        if (process != NULL &&
            plymouthd_diagnostics_has_buffer (process->diagnostics)) {
                plymouthd_diagnostics_dump (process->diagnostics);
                sleep (30);
        }

        plymouthd_process_remove_pid_file (process);

        signal (signum, SIG_DFL);
        raise (signum);
}

plymouthd_process_t *
plymouthd_process_new (ply_boot_splash_mode_t mode,
                       const char            *default_tty,
                       const char            *debug_path,
                       bool                   capture_debug,
                       char                  *pid_file)
{
        plymouthd_process_t *process;

        process = calloc (1, sizeof(plymouthd_process_t));
        process->diagnostics = plymouthd_diagnostics_new (mode,
                                                          default_tty,
                                                          debug_path,
                                                          capture_debug);
        process->pid_file = pid_file;

        return process;
}

void
plymouthd_process_free (plymouthd_process_t *process)
{
        if (process == NULL)
                return;

        if (active_process == process)
                active_process = NULL;

        plymouthd_process_dump_diagnostics (process);
        ply_free_error_log ();
        plymouthd_diagnostics_free (process->diagnostics);
        plymouthd_process_remove_pid_file (process);
        free (process);
}

void
plymouthd_process_install_crash_handlers (plymouthd_process_t *process)
{
        active_process = process;
        signal (SIGABRT, on_crash);
        signal (SIGSEGV, on_crash);
        signal (SIGFPE, on_crash);
}

bool
plymouthd_process_has_diagnostics (plymouthd_process_t *process)
{
        return process != NULL &&
               plymouthd_diagnostics_has_buffer (process->diagnostics);
}

void
plymouthd_process_dump_diagnostics (plymouthd_process_t *process)
{
        if (process == NULL)
                return;

        plymouthd_diagnostics_dump (process->diagnostics);
}

void
plymouthd_process_write_pid_file (plymouthd_process_t *process)
{
        FILE *fp;

        if (process == NULL || process->pid_file == NULL)
                return;

        fp = fopen (process->pid_file, "w");
        if (fp == NULL) {
                ply_error ("could not write pid file %s: %m", process->pid_file);
                return;
        }

        fprintf (fp, "%d\n", (int) getpid ());
        fclose (fp);
}
