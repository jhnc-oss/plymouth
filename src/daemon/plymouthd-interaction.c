/* plymouthd-interaction.c - internal prompt and keystroke state
 *
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#include "plymouthd-interaction-private.h"

#include <stdlib.h>
#include <string.h>

#include "ply-buffer.h"
#include "ply-list.h"
#include "ply-logger.h"
#include "ply-utils.h"

typedef enum
{
        PLYMOUTHD_ENTRY_TRIGGER_TYPE_PASSWORD,
        PLYMOUTHD_ENTRY_TRIGGER_TYPE_QUESTION,
} plymouthd_entry_trigger_type_t;

typedef struct
{
        plymouthd_entry_trigger_type_t type;
        const char                    *prompt;
        ply_trigger_t                 *trigger;
        ply_boot_connection_t         *connection;
} plymouthd_entry_trigger_t;

typedef struct
{
        const char            *keys;
        ply_trigger_t         *trigger;
        ply_boot_connection_t *connection;
} plymouthd_keystroke_watch_t;

struct _plymouthd_interaction
{
        ply_list_t   *keystroke_watches;
        ply_list_t   *entry_triggers;
        ply_buffer_t *entry_buffer;
};

static void
free_list_data (ply_list_t *list)
{
        ply_list_node_t *node;

        while ((node = ply_list_get_first_node (list)) != NULL) {
                free (ply_list_node_get_data (node));
                ply_list_remove_node (list, node);
        }

        ply_list_free (list);
}

plymouthd_interaction_t *
plymouthd_interaction_new (void)
{
        plymouthd_interaction_t *interaction;

        interaction = calloc (1, sizeof(plymouthd_interaction_t));
        interaction->keystroke_watches = ply_list_new ();
        interaction->entry_triggers = ply_list_new ();
        interaction->entry_buffer = ply_buffer_new ();

        return interaction;
}

void
plymouthd_interaction_free (plymouthd_interaction_t *interaction)
{
        if (interaction == NULL)
                return;

        free_list_data (interaction->keystroke_watches);
        free_list_data (interaction->entry_triggers);
        ply_buffer_free (interaction->entry_buffer);
        free (interaction);
}

bool
plymouthd_validate_prompt_input (ply_boot_splash_t *splash,
                                 const char        *entry_text,
                                 const char        *add_text)
{
        if (splash == NULL)
                return true;

        return ply_boot_splash_validate_input (splash, entry_text, add_text);
}

void
plymouthd_interaction_update_display (plymouthd_interaction_t *interaction,
                                      ply_boot_splash_t       *splash)
{
        ply_list_node_t *node;

        if (splash == NULL)
                return;

        node = ply_list_get_first_node (interaction->entry_triggers);
        if (node != NULL) {
                plymouthd_entry_trigger_t *entry_trigger;

                entry_trigger = ply_list_node_get_data (node);
                if (entry_trigger->type == PLYMOUTHD_ENTRY_TRIGGER_TYPE_PASSWORD) {
                        int bullets;

                        bullets = ply_utf8_string_get_length (
                                ply_buffer_get_bytes (interaction->entry_buffer),
                                ply_buffer_get_size (interaction->entry_buffer));
                        bullets = MAX (0, bullets);
                        ply_boot_splash_display_password (splash,
                                                          entry_trigger->prompt,
                                                          bullets);
                        ply_boot_splash_display_prompt (
                                splash,
                                entry_trigger->prompt,
                                ply_buffer_get_bytes (interaction->entry_buffer),
                                true);
                } else if (entry_trigger->type == PLYMOUTHD_ENTRY_TRIGGER_TYPE_QUESTION) {
                        ply_boot_splash_display_question (
                                splash,
                                entry_trigger->prompt,
                                ply_buffer_get_bytes (interaction->entry_buffer));
                        ply_boot_splash_display_prompt (
                                splash,
                                entry_trigger->prompt,
                                ply_buffer_get_bytes (interaction->entry_buffer),
                                false);
                } else {
                        ply_trace ("unkown entry type");
                }
        } else {
                ply_boot_splash_display_normal (splash);
        }
}

static void
queue_entry (plymouthd_interaction_t       *interaction,
             ply_boot_splash_t             *splash,
             plymouthd_entry_trigger_type_t type,
             const char                    *prompt,
             ply_trigger_t                 *answer,
             ply_boot_connection_t         *connection)
{
        plymouthd_entry_trigger_t *entry_trigger;

        entry_trigger = calloc (1, sizeof(plymouthd_entry_trigger_t));
        entry_trigger->type = type;
        entry_trigger->prompt = prompt;
        entry_trigger->trigger = answer;
        entry_trigger->connection = connection;
        ply_list_append_data (interaction->entry_triggers, entry_trigger);
        plymouthd_interaction_update_display (interaction, splash);
}

void
plymouthd_interaction_queue_password (plymouthd_interaction_t *interaction,
                                      ply_boot_splash_t       *splash,
                                      const char              *prompt,
                                      ply_trigger_t           *answer,
                                      ply_boot_connection_t   *connection)
{
        ply_trace ("queuing password request with boot splash");
        queue_entry (interaction,
                     splash,
                     PLYMOUTHD_ENTRY_TRIGGER_TYPE_PASSWORD,
                     prompt,
                     answer,
                     connection);
}

void
plymouthd_interaction_queue_question (plymouthd_interaction_t *interaction,
                                      ply_boot_splash_t       *splash,
                                      const char              *prompt,
                                      ply_trigger_t           *answer,
                                      ply_boot_connection_t   *connection)
{
        ply_trace ("queuing question with boot splash");
        queue_entry (interaction,
                     splash,
                     PLYMOUTHD_ENTRY_TRIGGER_TYPE_QUESTION,
                     prompt,
                     answer,
                     connection);
}

void
plymouthd_interaction_watch_keystroke (plymouthd_interaction_t *interaction,
                                       const char              *keys,
                                       ply_trigger_t           *trigger,
                                       ply_boot_connection_t   *connection)
{
        plymouthd_keystroke_watch_t *keystroke_watch;

        keystroke_watch = calloc (1, sizeof(plymouthd_keystroke_watch_t));
        ply_trace ("watching for keystroke");
        keystroke_watch->keys = keys;
        keystroke_watch->trigger = trigger;
        keystroke_watch->connection = connection;
        ply_list_append_data (interaction->keystroke_watches, keystroke_watch);
}

void
plymouthd_interaction_ignore_keystroke (plymouthd_interaction_t *interaction,
                                        const char              *keys)
{
        ply_list_node_t *node;

        ply_trace ("ignoring for keystroke");

        for (node = ply_list_get_first_node (interaction->keystroke_watches);
             node != NULL;
             node = ply_list_get_next_node (interaction->keystroke_watches, node)) {
                plymouthd_keystroke_watch_t *keystroke_watch;

                keystroke_watch = ply_list_node_get_data (node);
                if ((keystroke_watch->keys == NULL && keys == NULL) ||
                    (keystroke_watch->keys != NULL && keys != NULL &&
                     strcmp (keystroke_watch->keys, keys) == 0)) {
                        ply_trigger_pull (keystroke_watch->trigger, NULL);
                        ply_list_remove_node (interaction->keystroke_watches, node);
                        free (keystroke_watch);
                        return;
                }
        }
}

void
plymouthd_interaction_cancel_connection (plymouthd_interaction_t *interaction,
                                         ply_boot_splash_t       *splash,
                                         ply_boot_connection_t   *connection)
{
        ply_list_node_t *node;

        node = ply_list_get_first_node (interaction->entry_triggers);
        while (node != NULL) {
                ply_list_node_t *next;
                plymouthd_entry_trigger_t *entry_trigger;

                next = ply_list_get_next_node (interaction->entry_triggers, node);
                entry_trigger = ply_list_node_get_data (node);
                if (entry_trigger->connection == connection) {
                        bool is_active;

                        is_active = node == ply_list_get_first_node (interaction->entry_triggers);
                        ply_trace ("cancelling pending prompt for disconnected client");
                        ply_trigger_pull (entry_trigger->trigger, NULL);
                        if (is_active)
                                ply_buffer_clear (interaction->entry_buffer);
                        ply_list_remove_node (interaction->entry_triggers, node);
                        free (entry_trigger);
                        plymouthd_interaction_update_display (interaction, splash);
                }
                node = next;
        }

        node = ply_list_get_first_node (interaction->keystroke_watches);
        while (node != NULL) {
                ply_list_node_t *next;
                plymouthd_keystroke_watch_t *keystroke_watch;

                next = ply_list_get_next_node (interaction->keystroke_watches, node);
                keystroke_watch = ply_list_node_get_data (node);
                if (keystroke_watch->connection == connection) {
                        ply_trace ("cancelling pending keystroke watch for disconnected client");
                        ply_trigger_pull (keystroke_watch->trigger, NULL);
                        ply_list_remove_node (interaction->keystroke_watches, node);
                        free (keystroke_watch);
                }
                node = next;
        }
}

void
plymouthd_interaction_handle_input (plymouthd_interaction_t *interaction,
                                    ply_boot_splash_t       *splash,
                                    const char              *keyboard_input,
                                    size_t                   character_size)
{
        ply_list_node_t *node;

        node = ply_list_get_first_node (interaction->entry_triggers);
        if (node != NULL) {
                if (!plymouthd_validate_prompt_input (
                            splash,
                            ply_buffer_get_bytes (interaction->entry_buffer),
                            keyboard_input))
                        return;

                if (character_size == 1 &&
                    (keyboard_input[0] == '\x3' || keyboard_input[0] == '\x4')) {
                        plymouthd_entry_trigger_t *entry_trigger;

                        entry_trigger = ply_list_node_get_data (node);
                        ply_trigger_pull (entry_trigger->trigger, "\x3");
                        ply_buffer_clear (interaction->entry_buffer);
                        ply_list_remove_node (interaction->entry_triggers, node);
                        free (entry_trigger);
                } else if (character_size >= 2 && keyboard_input[0] == '\033') {
                        /* Ignore escape sequences */
                } else {
                        ply_buffer_append_bytes (interaction->entry_buffer,
                                                 keyboard_input,
                                                 character_size);
                }
                plymouthd_interaction_update_display (interaction, splash);
                return;
        }

        for (node = ply_list_get_first_node (interaction->keystroke_watches);
             node != NULL;
             node = ply_list_get_next_node (interaction->keystroke_watches, node)) {
                plymouthd_keystroke_watch_t *keystroke_watch;

                keystroke_watch = ply_list_node_get_data (node);
                if (keystroke_watch->keys == NULL ||
                    strstr (keystroke_watch->keys, keyboard_input) != NULL) {
                        ply_trigger_pull (keystroke_watch->trigger, keyboard_input);
                        ply_list_remove_node (interaction->keystroke_watches, node);
                        free (keystroke_watch);
                        return;
                }
        }
}

void
plymouthd_interaction_handle_backspace (plymouthd_interaction_t *interaction,
                                        ply_boot_splash_t       *splash)
{
        char *bytes;
        size_t size;
        size_t capacity;

        if (ply_list_get_first_node (interaction->entry_triggers) == NULL)
                return;

        ply_buffer_borrow_bytes (interaction->entry_buffer, &bytes, &size, &capacity) {
                ply_utf8_string_remove_last_character (&bytes, &size);
        }

        plymouthd_interaction_update_display (interaction, splash);
}

void
plymouthd_interaction_handle_enter (plymouthd_interaction_t *interaction,
                                    ply_boot_splash_t       *splash,
                                    const char              *line)
{
        ply_list_node_t *node;

        node = ply_list_get_first_node (interaction->entry_triggers);
        if (node != NULL) {
                plymouthd_entry_trigger_t *entry_trigger;
                const char *reply_text;

                entry_trigger = ply_list_node_get_data (node);
                reply_text = ply_buffer_get_bytes (interaction->entry_buffer);
                if (!plymouthd_validate_prompt_input (splash, reply_text, "\n"))
                        return;

                ply_trigger_pull (entry_trigger->trigger, reply_text);
                ply_buffer_clear (interaction->entry_buffer);
                ply_list_remove_node (interaction->entry_triggers, node);
                free (entry_trigger);
                plymouthd_interaction_update_display (interaction, splash);
                return;
        }

        for (node = ply_list_get_first_node (interaction->keystroke_watches);
             node != NULL;
             node = ply_list_get_next_node (interaction->keystroke_watches, node)) {
                plymouthd_keystroke_watch_t *keystroke_watch;

                keystroke_watch = ply_list_node_get_data (node);
                if (keystroke_watch->keys == NULL ||
                    strstr (keystroke_watch->keys, "\n") != NULL) {
                        ply_trigger_pull (keystroke_watch->trigger, line);
                        ply_list_remove_node (interaction->keystroke_watches, node);
                        free (keystroke_watch);
                        return;
                }
        }
}
