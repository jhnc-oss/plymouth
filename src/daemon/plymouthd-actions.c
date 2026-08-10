/* plymouthd-actions.c - daemon command actions
 *
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#include "plymouthd-actions-private.h"

#include <locale.h>
#include <string.h>
#include <unistd.h>

#include "ply-boot-splash.h"
#include "ply-device-manager.h"
#include "ply-logger.h"
#include "ply-terminal.h"
#include "ply-trigger.h"
#include "plymouthd-control-private.h"
#include "plymouthd-display-private.h"
#include "plymouthd-interaction-private.h"
#include "plymouthd-logging-private.h"
#include "plymouthd-messages-private.h"
#include "plymouthd-policy-private.h"
#include "plymouthd-process-private.h"
#include "plymouthd-progress-private.h"
#include "plymouthd-settings-private.h"
#include "plymouthd-state-private.h"

typedef plymouthd_t state_t;

void
plymouthd_handle_update (state_t    *state,
                         const char *status)
{
        ply_trace ("updating status to '%s'", status);
        plymouthd_progress_status_update (state->progress, status);
        if (state->boot_splash != NULL)
                ply_boot_splash_update_status (state->boot_splash,
                                               status);
}

void
plymouthd_handle_change_mode (state_t    *state,
                              const char *mode)
{
        ply_boot_splash_mode_t new_mode;

        ply_trace ("updating mode to '%s'", mode);
        new_mode = plymouthd_mode_from_string (mode);
        if (new_mode == PLY_BOOT_SPLASH_MODE_INVALID)
                return;

        state->mode = new_mode;
        plymouthd_logging_set_mode (state->logging, new_mode);
        plymouthd_progress_set_mode (state->progress, new_mode);

        plymouthd_logging_prepare (state->logging, state->session);

        if (state->boot_splash == NULL) {
                ply_trace ("no splash set");
                return;
        }

        if (!ply_boot_splash_show (state->boot_splash, state->mode)) {
                ply_trace ("failed to update splash");
                return;
        }
}

void
plymouthd_handle_system_update (state_t *state,
                                int      progress)
{
        if (state->boot_splash == NULL) {
                ply_trace ("no splash set");
                return;
        }

        ply_trace ("setting system update to '%i'", progress);
        if (!ply_boot_splash_system_update (state->boot_splash, progress)) {
                ply_trace ("failed to update splash");
                return;
        }
}

void
plymouthd_handle_ask_for_password (state_t               *state,
                                   const char            *prompt,
                                   ply_trigger_t         *answer,
                                   ply_boot_connection_t *connection)
{
        if (state->boot_splash == NULL) {
                /* Waiting to be shown, boot splash will
                 * arrive shortly so just sit tight
                 */
                if (state->is_shown) {
                        bool has_displays;

                        plymouthd_cancel_pending_show (state);

                        has_displays = ply_device_manager_has_displays (state->device_manager);

                        if (has_displays) {
                                ply_trace ("displays available now, showing splash immediately");
                                plymouthd_show_splash (state);
                        } else {
                                ply_trace ("splash still coming up, waiting a bit");
                        }
                } else {
                        /* No splash, client will have to get password */
                        ply_trace ("no splash loaded, replying immediately with no password");
                        ply_trigger_pull (answer, NULL);
                        return;
                }
        }

        plymouthd_interaction_queue_password (state->interaction,
                                              state->boot_splash,
                                              prompt,
                                              answer,
                                              connection);
}

void
plymouthd_handle_ask_question (state_t               *state,
                               const char            *prompt,
                               ply_trigger_t         *answer,
                               ply_boot_connection_t *connection)
{
        plymouthd_interaction_queue_question (state->interaction,
                                              state->boot_splash,
                                              prompt,
                                              answer,
                                              connection);
}

void
plymouthd_handle_display_message (state_t    *state,
                                  const char *message)
{
        plymouthd_messages_display (state->messages,
                                    state->boot_splash,
                                    message);
}

void
plymouthd_handle_hide_message (state_t    *state,
                               const char *message)
{
        plymouthd_messages_hide (state->messages,
                                 state->boot_splash,
                                 message);
}

void
plymouthd_handle_watch_for_keystroke (state_t               *state,
                                      const char            *keys,
                                      ply_trigger_t         *trigger,
                                      ply_boot_connection_t *connection)
{
        plymouthd_interaction_watch_keystroke (state->interaction,
                                               keys,
                                               trigger,
                                               connection);
}

void
plymouthd_handle_connection_hangup (state_t               *state,
                                    ply_boot_connection_t *connection)
{
        plymouthd_interaction_cancel_connection (state->interaction,
                                                 state->boot_splash,
                                                 connection);
}

void
plymouthd_handle_ignore_keystroke (state_t    *state,
                                   const char *keys)
{
        plymouthd_interaction_ignore_keystroke (state->interaction, keys);
}

void
plymouthd_handle_progress_pause (state_t *state)
{
        ply_trace ("pausing progress");
        plymouthd_progress_pause (state->progress);
}

void
plymouthd_handle_progress_unpause (state_t *state)
{
        ply_trace ("unpausing progress");
        plymouthd_progress_unpause (state->progress);
}

void
plymouthd_handle_newroot (state_t    *state,
                          const char *root_dir)
{
        if (plymouthd_shell_is_init ()) {
                ply_trace ("new root mounted at \"%s\", exiting since init= a shell", root_dir);
                plymouthd_handle_quit (state, false, ply_trigger_new (NULL));
                return;
        }

        ply_trace ("new root mounted at \"%s\", switching to it", root_dir);

        if (!strcmp (root_dir, "/run/initramfs") &&
            plymouthd_process_has_diagnostics (state->process)) {
                ply_trace ("switching back to initramfs, dumping debug-buffer now");
                plymouthd_process_dump_diagnostics (state->process);
        }

        chdir (root_dir);
        chroot (".");
        chdir ("/");
        /* Update local now that we have /usr/share/locale available */
        setlocale (LC_ALL, "");
        plymouthd_progress_load_cache (state->progress);
        if (state->boot_splash != NULL)
                ply_boot_splash_root_mounted (state->boot_splash);
}

void
plymouthd_handle_error (state_t *state)
{
        ply_trace ("encountered error during boot up");

        plymouthd_logging_record_error (state->logging);
}

void
plymouthd_handle_reload (state_t *state)
{
        ply_trace ("reloading");
        if (state->boot_splash != NULL) {
                ply_boot_splash_hide (state->boot_splash);
                ply_boot_splash_free (state->boot_splash);
                state->boot_splash = NULL;
        }

        plymouthd_settings_reload_theme_paths (&state->settings);

        if (state->is_inactive) {
                ply_trace ("reload while inactive");
                return;
        }

        if (!state->is_shown) {
                ply_trace ("reload while not shown");
                return;
        }

        if (state->showing_details) {
                plymouthd_show_detailed_splash (state);
        } else {
                plymouthd_show_default_splash (state);
        }
}


bool
plymouthd_handle_has_active_vt (state_t *state)
{
        if (state->local_console_terminal != NULL)
                return ply_terminal_is_active (state->local_console_terminal);
        else
                return false;
}
