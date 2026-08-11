/* plymouthd-messages.c - internal splash message backlog
 *
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#include "plymouthd-messages-private.h"

#include <stdlib.h>
#include <string.h>

#include "ply-list.h"
#include "ply-logger.h"

struct _plymouthd_messages
{
        ply_list_t *backlog;
};

plymouthd_messages_t *
plymouthd_messages_new (void)
{
        plymouthd_messages_t *messages;

        messages = calloc (1, sizeof(plymouthd_messages_t));
        messages->backlog = ply_list_new ();

        return messages;
}

void
plymouthd_messages_free (plymouthd_messages_t *messages)
{
        ply_list_node_t *node;

        if (messages == NULL)
                return;

        while ((node = ply_list_get_first_node (messages->backlog)) != NULL) {
                free (ply_list_node_get_data (node));
                ply_list_remove_node (messages->backlog, node);
        }

        ply_list_free (messages->backlog);
        free (messages);
}

void
plymouthd_messages_display (plymouthd_messages_t *messages,
                            ply_boot_splash_t    *splash,
                            const char           *message)
{
        if (splash != NULL) {
                ply_trace ("displaying message %s", message);
                ply_boot_splash_display_message (splash, message);
        } else {
                ply_trace ("not displaying message %s as no splash", message);
        }

        ply_list_append_data (messages->backlog, strdup (message));
}

void
plymouthd_messages_hide (plymouthd_messages_t *messages,
                         ply_boot_splash_t    *splash,
                         const char           *message)
{
        ply_list_node_t *node;

        ply_trace ("hiding message %s", message);

        node = ply_list_get_first_node (messages->backlog);
        while (node != NULL) {
                ply_list_node_t *next_node;
                char *backlog_message;

                backlog_message = ply_list_node_get_data (node);
                next_node = ply_list_get_next_node (messages->backlog, node);

                if (strcmp (backlog_message, message) == 0) {
                        free (backlog_message);
                        ply_list_remove_node (messages->backlog, node);
                        if (splash != NULL)
                                ply_boot_splash_hide_message (splash, message);
                }
                node = next_node;
        }
}

void
plymouthd_messages_replay (plymouthd_messages_t *messages,
                           ply_boot_splash_t    *splash)
{
        ply_list_node_t *node;

        if (splash == NULL) {
                ply_trace ("not displaying messages, since no boot splash");
                return;
        }

        node = ply_list_get_first_node (messages->backlog);
        while (node != NULL) {
                ply_trace ("displaying messages");
                ply_boot_splash_display_message (splash,
                                                 ply_list_node_get_data (node));
                node = ply_list_get_next_node (messages->backlog, node);
        }
}
