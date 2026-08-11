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
#include "ply-logger.h"
#include "ply-trigger.h"
#include "plymouthd-control-private.h"
#include "plymouthd-devices-private.h"
#include "plymouthd-display-private.h"
#include "plymouthd-interaction-private.h"
#include "plymouthd-logging-private.h"
#include "plymouthd-messages-private.h"
#include "plymouthd-policy-private.h"
#include "plymouthd-process-private.h"
#include "plymouthd-progress-private.h"
#include "plymouthd-session-private.h"
#include "plymouthd-settings-private.h"
#include "plymouthd-splash-private.h"
#include "plymouthd-state-private.h"

void
plymouthd_handle_update (plymouthd_t *daemon,
                         const char  *status)
{
        ply_trace ("updating status to '%s'", status);
        plymouthd_progress_status_update (daemon->progress, status);
        if (plymouthd_splash_get (daemon->splash) != NULL)
                ply_boot_splash_update_status (plymouthd_splash_get (daemon->splash),
                                               status);
}

void
plymouthd_handle_change_mode (plymouthd_t *daemon,
                              const char  *mode)
{
        ply_boot_splash_mode_t new_mode;

        ply_trace ("updating mode to '%s'", mode);
        new_mode = plymouthd_mode_from_string (mode);
        if (new_mode == PLY_BOOT_SPLASH_MODE_INVALID)
                return;

        daemon->mode = new_mode;
        plymouthd_logging_set_mode (daemon->logging, new_mode);
        plymouthd_progress_set_mode (daemon->progress, new_mode);

        plymouthd_logging_prepare (daemon->logging, daemon->session);

        if (plymouthd_splash_get (daemon->splash) == NULL) {
                ply_trace ("no splash set");
                return;
        }

        if (!ply_boot_splash_show (plymouthd_splash_get (daemon->splash),
                                   daemon->mode)) {
                ply_trace ("failed to update splash");
                return;
        }
}

void
plymouthd_handle_system_update (plymouthd_t *daemon,
                                int          progress)
{
        if (plymouthd_splash_get (daemon->splash) == NULL) {
                ply_trace ("no splash set");
                return;
        }

        ply_trace ("setting system update to '%i'", progress);
        if (!ply_boot_splash_system_update (
                    plymouthd_splash_get (daemon->splash),
                    progress)) {
                ply_trace ("failed to update splash");
                return;
        }
}

void
plymouthd_handle_ask_for_password (plymouthd_t           *daemon,
                                   const char            *prompt,
                                   ply_trigger_t         *answer,
                                   ply_boot_connection_t *connection)
{
        if (plymouthd_splash_get (daemon->splash) == NULL) {
                /* Waiting to be shown, boot splash will
                 * arrive shortly so just sit tight
                 */
                if (plymouthd_splash_is_shown (daemon->splash)) {
                        bool has_displays;

                        plymouthd_cancel_pending_show (daemon);

                        has_displays = plymouthd_devices_has_displays (daemon->devices);

                        if (has_displays) {
                                ply_trace ("displays available now, showing splash immediately");
                                plymouthd_show_splash (daemon);
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

        plymouthd_interaction_queue_password (daemon->interaction,
                                              plymouthd_splash_get (daemon->splash),
                                              prompt,
                                              answer,
                                              connection);
}

void
plymouthd_handle_ask_question (plymouthd_t           *daemon,
                               const char            *prompt,
                               ply_trigger_t         *answer,
                               ply_boot_connection_t *connection)
{
        plymouthd_interaction_queue_question (daemon->interaction,
                                              plymouthd_splash_get (daemon->splash),
                                              prompt,
                                              answer,
                                              connection);
}

void
plymouthd_handle_display_message (plymouthd_t *daemon,
                                  const char  *message)
{
        plymouthd_messages_display (daemon->messages,
                                    plymouthd_splash_get (daemon->splash),
                                    message);
}

void
plymouthd_handle_hide_message (plymouthd_t *daemon,
                               const char  *message)
{
        plymouthd_messages_hide (daemon->messages,
                                 plymouthd_splash_get (daemon->splash),
                                 message);
}

void
plymouthd_handle_watch_for_keystroke (plymouthd_t           *daemon,
                                      const char            *keys,
                                      ply_trigger_t         *trigger,
                                      ply_boot_connection_t *connection)
{
        plymouthd_interaction_watch_keystroke (daemon->interaction,
                                               keys,
                                               trigger,
                                               connection);
}

void
plymouthd_handle_connection_hangup (plymouthd_t           *daemon,
                                    ply_boot_connection_t *connection)
{
        plymouthd_interaction_cancel_connection (daemon->interaction,
                                                 plymouthd_splash_get (daemon->splash),
                                                 connection);
}

void
plymouthd_handle_ignore_keystroke (plymouthd_t *daemon,
                                   const char  *keys)
{
        plymouthd_interaction_ignore_keystroke (daemon->interaction, keys);
}

void
plymouthd_handle_progress_pause (plymouthd_t *daemon)
{
        ply_trace ("pausing progress");
        plymouthd_progress_pause (daemon->progress);
}

void
plymouthd_handle_progress_unpause (plymouthd_t *daemon)
{
        ply_trace ("unpausing progress");
        plymouthd_progress_unpause (daemon->progress);
}

void
plymouthd_handle_newroot (plymouthd_t *daemon,
                          const char  *root_dir)
{
        if (plymouthd_shell_is_init ()) {
                ply_trace ("new root mounted at \"%s\", exiting since init= a shell", root_dir);
                plymouthd_handle_quit (daemon, false, ply_trigger_new (NULL));
                return;
        }

        ply_trace ("new root mounted at \"%s\", switching to it", root_dir);

        if (!strcmp (root_dir, "/run/initramfs") &&
            plymouthd_process_has_diagnostics (daemon->process)) {
                ply_trace ("switching back to initramfs, dumping debug-buffer now");
                plymouthd_process_dump_diagnostics (daemon->process);
        }

        chdir (root_dir);
        chroot (".");
        chdir ("/");
        /* Update local now that we have /usr/share/locale available */
        setlocale (LC_ALL, "");
        plymouthd_progress_load_cache (daemon->progress);
        if (plymouthd_splash_get (daemon->splash) != NULL)
                ply_boot_splash_root_mounted (
                        plymouthd_splash_get (daemon->splash));
}

void
plymouthd_handle_system_initialized (plymouthd_t *daemon)
{
        ply_trace ("system now initialized, opening log");

        plymouthd_session_request_details (daemon->session);

        plymouthd_logging_system_initialized (daemon->logging, daemon->session);
}

void
plymouthd_handle_error (plymouthd_t *daemon)
{
        ply_trace ("encountered error during boot up");

        plymouthd_logging_record_error (daemon->logging);
}

void
plymouthd_handle_reload (plymouthd_t *daemon)
{
        ply_trace ("reloading");
        if (plymouthd_splash_get (daemon->splash) != NULL) {
                ply_boot_splash_hide (plymouthd_splash_get (daemon->splash));
                plymouthd_splash_clear (daemon->splash);
        }

        plymouthd_settings_reload_theme_paths (daemon->settings);

        if (daemon->is_inactive) {
                ply_trace ("reload while inactive");
                return;
        }

        if (!plymouthd_splash_is_shown (daemon->splash)) {
                ply_trace ("reload while not shown");
                return;
        }

        if (plymouthd_splash_is_showing_details (daemon->splash)) {
                plymouthd_show_detailed_splash (daemon);
        } else {
                plymouthd_show_default_splash (daemon);
        }
}


bool
plymouthd_handle_has_active_vt (plymouthd_t *daemon)
{
        return plymouthd_devices_has_active_vt (daemon->devices);
}
