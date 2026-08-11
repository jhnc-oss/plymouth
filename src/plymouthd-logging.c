/* plymouthd-logging.c - internal boot logging lifecycle
 *
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#include "plymouthd-logging-private.h"
#include "plymouthd-session-private.h"

#include <paths.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ply-logger.h"
#include "ply-utils.h"

struct _plymouthd_logging
{
        char                  *boot_log_file;
        ply_boot_splash_mode_t mode;

        int                    number_of_errors;
        uint32_t               disabled : 1;
        uint32_t               initialized : 1;
};

static void
spool_error (plymouthd_logging_t *logging)
{
        const char *log_file;
        const char *spool_file;

        ply_trace ("spooling error for viewer");

        log_file = plymouthd_logging_get_log_file (logging);
        spool_file = plymouthd_logging_get_spool_file (logging);

        if (log_file != NULL && spool_file != NULL) {
                unlink (spool_file);
                ply_create_file_link (log_file, spool_file);
        }
}

plymouthd_logging_t *
plymouthd_logging_new (ply_boot_splash_mode_t mode,
                       const char            *boot_log_file,
                       bool                   disabled)
{
        plymouthd_logging_t *logging;

        logging = calloc (1, sizeof(plymouthd_logging_t));
        logging->mode = mode;
        logging->disabled = disabled ||
                            ply_kernel_command_line_has_argument ("plymouth.nolog");

        if (boot_log_file != NULL)
                logging->boot_log_file = strdup (boot_log_file);
        else
                logging->boot_log_file =
                        ply_kernel_command_line_get_key_value ("plymouth.boot-log=");

        ply_trace ("logging will%s be enabled", logging->disabled ? " not" : "");

        return logging;
}

void
plymouthd_logging_free (plymouthd_logging_t *logging)
{
        if (logging == NULL)
                return;

        free (logging->boot_log_file);
        free (logging);
}

void
plymouthd_logging_set_mode (plymouthd_logging_t   *logging,
                            ply_boot_splash_mode_t mode)
{
        logging->mode = mode;
}

bool
plymouthd_logging_is_enabled (plymouthd_logging_t *logging)
{
        return !logging->disabled;
}

bool
plymouthd_logging_is_initialized (plymouthd_logging_t *logging)
{
        return logging->initialized;
}

const char *
plymouthd_logging_get_log_file (plymouthd_logging_t *logging)
{
        const char *filename;

        switch (logging->mode) {
        case PLY_BOOT_SPLASH_MODE_BOOT_UP:
                if (logging->disabled)
                        filename = NULL;
                else if (logging->boot_log_file != NULL)
                        filename = logging->boot_log_file;
                else
                        filename = PLYMOUTH_LOG_DIRECTORY "/boot.log";
                break;
        case PLY_BOOT_SPLASH_MODE_SHUTDOWN:
        case PLY_BOOT_SPLASH_MODE_REBOOT:
        case PLY_BOOT_SPLASH_MODE_UPDATES:
        case PLY_BOOT_SPLASH_MODE_SYSTEM_UPGRADE:
        case PLY_BOOT_SPLASH_MODE_FIRMWARE_UPGRADE:
        case PLY_BOOT_SPLASH_MODE_SYSTEM_RESET:
                filename = _PATH_DEVNULL;
                break;
        case PLY_BOOT_SPLASH_MODE_INVALID:
        default:
                ply_error ("Unhandled case in %s line %d\n", __FILE__, __LINE__);
                abort ();
                break;
        }

        ply_trace ("returning log file '%s'", filename);
        return filename;
}

const char *
plymouthd_logging_get_spool_file (plymouthd_logging_t *logging)
{
        const char *filename;

        switch (logging->mode) {
        case PLY_BOOT_SPLASH_MODE_BOOT_UP:
                filename = PLYMOUTH_SPOOL_DIRECTORY "/boot.log";
                break;
        case PLY_BOOT_SPLASH_MODE_SHUTDOWN:
        case PLY_BOOT_SPLASH_MODE_REBOOT:
        case PLY_BOOT_SPLASH_MODE_UPDATES:
        case PLY_BOOT_SPLASH_MODE_SYSTEM_UPGRADE:
        case PLY_BOOT_SPLASH_MODE_FIRMWARE_UPGRADE:
        case PLY_BOOT_SPLASH_MODE_SYSTEM_RESET:
                filename = NULL;
                break;
        case PLY_BOOT_SPLASH_MODE_INVALID:
        default:
                ply_error ("Unhandled case in %s line %d\n", __FILE__, __LINE__);
                abort ();
                break;
        }

        ply_trace ("returning spool file '%s'", filename);
        return filename;
}

void
plymouthd_logging_prepare (plymouthd_logging_t *logging,
                           plymouthd_session_t *session)
{
        const char *log_file;

        if (!logging->initialized) {
                ply_trace ("not preparing logging yet, system not initialized");
                return;
        }

        if (session == NULL || !plymouthd_session_has_terminal (session)) {
                ply_trace ("not preparing logging, no session");
                return;
        }

        plymouthd_session_close_log (session);

        log_file = plymouthd_logging_get_log_file (logging);
        if (log_file != NULL) {
                bool log_opened;

                ply_trace ("opening log '%s'", log_file);
                log_opened = plymouthd_session_open_log (session, log_file);

                if (!log_opened)
                        ply_trace ("failed to open log: %m");

                if (logging->number_of_errors > 0)
                        spool_error (logging);
        }
}

void
plymouthd_logging_system_initialized (plymouthd_logging_t *logging,
                                      plymouthd_session_t *session)
{
        logging->initialized = true;
        plymouthd_logging_prepare (logging, session);
}

void
plymouthd_logging_record_error (plymouthd_logging_t *logging)
{
        if (logging->initialized && logging->number_of_errors == 0)
                spool_error (logging);
        else
                ply_trace ("not spooling because number of errors %d",
                           logging->number_of_errors);

        logging->number_of_errors++;
}
