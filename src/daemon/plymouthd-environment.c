/* plymouthd-environment.c - daemon operating environment
 *
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#include "plymouthd-environment-private.h"

#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#include "ply-logger.h"
#include "ply-utils.h"
#include "plymouthd-interaction-private.h"
#include "plymouthd-messages-private.h"
#include "plymouthd-process-private.h"
#include "plymouthd-splash-private.h"
#include "plymouthd-state-private.h"

static bool
redirect_standard_io_to_dev_null (void)
{
        int fd;

        fd = open ("/dev/null", O_RDWR | O_APPEND);

        if (fd < 0)
                return false;

        dup2 (fd, STDIN_FILENO);
        dup2 (fd, STDOUT_FILENO);
        dup2 (fd, STDERR_FILENO);
        close (fd);

        return true;
}

static const char *
find_fallback_tty (plymouthd_t *daemon)
{
        static const char *tty_list[] =
        {
                "/dev/ttyS0",
                "/dev/hvc0",
                "/dev/xvc0",
                "/dev/ttySG0",
                NULL
        };
        int i;

        for (i = 0; tty_list[i] != NULL; i++) {
                if (ply_character_device_exists (tty_list[i]))
                        return tty_list[i];
        }

        return daemon->default_tty;
}

bool
plymouthd_initialize_environment (plymouthd_t *daemon,
                                  const char  *debug_path,
                                  bool         capture_debug,
                                  bool         should_force_default_splash,
                                  char        *pid_file)
{
        ply_trace ("initializing minimal work environment");

        if (daemon->default_tty == NULL &&
            getenv ("DISPLAY") != NULL &&
            access (PLYMOUTH_PLUGIN_PATH "renderers/x11.so", F_OK) == 0) {
                daemon->default_tty = "/dev/tty";
        }

        if (daemon->default_tty == NULL) {
                if (daemon->mode == PLY_BOOT_SPLASH_MODE_SHUTDOWN ||
                    daemon->mode == PLY_BOOT_SPLASH_MODE_REBOOT)
                        daemon->default_tty = SHUTDOWN_TTY;
                else
                        daemon->default_tty = BOOT_TTY;

                ply_trace ("checking if '%s' exists", daemon->default_tty);
                if (!ply_character_device_exists (daemon->default_tty)) {
                        if (!should_force_default_splash) {
                                ply_trace ("nope, forcing details mode");
                                plymouthd_splash_force_details (daemon->splash);
                        }

                        daemon->default_tty = find_fallback_tty (daemon);
                        ply_trace ("going to go with '%s'", daemon->default_tty);
                }
        }

        daemon->process = plymouthd_process_new (daemon->mode,
                                                 daemon->default_tty,
                                                 debug_path,
                                                 capture_debug,
                                                 pid_file);
        plymouthd_process_install_crash_handlers (daemon->process);

        ply_trace ("source built on %s", __DATE__);

        daemon->interaction = plymouthd_interaction_new ();
        daemon->messages = plymouthd_messages_new ();

        if (!ply_is_tracing_to_terminal ())
                redirect_standard_io_to_dev_null ();

        ply_trace ("Making sure " PLYMOUTH_RUNTIME_DIR " exists");
        if (!ply_create_directory (PLYMOUTH_RUNTIME_DIR))
                ply_trace ("could not create " PLYMOUTH_RUNTIME_DIR ": %m");

        ply_trace ("initialized minimal work environment");
        return true;
}
