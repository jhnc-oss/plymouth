/* ply-kmsg-reader.c - kernel log message reader
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
 */

#include "ply-list.h"
#include "ply-kmsg-reader.h"
#include "ply-event-loop.h"
#include "ply-logger.h"
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#include <stdio.h>

size_t
unhexmangle_to_buffer (const char *s,
                       char       *buf,
                       size_t      len)
{
        size_t sz = 0;
        const char *buf0 = buf;

        if (!s)
                return 0;

        while (*s && sz < len - 1) {
                if (*s == '\\' && sz + 3 < len - 1 && s[1] == 'x' &&
                    isxdigit (s[2]) && isxdigit (s[3])) {
                        *buf++ = from_hex (s[2]) << 4 | from_hex (s[3]);
                        s += 4;
                        sz += 4;
                } else {
                        *buf++ = *s++;
                        sz++;
                }
        }
        *buf = '\0';
        return buf - buf0 + 1;
}

int
handle_kmsg_message (ply_kmsg_reader_t *kmsg_reader,
                     int                fd)
{
        ssize_t bytes_read;
        ssize_t alloc_size = 8192 + 1 * sizeof(char);
        char *read_buffer = calloc (1, alloc_size);

        bytes_read = read (fd, read_buffer, alloc_size);
        if (bytes_read > 0) {
                char *fields, *field_prefix, *field_sequence, *field_timestamp, *priority_name, *facility_name, *message, *message_substr, *saveptr;
                int prefix, priority, facility;
                uint64_t sequence;
                unsigned long long timestamp;
                kmsg_message_t *kmsg_message;

                fields = strtok_r (read_buffer, ";", &message);

                unhexmangle_to_buffer (message, (char *) message, strlen (message));

                field_prefix = strtok_r (fields, ",", &fields);
                field_sequence = strtok_r (fields, ",", &fields);
                field_timestamp = strtok_r (fields, ",", &fields);

                prefix = atoi (field_prefix);
                sequence = strtoull (field_sequence, NULL, 0);
                timestamp = strtoull (field_timestamp, NULL, 0);

                priority = prefix & 7;
                facility = prefix >> 3;
                priority_name = (char *) priority_names[priority].name;
                facility_name = (char *) facility_names[facility].name;

                message_substr = strtok_r (message, "\n", &saveptr);
                while (message_substr != NULL) {
                        kmsg_message = calloc (1, sizeof(kmsg_message_t));

                        kmsg_message->priority = priority;
                        kmsg_message->facility = facility;
                        kmsg_message->priority_name = priority_name;
                        kmsg_message->facility_name = facility_name;
                        kmsg_message->sequence = sequence;
                        kmsg_message->timestamp = timestamp;
                        kmsg_message->message = message_substr;

                        ply_list_append_data (kmsg_reader->kmsg_messages, kmsg_message);

                        message_substr = strtok_r (NULL, "\n", &saveptr);
                }

                ply_trigger_pull (kmsg_reader->kmsg_trigger, NULL);

                return 0;
        } else {
                return -1;
        }
}

void
ply_kmsg_reader_start (ply_kmsg_reader_t *kmsg_reader)
{
        //dmesg source from util-linux says Linux 3.4 and older fail handling /dev/knsg
        if (handle_kmsg_message (kmsg_reader, kmsg_reader->kmsg_fd) < 0) {
                close (kmsg_reader->kmsg_fd);
                return;
        }

        kmsg_reader->fd_watch = ply_event_loop_watch_fd (ply_event_loop_get_default (), kmsg_reader->kmsg_fd, PLY_EVENT_LOOP_FD_STATUS_HAS_DATA,
                                                         (ply_event_handler_t) handle_kmsg_message,
                                                         NULL,
                                                         kmsg_reader);
}
