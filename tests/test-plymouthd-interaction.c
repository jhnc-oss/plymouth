/*
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#include "ply-test.h"

#include <stdint.h>
#include <string.h>

#include "plugins/fake-splash.h"
#include "ply-boot-splash.h"
#include "ply-trigger.h"
#include "ply-utils.h"
#include "plymouthd-interaction-private.h"

typedef struct
{
        int  count;
        bool received_null;
        char response[64];
} response_t;

static void
record_response (void          *user_data,
                 const void    *trigger_data,
                 ply_trigger_t *trigger)
{
        response_t *response = user_data;

        response->count++;
        response->received_null = trigger_data == NULL;
        if (trigger_data != NULL)
                strncpy (response->response,
                         trigger_data,
                         sizeof(response->response) - 1);
}

static const test_splash_plugin_state_t *
get_plugin_state (ply_module_handle_t *module)
{
        test_splash_plugin_get_state_function_t get_state;

        get_state = (test_splash_plugin_get_state_function_t)
                    ply_module_look_up_function (module,
                                                 "test_splash_plugin_get_state");
        if (get_state == NULL)
                return NULL;

        return get_state ();
}

static ply_boot_splash_t *
load_splash (void)
{
        ply_boot_splash_t *splash;

        splash = ply_boot_splash_new (TEST_SPLASH_THEME_PATH,
                                      TEST_SPLASH_PLUGIN_DIR,
                                      NULL);
        if (!ply_boot_splash_load (splash)) {
                ply_boot_splash_free (splash);
                return NULL;
        }

        return splash;
}

static bool
test_password_input_updates_splash_and_completes (void)
{
        const test_splash_plugin_state_t *state;
        plymouthd_interaction_t *interaction;
        ply_module_handle_t *module;
        ply_boot_splash_t *splash;
        ply_trigger_t *answer;
        response_t response = { 0 };

        module = ply_open_module (TEST_SPLASH_PLUGIN_PATH);
        PLY_TEST_ASSERT (module != NULL);
        splash = load_splash ();
        PLY_TEST_ASSERT (splash != NULL);
        interaction = plymouthd_interaction_new ();
        answer = ply_trigger_new (NULL);
        ply_trigger_add_handler (answer, record_response, &response);

        plymouthd_interaction_queue_password (interaction,
                                              splash,
                                              "Password:",
                                              answer,
                                              NULL);

        state = get_plugin_state (module);
        PLY_TEST_ASSERT (state != NULL);
        PLY_TEST_ASSERT (state->password_count == 1);
        PLY_TEST_ASSERT (strcmp (state->password_prompt, "Password:") == 0);
        PLY_TEST_ASSERT (state->password_bullets == 0);
        PLY_TEST_ASSERT (state->prompt_is_secret);

        plymouthd_interaction_handle_input (interaction, splash, "a", 1);
        PLY_TEST_ASSERT (state->validate_count == 1);
        PLY_TEST_ASSERT (state->password_bullets == 1);
        PLY_TEST_ASSERT (strcmp (state->prompt_entry, "a") == 0);

        plymouthd_interaction_handle_input (interaction, splash, "!", 1);
        PLY_TEST_ASSERT (state->validate_count == 2);
        PLY_TEST_ASSERT (state->password_bullets == 1);
        PLY_TEST_ASSERT (strcmp (state->prompt_entry, "a") == 0);

        plymouthd_interaction_handle_input (interaction, splash, "bc", 2);
        plymouthd_interaction_handle_enter (interaction, splash, "ignored");
        PLY_TEST_ASSERT (response.count == 1);
        PLY_TEST_ASSERT (!response.received_null);
        PLY_TEST_ASSERT (strcmp (response.response, "abc") == 0);
        PLY_TEST_ASSERT (state->normal_count == 1);

        ply_trigger_free (answer);
        plymouthd_interaction_free (interaction);
        ply_boot_splash_free (splash);
        ply_close_module (module);
        return true;
}

static bool
test_connection_cancel_clears_only_active_input (void)
{
        plymouthd_interaction_t *interaction;
        ply_trigger_t *first_answer;
        ply_trigger_t *second_answer;
        ply_boot_connection_t *first_connection;
        ply_boot_connection_t *second_connection;
        response_t first_response = { 0 };
        response_t second_response = { 0 };

        first_connection = (ply_boot_connection_t *) (uintptr_t) 1;
        second_connection = (ply_boot_connection_t *) (uintptr_t) 2;
        interaction = plymouthd_interaction_new ();
        first_answer = ply_trigger_new (NULL);
        second_answer = ply_trigger_new (NULL);
        ply_trigger_add_handler (first_answer, record_response, &first_response);
        ply_trigger_add_handler (second_answer, record_response, &second_response);

        plymouthd_interaction_queue_question (interaction,
                                              NULL,
                                              "First?",
                                              first_answer,
                                              first_connection);
        plymouthd_interaction_queue_question (interaction,
                                              NULL,
                                              "Second?",
                                              second_answer,
                                              second_connection);
        plymouthd_interaction_handle_input (interaction, NULL, "stale", 5);
        plymouthd_interaction_cancel_connection (interaction,
                                                 NULL,
                                                 first_connection);

        PLY_TEST_ASSERT (first_response.count == 1);
        PLY_TEST_ASSERT (first_response.received_null);
        PLY_TEST_ASSERT (second_response.count == 0);

        plymouthd_interaction_handle_input (interaction, NULL, "fresh", 5);
        plymouthd_interaction_handle_enter (interaction, NULL, "ignored");
        PLY_TEST_ASSERT (second_response.count == 1);
        PLY_TEST_ASSERT (!second_response.received_null);
        PLY_TEST_ASSERT (strcmp (second_response.response, "fresh") == 0);

        ply_trigger_free (first_answer);
        ply_trigger_free (second_answer);
        plymouthd_interaction_free (interaction);
        return true;
}

static bool
test_keystroke_watches_match_ignore_and_disconnect (void)
{
        plymouthd_interaction_t *interaction;
        ply_boot_connection_t *connection;
        ply_trigger_t *matched_trigger;
        ply_trigger_t *ignored_trigger;
        ply_trigger_t *disconnected_trigger;
        response_t matched = { 0 };
        response_t ignored = { 0 };
        response_t disconnected = { 0 };

        connection = (ply_boot_connection_t *) (uintptr_t) 1;
        interaction = plymouthd_interaction_new ();
        matched_trigger = ply_trigger_new (NULL);
        ignored_trigger = ply_trigger_new (NULL);
        disconnected_trigger = ply_trigger_new (NULL);
        ply_trigger_add_handler (matched_trigger, record_response, &matched);
        ply_trigger_add_handler (ignored_trigger, record_response, &ignored);
        ply_trigger_add_handler (disconnected_trigger,
                                 record_response,
                                 &disconnected);

        plymouthd_interaction_watch_keystroke (interaction,
                                               "yn",
                                               matched_trigger,
                                               NULL);
        plymouthd_interaction_handle_input (interaction, NULL, "x", 1);
        PLY_TEST_ASSERT (matched.count == 0);
        plymouthd_interaction_handle_input (interaction, NULL, "y", 1);
        PLY_TEST_ASSERT (matched.count == 1);
        PLY_TEST_ASSERT (strcmp (matched.response, "y") == 0);

        plymouthd_interaction_watch_keystroke (interaction,
                                               "abc",
                                               ignored_trigger,
                                               NULL);
        plymouthd_interaction_ignore_keystroke (interaction, "abc");
        PLY_TEST_ASSERT (ignored.count == 1);
        PLY_TEST_ASSERT (ignored.received_null);

        plymouthd_interaction_watch_keystroke (interaction,
                                               NULL,
                                               disconnected_trigger,
                                               connection);
        plymouthd_interaction_cancel_connection (interaction,
                                                 NULL,
                                                 connection);
        PLY_TEST_ASSERT (disconnected.count == 1);
        PLY_TEST_ASSERT (disconnected.received_null);

        ply_trigger_free (matched_trigger);
        ply_trigger_free (ignored_trigger);
        ply_trigger_free (disconnected_trigger);
        plymouthd_interaction_free (interaction);
        return true;
}

static const ply_test_case_t test_cases[] =
{
        PLY_TEST_CASE (test_password_input_updates_splash_and_completes),
        PLY_TEST_CASE (test_connection_cancel_clears_only_active_input),
        PLY_TEST_CASE (test_keystroke_watches_match_ignore_and_disconnect),
};

PLY_TEST_MAIN (test_cases)
