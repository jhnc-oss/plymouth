/*
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#include "ply-test.h"

#include <string.h>

#include "plugins/fake-splash.h"
#include "ply-boot-splash.h"
#include "ply-utils.h"
#include "plymouthd-messages-private.h"

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
test_queued_messages_replay_in_order (void)
{
        const test_splash_plugin_state_t *state;
        plymouthd_messages_t *messages;
        ply_module_handle_t *module;
        ply_boot_splash_t *splash;

        module = ply_open_module (TEST_SPLASH_PLUGIN_PATH);
        PLY_TEST_ASSERT (module != NULL);
        splash = load_splash ();
        PLY_TEST_ASSERT (splash != NULL);
        messages = plymouthd_messages_new ();

        plymouthd_messages_display (messages, NULL, "first");
        plymouthd_messages_display (messages, NULL, "second");
        plymouthd_messages_replay (messages, splash);

        state = get_plugin_state (module);
        PLY_TEST_ASSERT (state != NULL);
        PLY_TEST_ASSERT (state->display_message_count == 2);
        PLY_TEST_ASSERT (strcmp (state->displayed_message, "second") == 0);

        plymouthd_messages_free (messages);
        ply_boot_splash_free (splash);
        ply_close_module (module);
        return true;
}

static bool
test_hide_removes_every_matching_message (void)
{
        const test_splash_plugin_state_t *state;
        plymouthd_messages_t *messages;
        ply_module_handle_t *module;
        ply_boot_splash_t *splash;

        module = ply_open_module (TEST_SPLASH_PLUGIN_PATH);
        PLY_TEST_ASSERT (module != NULL);
        splash = load_splash ();
        PLY_TEST_ASSERT (splash != NULL);
        messages = plymouthd_messages_new ();

        plymouthd_messages_display (messages, NULL, "duplicate");
        plymouthd_messages_display (messages, NULL, "retained");
        plymouthd_messages_display (messages, NULL, "duplicate");
        plymouthd_messages_hide (messages, splash, "duplicate");
        plymouthd_messages_replay (messages, splash);

        state = get_plugin_state (module);
        PLY_TEST_ASSERT (state != NULL);
        PLY_TEST_ASSERT (state->hide_message_count == 2);
        PLY_TEST_ASSERT (strcmp (state->hidden_message, "duplicate") == 0);
        PLY_TEST_ASSERT (state->display_message_count == 1);
        PLY_TEST_ASSERT (strcmp (state->displayed_message, "retained") == 0);

        plymouthd_messages_free (messages);
        ply_boot_splash_free (splash);
        ply_close_module (module);
        return true;
}

static bool
test_live_message_remains_available_for_replay (void)
{
        const test_splash_plugin_state_t *state;
        plymouthd_messages_t *messages;
        ply_module_handle_t *module;
        ply_boot_splash_t *splash;

        module = ply_open_module (TEST_SPLASH_PLUGIN_PATH);
        PLY_TEST_ASSERT (module != NULL);
        splash = load_splash ();
        PLY_TEST_ASSERT (splash != NULL);
        messages = plymouthd_messages_new ();

        plymouthd_messages_display (messages, splash, "status");
        plymouthd_messages_replay (messages, splash);

        state = get_plugin_state (module);
        PLY_TEST_ASSERT (state != NULL);
        PLY_TEST_ASSERT (state->display_message_count == 2);
        PLY_TEST_ASSERT (strcmp (state->displayed_message, "status") == 0);

        plymouthd_messages_free (messages);
        ply_boot_splash_free (splash);
        ply_close_module (module);
        return true;
}

static const ply_test_case_t test_cases[] =
{
        PLY_TEST_CASE (test_queued_messages_replay_in_order),
        PLY_TEST_CASE (test_hide_removes_every_matching_message),
        PLY_TEST_CASE (test_live_message_remains_available_for_replay),
};

PLY_TEST_MAIN (test_cases)
