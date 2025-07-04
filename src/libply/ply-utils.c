/* ply-utils.c -  random useful functions and macros
 *
 * Copyright (C) 2007 Red Hat, Inc.
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
 *
 * Written by: Ray Strode <rstrode@redhat.com>
 */

#include "ply-utils.h"

#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <locale.h>
#include <math.h>
#include <poll.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/klog.h>
#include <sys/mount.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <time.h>
#include <linux/vt.h>

#include <dlfcn.h>

#include "ply-logger.h"

#ifndef PLY_OPEN_FILE_DESCRIPTORS_DIR
#define PLY_OPEN_FILE_DESCRIPTORS_DIR "/proc/self/fd"
#endif

#ifndef PLY_ERRNO_STACK_SIZE
#define PLY_ERRNO_STACK_SIZE 256
#endif

#ifndef PLY_SUPER_SECRET_LAZY_UNMOUNT_FLAG
#define PLY_SUPER_SECRET_LAZY_UNMOUNT_FLAG 2
#endif

#ifndef PLY_DISABLE_CONSOLE_PRINTK
#define PLY_DISABLE_CONSOLE_PRINTK 6
#endif

#ifndef PLY_ENABLE_CONSOLE_PRINTK
#define PLY_ENABLE_CONSOLE_PRINTK 7
#endif

#ifndef PLY_MAX_COMMAND_LINE_SIZE
#define PLY_MAX_COMMAND_LINE_SIZE 4096
#endif

#define EFI_VARIABLES_PATH "/sys/firmware/efi/efivars/"
#define EFI_GLOBAL_VARIABLES_GUID "8be4df61-93ca-11d2-aa0d-00e098032b8c"
#define SECURE_BOOT_GLOBAL_VARIABLES_FILE EFI_VARIABLES_PATH "SecureBoot-" EFI_GLOBAL_VARIABLES_GUID
#define IS_SECURE_BOOT_ENABLED(sb_config) ((sb_config) == 0x1)

static int errno_stack[PLY_ERRNO_STACK_SIZE];
static int errno_stack_position = 0;

static int overridden_device_scale = 0;

static char kernel_command_line[PLY_MAX_COMMAND_LINE_SIZE];
static bool kernel_command_line_is_set;

static int cached_current_log_level = 0;
static int cached_default_log_level = 0;
static double log_level_update_time = 0.0;

bool
ply_open_unidirectional_pipe (int *sender_fd,
                              int *receiver_fd)
{
        int pipe_fds[2];

        assert (sender_fd != NULL);
        assert (receiver_fd != NULL);

        if (pipe2 (pipe_fds, O_CLOEXEC) < 0)
                return false;

        *sender_fd = pipe_fds[1];
        *receiver_fd = pipe_fds[0];

        return true;
}

static int
ply_open_unix_socket (void)
{
        int fd;
        const int should_pass_credentials = true;

        fd = socket (PF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);

        if (fd < 0)
                return -1;

        if (setsockopt (fd, SOL_SOCKET, SO_PASSCRED,
                        &should_pass_credentials, sizeof(should_pass_credentials)) < 0) {
                ply_save_errno ();
                close (fd);
                ply_restore_errno ();
                return -1;
        }

        return fd;
}

static struct sockaddr *
create_unix_address_from_path (const char            *path,
                               ply_unix_socket_type_t type,
                               size_t                *address_size)
{
        struct sockaddr_un *address;

        assert (path != NULL && path[0] != '\0');
        assert (strlen (path) < sizeof(address->sun_path));

        address = calloc (1, sizeof(struct sockaddr_un));
        address->sun_family = AF_UNIX;

        /* a socket is marked as abstract if its path has the
         * NUL byte at the beginning of the buffer.
         *
         * Note, we depend on the memory being zeroed by the calloc
         * call above.
         */
        if (type == PLY_UNIX_SOCKET_TYPE_CONCRETE)
                strncpy (address->sun_path, path, sizeof(address->sun_path) - 1);
        else
                strncpy (address->sun_path + 1, path, sizeof(address->sun_path) - 1);

        assert (address_size != NULL);

        /* it's very popular to trim the trailing zeros off the end of the path these
         * days for abstract sockets.  Unfortunately, the 0s are part of the name, so
         * both client and server have to agree.
         */
        if (type == PLY_UNIX_SOCKET_TYPE_TRIMMED_ABSTRACT) {
                *address_size = offsetof (struct sockaddr_un, sun_path)
                                + 1 /* NUL */ +
                                strlen (address->sun_path + 1) /* path */;
        } else {
                *address_size = sizeof(struct sockaddr_un);
        }

        return (struct sockaddr *) address;
}

int
ply_connect_to_unix_socket (const char            *path,
                            ply_unix_socket_type_t type)
{
        struct sockaddr *address;
        size_t address_size;
        int fd;

        assert (path != NULL);
        assert (path[0] != '\0');

        fd = ply_open_unix_socket ();

        if (fd < 0)
                return -1;

        address = create_unix_address_from_path (path, type, &address_size);

        if (connect (fd, address, address_size) < 0) {
                ply_save_errno ();
                free (address);
                close (fd);
                ply_restore_errno ();

                return -1;
        }
        free (address);

        return fd;
}

int
ply_listen_to_unix_socket (const char            *path,
                           ply_unix_socket_type_t type)
{
        struct sockaddr *address;
        size_t address_size;
        int fd;

        assert (path != NULL);
        assert (path[0] != '\0');

        fd = ply_open_unix_socket ();

        if (fd < 0)
                return -1;

        address = create_unix_address_from_path (path, type, &address_size);

        if (bind (fd, address, address_size) < 0) {
                ply_save_errno ();
                free (address);
                close (fd);
                ply_restore_errno ();

                return -1;
        }

        free (address);

        if (listen (fd, SOMAXCONN) < 0) {
                ply_save_errno ();
                close (fd);
                ply_restore_errno ();
                return -1;
        }

        if (type == PLY_UNIX_SOCKET_TYPE_CONCRETE) {
                if (fchmod (fd, 0600) < 0) {
                        ply_save_errno ();
                        close (fd);
                        ply_restore_errno ();
                        return -1;
                }
        }

        return fd;
}

bool
ply_get_credentials_from_fd (int    fd,
                             pid_t *pid,
                             uid_t *uid,
                             gid_t *gid)
{
        struct ucred credentials;
        socklen_t credential_size;

        credential_size = sizeof(credentials);
        if (getsockopt (fd, SOL_SOCKET, SO_PEERCRED, &credentials,
                        &credential_size) < 0)
                return false;

        if (credential_size < sizeof(credentials))
                return false;

        if (pid != NULL)
                *pid = credentials.pid;

        if (uid != NULL)
                *uid = credentials.uid;

        if (gid != NULL)
                *gid = credentials.gid;

        return true;
}

bool
ply_write (int         fd,
           const void *buffer,
           size_t      number_of_bytes)
{
        size_t bytes_left_to_write;
        size_t total_bytes_written = 0;

        assert (fd >= 0);

        bytes_left_to_write = number_of_bytes;

        do {
                ssize_t bytes_written = 0;

                bytes_written = write (fd,
                                       ((uint8_t *) buffer) + total_bytes_written,
                                       bytes_left_to_write);

                if (bytes_written > 0) {
                        total_bytes_written += bytes_written;
                        bytes_left_to_write -= bytes_written;
                } else if ((errno != EINTR)) {
                        break;
                }
        } while (bytes_left_to_write > 0);

        return bytes_left_to_write == 0;
}

bool
ply_write_uint32 (int      fd,
                  uint32_t value)
{
        uint8_t buffer[4];

        buffer[0] = (value >> 0) & 0xFF;
        buffer[1] = (value >> 8) & 0xFF;
        buffer[2] = (value >> 16) & 0xFF;
        buffer[3] = (value >> 24) & 0xFF;

        return ply_write (fd, buffer, 4 * sizeof(uint8_t));
}

static ssize_t
ply_read_some_bytes (int    fd,
                     void  *buffer,
                     size_t max_bytes)
{
        size_t bytes_left_to_read;
        size_t total_bytes_read = 0;

        assert (fd >= 0);

        bytes_left_to_read = max_bytes;

        do {
                ssize_t bytes_read = 0;

                bytes_read = read (fd,
                                   ((uint8_t *) buffer) + total_bytes_read,
                                   bytes_left_to_read);

                if (bytes_read > 0) {
                        total_bytes_read += bytes_read;
                        bytes_left_to_read -= bytes_read;
                } else if ((errno != EINTR)) {
                        break;
                }
        } while (bytes_left_to_read > 0);

        if ((bytes_left_to_read > 0) && (errno != EAGAIN))
                total_bytes_read = -1;

        return total_bytes_read;
}

bool
ply_read (int    fd,
          void  *buffer,
          size_t number_of_bytes)
{
        size_t total_bytes_read;
        bool read_was_successful;

        assert (fd >= 0);
        assert (buffer != NULL);
        assert (number_of_bytes != 0);

        total_bytes_read = ply_read_some_bytes (fd, buffer, number_of_bytes);

        read_was_successful = total_bytes_read == number_of_bytes;

        return read_was_successful;
}

bool
ply_read_uint32 (int       fd,
                 uint32_t *value)
{
        uint8_t buffer[4];

        if (!ply_read (fd, buffer, 4 * sizeof(uint8_t)))
                return false;

        *value = (buffer[0] << 0) |
                 (buffer[1] << 8) |
                 (buffer[2] << 16) |
                 (buffer[3] << 24);
        return true;
}

bool
ply_fd_has_data (int fd)
{
        struct pollfd poll_data;
        int result;

        poll_data.fd = fd;
        poll_data.events = POLLIN | POLLPRI;
        poll_data.revents = 0;
        result = poll (&poll_data, 1, 10);

        return result == 1
               && ((poll_data.revents & POLLIN)
                   || (poll_data.revents & POLLPRI));
}

bool
ply_set_fd_as_blocking (int fd)
{
        int flags;
        int ret = 0;

        assert (fd >= 0);

        flags = fcntl (fd, F_GETFL);

        if (flags == -1) {
                return false;
        }

        if (flags & O_NONBLOCK) {
                flags &= ~O_NONBLOCK;

                ret = fcntl (fd, F_SETFL, flags);
        }

        return ret == 0;
}

char **
ply_copy_string_array (const char *const *array)
{
        char **copy;
        int i;

        for (i = 0; array[i] != NULL; i++) {
        }

        copy = calloc (i + 1, sizeof(char *));

        for (i = 0; array[i] != NULL; i++) {
                copy[i] = strdup (array[i]);
        }

        return copy;
}

void
ply_free_string_array (char **array)
{
        int i;

        if (array == NULL)
                return;

        for (i = 0; array[i] != NULL; i++) {
                free (array[i]);
                array[i] = NULL;
        }

        free (array);
}

bool
ply_string_has_prefix (const char *str,
                       const char *prefix)
{
        if (str == NULL || prefix == NULL)
                return false;

        return strncmp (str, prefix, strlen (prefix)) == 0;
}

bool
ply_string_has_suffix (const char *str,
                       const char *suffix)
{
        size_t str_len, suffix_len;

        if (str == NULL || suffix == NULL)
                return false;

        str_len = strlen (str);
        suffix_len = strlen (suffix);

        if (suffix_len > str_len)
                return false;

        return strcmp (str + (str_len - suffix_len), suffix) == 0;
}

double
ply_get_timestamp (void)
{
        const double nanoseconds_per_second = 1000000000.0;
        double timestamp;
        struct timespec now = { 0L, /* zero-filled */ };

        clock_gettime (CLOCK_MONOTONIC, &now);
        timestamp = ((nanoseconds_per_second * now.tv_sec) + now.tv_nsec) /
                    nanoseconds_per_second;

        return timestamp;
}

void
ply_save_errno (void)
{
        assert (errno_stack_position < PLY_ERRNO_STACK_SIZE);
        errno_stack[errno_stack_position] = errno;
        errno_stack_position++;
}

void
ply_restore_errno (void)
{
        assert (errno_stack_position > 0);
        errno_stack_position--;
        errno = errno_stack[errno_stack_position];
}

bool
ply_directory_exists (const char *dir)
{
        struct stat file_info;

        if (stat (dir, &file_info) < 0)
                return false;

        return S_ISDIR (file_info.st_mode);
}

bool
ply_file_exists (const char *file)
{
        struct stat file_info;

        if (stat (file, &file_info) < 0)
                return false;

        return S_ISREG (file_info.st_mode);
}

bool
ply_character_device_exists (const char *device)
{
        struct stat file_info;

        if (stat (device, &file_info) < 0)
                return false;

        return S_ISCHR (file_info.st_mode);
}

ply_module_handle_t *
ply_open_module (const char *module_path)
{
        ply_module_handle_t *handle;

        assert (module_path != NULL);

        handle = (ply_module_handle_t *) dlopen (module_path,
                                                 RTLD_NODELETE | RTLD_NOW | RTLD_LOCAL);

        if (handle == NULL) {
                ply_trace ("Could not load module \"%s\": %s", module_path, dlerror ());
                if (errno == 0)
                        errno = ELIBACC;
        }

        return handle;
}

ply_module_handle_t *
ply_open_built_in_module (void)
{
        ply_module_handle_t *handle;

        handle = (ply_module_handle_t *) dlopen (NULL,
                                                 RTLD_NODELETE | RTLD_NOW | RTLD_LOCAL);

        if (handle == NULL) {
                ply_trace ("Could not load built-in module: %s\n", dlerror ());
                if (errno == 0)
                        errno = ELIBACC;
        }

        return handle;
}

ply_module_function_t
ply_module_look_up_function (ply_module_handle_t *handle,
                             const char          *function_name)
{
        ply_module_function_t function;

        assert (handle != NULL);
        assert (function_name != NULL);

        dlerror ();
        function = (ply_module_function_t) dlsym (handle, function_name);

        if (dlerror () != NULL) {
                if (errno == 0)
                        errno = ELIBACC;

                return NULL;
        }

        return function;
}

void
ply_close_module (ply_module_handle_t *handle)
{
        dlclose (handle);
}

bool
ply_create_directory (const char *directory)
{
        assert (directory != NULL);
        assert (directory[0] != '\0');

        if (ply_directory_exists (directory)) {
                ply_trace ("directory '%s' already exists", directory);
                return true;
        }

        if (ply_file_exists (directory)) {
                ply_trace ("file '%s' is in the way", directory);
                errno = EEXIST;
                return false;
        }

        if (mkdir (directory, 0755) < 0) {
                char *parent_directory;
                char *last_path_component;
                bool is_created;

                is_created = errno == EEXIST;
                if (errno == ENOENT) {
                        parent_directory = strdup (directory);
                        last_path_component = strrchr (parent_directory, '/');
                        *last_path_component = '\0';

                        ply_trace ("parent directory '%s' doesn't exist, creating it first", parent_directory);
                        if (ply_create_directory (parent_directory)
                            && ((mkdir (directory, 0755) == 0) || errno == EEXIST))
                                is_created = true;

                        ply_save_errno ();
                        free (parent_directory);
                        ply_restore_errno ();
                }

                return is_created;
        }


        return true;
}

bool
ply_create_file_link (const char *source,
                      const char *destination)
{
        if (link (source, destination) < 0)
                return false;

        return true;
}

ply_daemon_handle_t *
ply_create_daemon (void)
{
        pid_t pid;
        int sender_fd, receiver_fd;
        int *handle;

        if (!ply_open_unidirectional_pipe (&sender_fd, &receiver_fd))
                return NULL;


        pid = fork ();

        if (pid < 0)
                return NULL;

        if (pid != 0) {
                uint8_t byte;
                close (sender_fd);

                if (!ply_read (receiver_fd, &byte, sizeof(uint8_t))) {
                        int read_error = errno;
                        int status;

                        if (waitpid (pid, &status, WNOHANG) <= 0)
                                ply_error ("failed to read status from child immediately after starting to daemonize: %s", strerror (read_error));
                        else if (WIFEXITED (status))
                                ply_error ("unexpectedly exited with status %d immediately after starting to daemonize", (int) WEXITSTATUS (status));
                        else if (WIFSIGNALED (status))
                                ply_error ("unexpectedly died from signal %s immediately after starting to daemonize", strsignal (WTERMSIG (status)));
                        _exit (1);
                }

                _exit ((int) byte);
        }
        close (receiver_fd);

        handle = calloc (1, sizeof(int));
        *handle = sender_fd;

        return (ply_daemon_handle_t *) handle;
}

bool
ply_detach_daemon (ply_daemon_handle_t *handle,
                   int                  exit_code)
{
        int sender_fd;
        uint8_t byte;

        assert (handle != NULL);
        assert (exit_code >= 0 && exit_code < 256);

        sender_fd = *(int *) handle;

        byte = (uint8_t) exit_code;

        if (!ply_write (sender_fd, &byte, sizeof(uint8_t)))
                return false;

        close (sender_fd);
        free (handle);

        return true;
}


/*                    UTF-8 encoding
 * 00000000-01111111    00-7F   US-ASCII (single byte)
 * 10000000-10111111    80-BF   Second, third, or fourth byte of a multi-byte sequence
 * 11000000-11011111    C0-DF   Start of 2-byte sequence
 * 11100000-11101111    E0-EF   Start of 3-byte sequence
 * 11110000-11110100    F0-F4   Start of 4-byte sequence
 */
ply_utf8_character_byte_type_t
ply_utf8_character_get_byte_type (const char byte)
{
        ply_utf8_character_byte_type_t byte_type;

        if (byte == '\0')
                byte_type = PLY_UTF8_CHARACTER_BYTE_TYPE_END_OF_STRING;
        else if ((byte & 0x80) == 0x00)
                byte_type = PLY_UTF8_CHARACTER_BYTE_TYPE_1_BYTE;
        else if ((byte & 0xE0) == 0xC0)
                byte_type = PLY_UTF8_CHARACTER_BYTE_TYPE_2_BYTES;
        else if ((byte & 0xF0) == 0xE0)
                byte_type = PLY_UTF8_CHARACTER_BYTE_TYPE_3_BYTES;
        else if ((byte & 0xF8) == 0xF0)
                byte_type = PLY_UTF8_CHARACTER_BYTE_TYPE_4_BYTES;
        else if ((byte & 0xC0) == 0x80)
                byte_type = PLY_UTF8_CHARACTER_BYTE_TYPE_CONTINUATION;
        else
                byte_type = PLY_UTF8_CHARACTER_BYTE_TYPE_INVALID;

        return byte_type;
}

ssize_t
ply_utf8_character_get_size_from_byte_type (ply_utf8_character_byte_type_t byte_type)
{
        size_t size;

        switch (byte_type) {
        case PLY_UTF8_CHARACTER_BYTE_TYPE_1_BYTE:
                size = 1;
                break;
        case PLY_UTF8_CHARACTER_BYTE_TYPE_2_BYTES:
                size = 2;
                break;
        case PLY_UTF8_CHARACTER_BYTE_TYPE_3_BYTES:
                size = 3;
                break;
        case PLY_UTF8_CHARACTER_BYTE_TYPE_4_BYTES:
                size = 4;
                break;
        case PLY_UTF8_CHARACTER_BYTE_TYPE_CONTINUATION:
        case PLY_UTF8_CHARACTER_BYTE_TYPE_INVALID:
        case PLY_UTF8_CHARACTER_BYTE_TYPE_END_OF_STRING:
                size = 0;
                break;
        }
        return size;
}

ssize_t
ply_utf8_character_get_size (const char *bytes)
{
        ply_utf8_character_byte_type_t byte_type;
        ssize_t size;

        byte_type = ply_utf8_character_get_byte_type (bytes[0]);
        size = ply_utf8_character_get_size_from_byte_type (byte_type);

        return size;
}

void
ply_utf8_string_remove_last_character (char  **string,
                                       size_t *size)
{
        char *bytes = *string;
        size_t size_in = *size, end_offset;

        if (size_in == 0)
                return;

        end_offset = size_in - 1;
        do {
                ply_utf8_character_byte_type_t byte_type;

                byte_type = ply_utf8_character_get_byte_type (bytes[end_offset]);

                if (byte_type != PLY_UTF8_CHARACTER_BYTE_TYPE_CONTINUATION) {
                        memset (bytes + end_offset, '\0', size_in - end_offset);
                        *size = end_offset;
                        break;
                }

                if (end_offset == 0)
                        break;

                end_offset--;
        } while (true);
}

int
ply_utf8_string_get_length (const char *string,
                            size_t      n)
{
        size_t count = 0;

        while (true) {
                size_t size = ply_utf8_character_get_size (string);

                if (size == 0)
                        break;

                if (size > n)
                        break;

                string += size;
                n -= size;
                count++;
        }
        return count;
}

size_t
ply_utf8_string_get_byte_offset_from_character_offset (const char *string,
                                                       size_t      character_offset)
{
        size_t byte_offset = 0;
        size_t i;

        for (i = 0; i < character_offset && string[byte_offset] != '\0'; i++) {
                byte_offset += ply_utf8_character_get_size (string + byte_offset);
        }

        return byte_offset;
}

void
ply_utf8_string_iterator_initialize (ply_utf8_string_iterator_t *iterator,
                                     const char                 *string,
                                     ssize_t                     starting_offset,
                                     ssize_t                     range)
{
        size_t byte_offset;

        iterator->character_range = range;
        iterator->string = string;

        byte_offset = ply_utf8_string_get_byte_offset_from_character_offset (string, starting_offset);
        iterator->current_byte_offset = byte_offset;
        iterator->number_characters_iterated = 0;
}

bool
ply_utf8_string_iterator_next (ply_utf8_string_iterator_t *iterator,
                               const char                **character,
                               size_t                     *size)
{
        size_t size_of_current_character;

        if (iterator->number_characters_iterated >= iterator->character_range)
                return false;

        if (iterator->string[iterator->current_byte_offset] == '\0')
                return false;

        size_of_current_character = ply_utf8_character_get_size (iterator->string + iterator->current_byte_offset);

        if (size_of_current_character == 0)
                return false;

        *character = &iterator->string[iterator->current_byte_offset];
        *size = size_of_current_character;

        iterator->current_byte_offset += size_of_current_character;
        iterator->number_characters_iterated++;

        return true;
}

char *
ply_get_process_command_line (pid_t pid)
{
        char *path;
        char *command_line;
        ssize_t bytes_read;
        int fd;
        ssize_t i;

        path = NULL;
        command_line = NULL;

        asprintf (&path, "/proc/%ld/cmdline", (long) pid);

        fd = open (path, O_RDONLY);

        if (fd < 0) {
                ply_trace ("Could not open %s: %m", path);
                goto error;
        }

        command_line = calloc (PLY_MAX_COMMAND_LINE_SIZE, sizeof(char));
        bytes_read = read (fd, command_line, PLY_MAX_COMMAND_LINE_SIZE - 1);
        if (bytes_read < 0) {
                ply_trace ("Could not read %s: %m", path);
                close (fd);
                goto error;
        }
        close (fd);
        free (path);

        for (i = 0; i < bytes_read - 1; i++) {
                if (command_line[i] == '\0')
                        command_line[i] = ' ';
        }
        command_line[i] = '\0';

        return command_line;

error:
        free (path);
        free (command_line);
        return NULL;
}

pid_t
ply_get_process_parent_pid (pid_t pid)
{
        char *path;
        FILE *fp;
        int ppid;

        asprintf (&path, "/proc/%ld/stat", (long) pid);

        ppid = 0;
        fp = fopen (path, "re");

        if (fp == NULL) {
                ply_trace ("Could not open %s: %m", path);
                goto out;
        }

        if (fscanf (fp, "%*d %*s %*c %d", &ppid) != 1) {
                ply_trace ("Could not parse %s: %m", path);
                goto out;
        }

        if (ppid <= 0) {
                ply_trace ("%s is returning invalid parent pid %d", path, ppid);
                ppid = 0;
                goto out;
        }

out:
        free (path);

        if (fp != NULL)
                fclose (fp);

        return (pid_t) ppid;
}

void
ply_set_device_scale (int device_scale)
{
        overridden_device_scale = device_scale;
        ply_trace ("Device scale is set to %d", device_scale);
}

/*
 * If we have guessed the scale once, keep guessing to avoid
 * changing the scale on simpledrm -> native driver switch.
 */
static bool guess_device_scale;

static int
get_device_scale_guess (uint32_t width,
                        uint32_t height)
{
        double aspect;

        /* Swap width <-> height for portrait screens */
        if (height > width) {
                uint32_t tmp = width;
                width = height;
                height = tmp;
        }

        /*
         * Special case for 3:2 screens which are only used in mobile form
         * factors, with a lower threshold to enable 2x hiDPI scaling.
         */
        aspect = (double) width / height;
        if (aspect == 1.5)
                return (width >= 1800 &&
                        height >= 1200) ? 2 : 1;

        return (width >= 2880 &&
                height >= 1620) ? 2 : 1;
}

static int
get_device_scale (uint32_t width,
                  uint32_t height,
                  uint32_t width_mm,
                  uint32_t height_mm,
                  bool     guess)
{
        int device_scale, target_dpi;
        float diag_inches, diag_pixels, physical_dpi, perfect_scale;
        const char *force_device_scale;

        device_scale = 1;

        if ((force_device_scale = getenv ("PLYMOUTH_FORCE_SCALE")))
                return strtoul (force_device_scale, NULL, 0);

        if (overridden_device_scale != 0)
                return overridden_device_scale;

        if (guess)
                return get_device_scale_guess (width, height);

        /* Somebody encoded the aspect ratio (16/9 or 16/10)
         * instead of the physical size */
        if ((width_mm == 160 && height_mm == 90) ||
            (width_mm == 160 && height_mm == 100) ||
            (width_mm == 16 && height_mm == 9) ||
            (width_mm == 16 && height_mm == 10))
                return 1;

        if (width_mm == 0 || height_mm == 0)
                return 1;

        diag_inches = sqrtf (width_mm * width_mm + height_mm * height_mm) /
                      25.4f;
        diag_pixels = sqrtf (width * width + height * height);
        physical_dpi = diag_pixels / diag_inches;

        /* These constants are copied from Mutter's meta-monitor.c in order
         * to match the default scale choice of the login screen.
         */
        target_dpi = (diag_inches >= 20.f) ? 110 : 135;

        perfect_scale = physical_dpi / target_dpi;
        device_scale = (perfect_scale > 1.625f) ? 2 : 1;

        return device_scale;
}

int
ply_get_device_scale (uint32_t width,
                      uint32_t height,
                      uint32_t width_mm,
                      uint32_t height_mm)
{
        return get_device_scale (width, height, width_mm, height_mm,
                                 guess_device_scale);
}

int ply_guess_device_scale (uint32_t width,
                            uint32_t height)
{
        guess_device_scale = true;
        return get_device_scale (width, height, 0, 0, true);
}

static void
ply_get_kmsg_log_levels_uncached (int *current_log_level,
                                  int *default_log_level)
{
        char log_levels[4096] = "";
        char *field, *fields;
        int fd;

        ply_trace ("opening /proc/sys/kernel/printk");
        fd = open ("/proc/sys/kernel/printk", O_RDONLY);

        if (fd < 0) {
                ply_trace ("couldn't open it: %m");
                return;
        }

        ply_trace ("reading kmsg log levels");
        if (read (fd, log_levels, sizeof(log_levels) - 1) < 0) {
                ply_trace ("couldn't read it: %m");
                close (fd);
                return;
        }
        close (fd);

        field = strtok_r (log_levels, " \t", &fields);

        if (field == NULL) {
                ply_trace ("Couldn't parse current log level: %m");
                return;
        }

        *current_log_level = atoi (field);

        field = strtok_r (NULL, " \t", &fields);

        if (field == NULL) {
                ply_trace ("Couldn't parse default log level: %m");
                return;
        }

        *default_log_level = atoi (field);
}

void
ply_get_kmsg_log_levels (int *current_log_level,
                         int *default_log_level)
{
        double current_time;
        bool no_cache;
        bool cache_expired;

        no_cache = cached_current_log_level == 0 || cached_default_log_level == 0;
        current_time = ply_get_timestamp ();
        cache_expired = log_level_update_time > 0.0 && (current_time - log_level_update_time) >= 1.0;

        if (no_cache || cache_expired) {
                ply_get_kmsg_log_levels_uncached (&cached_current_log_level, &cached_default_log_level);
                log_level_update_time = current_time;
        }

        *current_log_level = cached_current_log_level;
        *default_log_level = cached_default_log_level;
}

void
ply_show_new_kernel_messages (bool should_show)
{
        int type;

        if (should_show) {
                type = PLY_ENABLE_CONSOLE_PRINTK;

                cached_current_log_level = cached_default_log_level = 0;
                log_level_update_time = 0.0;
        } else {
                type = PLY_DISABLE_CONSOLE_PRINTK;

                ply_get_kmsg_log_levels_uncached (&cached_current_log_level, &cached_default_log_level);
                log_level_update_time = -1.0; /* Disable expiration */
        }

        if (klogctl (type, NULL, 0) < 0)
                ply_trace ("could not toggle printk visibility: %m");
}

static const char *
ply_get_kernel_command_line (void)
{
        const char *remaining_command_line;
        char *key;
        int fd;

        if (kernel_command_line_is_set)
                return kernel_command_line;

        ply_trace ("opening /proc/cmdline");
        fd = open ("/proc/cmdline", O_RDONLY);

        if (fd < 0) {
                ply_trace ("couldn't open it: %m");
                return NULL;
        }

        ply_trace ("reading kernel command line");
        if (read (fd, kernel_command_line, sizeof(kernel_command_line) - 1) < 0) {
                ply_trace ("couldn't read it: %m");
                close (fd);
                return NULL;
        }

        /* we now use plymouth.argument for kernel commandline arguments.
         * It used to be plymouth:argument. This bit just rewrites all : to be .
         */
        remaining_command_line = kernel_command_line;
        while ((key = strstr (remaining_command_line, "plymouth:")) != NULL) {
                char *colon;

                colon = key + strlen ("plymouth");
                *colon = '.';

                remaining_command_line = colon + 1;
        }
        ply_trace ("Kernel command line is: '%s'", kernel_command_line);

        close (fd);

        kernel_command_line_is_set = true;
        return kernel_command_line;
}

const char *
ply_kernel_command_line_get_string_after_prefix (const char *prefix)
{
        const char *command_line = ply_get_kernel_command_line ();
        char *argument;

        if (!command_line)
                return NULL;

        argument = strstr (command_line, prefix);

        if (argument == NULL)
                return NULL;

        if (argument == command_line ||
            argument[-1] == ' ')
                return argument + strlen (prefix);

        return NULL;
}

bool
ply_kernel_command_line_has_argument (const char *argument)
{
        const char *string;

        string = ply_kernel_command_line_get_string_after_prefix (argument);

        if (string == NULL)
                return false;

        if (!isspace ((int) string[0]) && string[0] != '\0')
                return false;

        return true;
}

char *
ply_kernel_command_line_get_key_value (const char *key)
{
        const char *value;

        value = ply_kernel_command_line_get_string_after_prefix (key);
        if (value == NULL || value[0] == '\0')
                return NULL;

        return strndup (value, strcspn (value, " \n"));
}

unsigned long
ply_kernel_command_line_get_ulong (const char   *key,
                                   unsigned long default_value)
{
        const char *raw_value;
        char *endptr = NULL;
        unsigned long u;

        raw_value = ply_kernel_command_line_get_string_after_prefix (key);
        if (raw_value == NULL || raw_value[0] == '\0')
                return default_value;

        u = strtoul (raw_value, &endptr, 0);
        if (!isspace ((int) *endptr) && *endptr != '\0') {
                ply_trace ("'%s' argument '%s' is not a valid unsigned number",
                           key, raw_value);
                return default_value;
        }

        return u;
}

void
ply_kernel_command_line_override (const char *command_line)
{
        strncpy (kernel_command_line, command_line, sizeof(kernel_command_line));
        kernel_command_line[sizeof(kernel_command_line) - 1] = '\0';
        kernel_command_line_is_set = true;
}

char *
ply_get_primary_kernel_console_type (void)
{
        int fd;
        int len;
        char proc_consoles[4096] = "";
        size_t return_length;

        if (ply_file_exists ("/proc/consoles")) {
                ply_trace ("opening /proc/consoles");

                fd = open ("/proc/consoles", O_RDONLY);
                len = read (fd, proc_consoles, sizeof(proc_consoles));
                close (fd);

                /* The device type for /dev/ttyS0 is "ttyS".
                 * for /dev/ttynull it is "ttynull", which appears as "ttynull0" in /proc/consoles. Exclude any numbers at the end.
                 */
                for (return_length = 0; return_length < len; return_length++) {
                        /* Read until the next digit or space, the digit should be first */
                        if (isdigit (proc_consoles[return_length]) || isspace (proc_consoles[return_length]))
                                return strndup (proc_consoles, return_length);
                }
        }

        return NULL;
}

double ply_strtod (const char *str)
{
        char *old_locale;
        double ret;

        /* Ensure strtod uses '.' as decimal separator, as we use this in our cfg files. */
        old_locale = setlocale (LC_NUMERIC, "C");
        ret = strtod (str, NULL);
        setlocale (LC_NUMERIC, old_locale);

        return ret;
}

static bool
check_secure_boot_settings (const char *filename)
{
        int fd;
        int len;
        uint8_t buf[5];

        fd = open (filename, O_RDONLY);
        len = read (fd, buf, 5);
        close (fd);

        /* /sys/firmware/efi/efivars/SecureBoot-8be4df61-93ca-11d2-aa0d-00e098032b8c
         * is in a binary format. The file is exactly 5 bytes long and the last byte
         * is the secure boot configuration. If it is 0x1, the secure boot is
         * enabled.
         */
        if (len == 5)
                if (IS_SECURE_BOOT_ENABLED (buf[4]))
                        return true;

        return false;
}

bool
ply_is_secure_boot_enabled (void)
{
        static int is_secure_boot_enabled = -1;

        if (is_secure_boot_enabled != -1)
                return is_secure_boot_enabled;

        if (check_secure_boot_settings (SECURE_BOOT_GLOBAL_VARIABLES_FILE))
                is_secure_boot_enabled = true;
        else
                is_secure_boot_enabled = false;

        return is_secure_boot_enabled;
}

long
ply_get_random_number (long lower_bound,
                       long range)
{
        static bool seed_initialized = false;
        long offset;

        if (!seed_initialized) {
                struct timespec now = { 0L, /* zero-filled */ };

                clock_gettime (CLOCK_TAI, &now);
                srand48 (now.tv_nsec);
                seed_initialized = true;
        }

        offset = (mrand48 () << 32) | (mrand48 () & 0xffffffff);

        offset = labs (offset) % range;

        return lower_bound + offset;
}

bool
ply_change_to_vt_with_fd (int vt_number,
                          int tty_fd)
{
        if (ioctl (tty_fd, VT_ACTIVATE, vt_number) < 0)
                return false;

        return true;
}

bool
ply_change_to_vt (int vt_number)
{
        int fd;
        bool changed_vt;

        fd = open ("/dev/tty0", O_RDWR);

        if (fd < 0)
                return false;

        ply_save_errno ();
        changed_vt = ply_change_to_vt_with_fd (vt_number, fd);
        ply_restore_errno ();
        close (fd);

        return changed_vt;
}

/* vim: set ts=4 sw=4 expandtab autoindent cindent cino={.5s,(0: */
