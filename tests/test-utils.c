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

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "ply-utils.h"

static bool
test_string_boundaries_and_copy (void)
{
        const char *source[] = { "first", "second", NULL };
        char **copy;

        PLY_TEST_ASSERT (ply_string_has_prefix ("plymouth", "ply"));
        PLY_TEST_ASSERT (ply_string_has_prefix ("plymouth", ""));
        PLY_TEST_ASSERT (!ply_string_has_prefix ("plymouth", "mouth"));
        PLY_TEST_ASSERT (!ply_string_has_prefix (NULL, "ply"));
        PLY_TEST_ASSERT (ply_string_has_suffix ("plymouth", "mouth"));
        PLY_TEST_ASSERT (ply_string_has_suffix ("plymouth", ""));
        PLY_TEST_ASSERT (!ply_string_has_suffix ("ply", "plymouth"));
        PLY_TEST_ASSERT (!ply_string_has_suffix ("plymouth", NULL));

        copy = ply_copy_string_array (source);
        PLY_TEST_ASSERT (copy != NULL);
        PLY_TEST_ASSERT (copy[0] != source[0]);
        PLY_TEST_ASSERT (copy[1] != source[1]);
        PLY_TEST_ASSERT (copy[2] == NULL);
        PLY_TEST_ASSERT (strcmp (copy[0], source[0]) == 0);
        PLY_TEST_ASSERT (strcmp (copy[1], source[1]) == 0);

        copy[0][0] = 'F';
        PLY_TEST_ASSERT (strcmp (source[0], "first") == 0);

        ply_free_string_array (copy);
        return true;
}

static bool
test_pipe_io_preserves_uint32_byte_order (void)
{
        static const uint8_t encoded_value[] = { 0xef, 0xcd, 0xab, 0x89 };
        uint8_t bytes[sizeof(encoded_value)];
        uint32_t decoded_value = 0;
        int sender_fd;
        int receiver_fd;

        PLY_TEST_ASSERT (ply_open_unidirectional_pipe (&sender_fd, &receiver_fd));

        PLY_TEST_ASSERT (ply_write_uint32 (sender_fd, UINT32_C (0x89abcdef)));
        PLY_TEST_ASSERT (ply_fd_has_data (receiver_fd));
        PLY_TEST_ASSERT (ply_read (receiver_fd, bytes, sizeof(bytes)));
        PLY_TEST_ASSERT (memcmp (bytes, encoded_value, sizeof(bytes)) == 0);

        PLY_TEST_ASSERT (ply_write (sender_fd,
                                    encoded_value,
                                    sizeof(encoded_value)));
        PLY_TEST_ASSERT (ply_read_uint32 (receiver_fd, &decoded_value));
        PLY_TEST_ASSERT (decoded_value == UINT32_C (0x89abcdef));

        close (sender_fd);
        close (receiver_fd);
        return true;
}

static bool
test_set_fd_as_blocking_removes_nonblocking_flag (void)
{
        int pipe_fds[2];
        int flags;

        PLY_TEST_ASSERT (pipe (pipe_fds) == 0);
        flags = fcntl (pipe_fds[0], F_GETFL);
        PLY_TEST_ASSERT (flags >= 0);
        PLY_TEST_ASSERT (fcntl (pipe_fds[0], F_SETFL, flags | O_NONBLOCK) == 0);

        PLY_TEST_ASSERT (ply_set_fd_as_blocking (pipe_fds[0]));
        flags = fcntl (pipe_fds[0], F_GETFL);
        PLY_TEST_ASSERT (flags >= 0);
        PLY_TEST_ASSERT ((flags & O_NONBLOCK) == 0);

        close (pipe_fds[0]);
        close (pipe_fds[1]);
        return true;
}

static bool
test_socket_credentials_match_process (void)
{
        int socket_fds[2];
        pid_t pid = 0;
        uid_t uid = 0;
        gid_t gid = 0;

        PLY_TEST_ASSERT (socketpair (AF_UNIX, SOCK_STREAM, 0, socket_fds) == 0);
        PLY_TEST_ASSERT (ply_get_credentials_from_fd (socket_fds[0],
                                                      &pid,
                                                      &uid,
                                                      &gid));
        PLY_TEST_ASSERT (pid == getpid ());
        PLY_TEST_ASSERT (uid == getuid ());
        PLY_TEST_ASSERT (gid == getgid ());

        close (socket_fds[0]);
        close (socket_fds[1]);
        return true;
}

static bool
test_utf8_character_types_and_offsets (void)
{
        static const char text[] =
                "A\xc2\xa2\xe2\x82\xac\xf0\x90\x8d\x88";

        PLY_TEST_ASSERT (ply_utf8_character_get_byte_type ('\0') ==
                         PLY_UTF8_CHARACTER_BYTE_TYPE_END_OF_STRING);
        PLY_TEST_ASSERT (ply_utf8_character_get_byte_type ('A') ==
                         PLY_UTF8_CHARACTER_BYTE_TYPE_1_BYTE);
        PLY_TEST_ASSERT (ply_utf8_character_get_byte_type ((char) 0xc2) ==
                         PLY_UTF8_CHARACTER_BYTE_TYPE_2_BYTES);
        PLY_TEST_ASSERT (ply_utf8_character_get_byte_type ((char) 0xe2) ==
                         PLY_UTF8_CHARACTER_BYTE_TYPE_3_BYTES);
        PLY_TEST_ASSERT (ply_utf8_character_get_byte_type ((char) 0xf0) ==
                         PLY_UTF8_CHARACTER_BYTE_TYPE_4_BYTES);
        PLY_TEST_ASSERT (ply_utf8_character_get_byte_type ((char) 0x80) ==
                         PLY_UTF8_CHARACTER_BYTE_TYPE_CONTINUATION);
        PLY_TEST_ASSERT (ply_utf8_character_get_byte_type ((char) 0xff) ==
                         PLY_UTF8_CHARACTER_BYTE_TYPE_INVALID);

        PLY_TEST_ASSERT (ply_utf8_string_get_length (text, strlen (text)) == 4);
        PLY_TEST_ASSERT (ply_utf8_string_get_length (text, 2) == 1);
        PLY_TEST_ASSERT (ply_utf8_string_get_byte_offset_from_character_offset (text, 0) == 0);
        PLY_TEST_ASSERT (ply_utf8_string_get_byte_offset_from_character_offset (text, 1) == 1);
        PLY_TEST_ASSERT (ply_utf8_string_get_byte_offset_from_character_offset (text, 2) == 3);
        PLY_TEST_ASSERT (ply_utf8_string_get_byte_offset_from_character_offset (text, 3) == 6);
        PLY_TEST_ASSERT (ply_utf8_string_get_byte_offset_from_character_offset (text, 4) == 10);

        return true;
}

static bool
test_utf8_iterator_and_removal (void)
{
        static const char text[] =
                "A\xc2\xa2\xe2\x82\xac\xf0\x90\x8d\x88";
        ply_utf8_string_iterator_t iterator;
        const char *character;
        size_t character_size;
        char *mutable_text;
        size_t mutable_size;

        ply_utf8_string_iterator_initialize (&iterator, text, 1, 2);
        PLY_TEST_ASSERT (ply_utf8_string_iterator_next (&iterator,
                                                        &character,
                                                        &character_size));
        PLY_TEST_ASSERT (character_size == 2);
        PLY_TEST_ASSERT (memcmp (character, "\xc2\xa2", 2) == 0);
        PLY_TEST_ASSERT (ply_utf8_string_iterator_next (&iterator,
                                                        &character,
                                                        &character_size));
        PLY_TEST_ASSERT (character_size == 3);
        PLY_TEST_ASSERT (memcmp (character, "\xe2\x82\xac", 3) == 0);
        PLY_TEST_ASSERT (!ply_utf8_string_iterator_next (&iterator,
                                                         &character,
                                                         &character_size));

        mutable_text = strdup (text);
        mutable_size = strlen (mutable_text);
        ply_utf8_string_remove_last_character (&mutable_text, &mutable_size);
        PLY_TEST_ASSERT (mutable_size == 6);
        PLY_TEST_ASSERT (strcmp (mutable_text, "A\xc2\xa2\xe2\x82\xac") == 0);

        free (mutable_text);
        return true;
}

static bool
test_kernel_command_line_uses_token_boundaries (void)
{
        const char *value;
        char *owned_value;

        ply_kernel_command_line_override (
                "notfoo=wrong\tfoo=right plain count=0x2a invalid=12tail");

        value = ply_kernel_command_line_get_string_after_prefix ("foo=");
        PLY_TEST_ASSERT (value != NULL);
        PLY_TEST_ASSERT (strncmp (value, "right", strlen ("right")) == 0);
        PLY_TEST_ASSERT (ply_kernel_command_line_has_argument ("plain"));
        PLY_TEST_ASSERT (!ply_kernel_command_line_has_argument ("lain"));
        PLY_TEST_ASSERT (!ply_kernel_command_line_has_argument ("missing"));

        owned_value = ply_kernel_command_line_get_key_value ("foo=");
        PLY_TEST_ASSERT (owned_value != NULL);
        PLY_TEST_ASSERT (strcmp (owned_value, "right") == 0);
        free (owned_value);

        PLY_TEST_ASSERT (ply_kernel_command_line_get_ulong ("count=", 7) == 42);
        PLY_TEST_ASSERT (ply_kernel_command_line_get_ulong ("invalid=", 7) == 7);
        PLY_TEST_ASSERT (ply_kernel_command_line_get_ulong ("missing=", 7) == 7);

        return true;
}

static const ply_test_case_t test_cases[] =
{
        PLY_TEST_CASE (test_string_boundaries_and_copy),
        PLY_TEST_CASE (test_pipe_io_preserves_uint32_byte_order),
        PLY_TEST_CASE (test_set_fd_as_blocking_removes_nonblocking_flag),
        PLY_TEST_CASE (test_socket_credentials_match_process),
        PLY_TEST_CASE (test_utf8_character_types_and_offsets),
        PLY_TEST_CASE (test_utf8_iterator_and_removal),
        PLY_TEST_CASE (test_kernel_command_line_uses_token_boundaries),
};

PLY_TEST_MAIN (test_cases)
