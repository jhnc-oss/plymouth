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
#include "ply-buffer.h"
#include "ply-utils.h"
#include "plymouthd-output-private.h"

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
test_output_is_retained_and_forwarded (void)
{
        const test_splash_plugin_state_t *state;
        plymouthd_output_t *output;
        ply_module_handle_t *module;
        ply_boot_splash_t *splash;
        ply_buffer_t *buffer;

        module = ply_open_module (TEST_SPLASH_PLUGIN_PATH);
        PLY_TEST_ASSERT (module != NULL);
        splash = load_splash ();
        PLY_TEST_ASSERT (splash != NULL);
        output = plymouthd_output_new ();

        plymouthd_output_append (output, NULL, "early", 5);
        plymouthd_output_append (output, splash, "live", 4);

        buffer = plymouthd_output_get_buffer (output);
        PLY_TEST_ASSERT (ply_buffer_get_size (buffer) == 9);
        PLY_TEST_ASSERT (memcmp (ply_buffer_get_bytes (buffer),
                                 "earlylive", 9) == 0);

        state = get_plugin_state (module);
        PLY_TEST_ASSERT (state != NULL);
        PLY_TEST_ASSERT (state->output_count == 1);
        PLY_TEST_ASSERT (state->output_size == 4);
        PLY_TEST_ASSERT (memcmp (state->output, "live", 4) == 0);

        plymouthd_output_free (output);
        ply_boot_splash_free (splash);
        ply_close_module (module);
        return true;
}

static const ply_test_case_t test_cases[] =
{
        PLY_TEST_CASE (test_output_is_retained_and_forwarded),
};

PLY_TEST_MAIN (test_cases)
