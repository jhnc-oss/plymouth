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

#include "ply-test.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "ply-event-loop.h"
#include "ply-kmsg-reader.h"

static int next_open_fd;
static int open_calls;
static const char *last_open_path;
static int last_open_flags;

static ssize_t next_read_result;
static int next_read_errno;
static int read_calls;
static int last_read_fd;
static void *last_read_buffer;
static size_t last_read_size;

int
ply_test_kmsg_open (const char *path,
                    int         flags,
                    ...)
{
        int fd;

        open_calls++;
        last_open_path = path;
        last_open_flags = flags;

        fd = next_open_fd;
        next_open_fd = -1;
        return fd;
}

ssize_t
ply_test_kmsg_read (int    fd,
                    void  *buffer,
                    size_t size)
{
        read_calls++;
        last_read_fd = fd;
        last_read_buffer = buffer;
        last_read_size = size;
        errno = next_read_errno;
        return next_read_result;
}

int handle_kmsg_message (ply_kmsg_reader_t *kmsg_reader,
                         int                fd);

static bool
start_reader_on_socket (ply_kmsg_reader_t *kmsg_reader,
                        int                socket_fds[2])
{
        PLY_TEST_ASSERT (socketpair (AF_UNIX,
                                     SOCK_STREAM | SOCK_CLOEXEC,
                                     0,
                                     socket_fds) == 0);

        next_open_fd = socket_fds[0];
        open_calls = 0;
        last_open_path = NULL;
        last_open_flags = 0;

        ply_kmsg_reader_start (kmsg_reader);

        PLY_TEST_ASSERT (open_calls == 1);
        PLY_TEST_ASSERT (strcmp (last_open_path, "/dev/kmsg") == 0);
        PLY_TEST_ASSERT ((last_open_flags & O_ACCMODE) == O_RDWR);
        PLY_TEST_ASSERT ((last_open_flags & O_NONBLOCK) != 0);
        PLY_TEST_ASSERT (next_open_fd == -1);
        PLY_TEST_ASSERT (kmsg_reader->kmsg_fd == socket_fds[0]);
        PLY_TEST_ASSERT (kmsg_reader->fd_watch != NULL);
        return true;
}

static bool
test_terminal_read_failure_clears_reader_state (void)
{
        ply_kmsg_reader_t kmsg_reader = { 0 };
        int socket_fds[2];
        int reader_fd;

        PLY_TEST_ASSERT (start_reader_on_socket (&kmsg_reader, socket_fds));
        reader_fd = kmsg_reader.kmsg_fd;

        next_read_result = -1;
        next_read_errno = EIO;
        read_calls = 0;
        last_read_fd = -1;
        last_read_buffer = NULL;
        last_read_size = 0;

        PLY_TEST_ASSERT (handle_kmsg_message (&kmsg_reader, reader_fd) == -1);
        PLY_TEST_ASSERT (read_calls == 1);
        PLY_TEST_ASSERT (last_read_fd == reader_fd);
        PLY_TEST_ASSERT (last_read_buffer != NULL);
        PLY_TEST_ASSERT (last_read_size > 0);
        PLY_TEST_ASSERT (kmsg_reader.kmsg_fd == -1);
        PLY_TEST_ASSERT (kmsg_reader.fd_watch == NULL);

        errno = 0;
        PLY_TEST_ASSERT (fcntl (reader_fd, F_GETFD) == -1);
        PLY_TEST_ASSERT (errno == EBADF);

        ply_kmsg_reader_stop (&kmsg_reader);
        close (socket_fds[1]);
        return true;
}

static const ply_test_case_t test_cases[] =
{
        PLY_TEST_CASE (test_terminal_read_failure_clears_reader_state),
};

PLY_TEST_MAIN (test_cases)
