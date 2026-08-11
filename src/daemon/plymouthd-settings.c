/* plymouthd-settings.c - internal daemon settings
 *
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#include "plymouthd-settings-private.h"

#include <math.h>
#include <stdlib.h>

#include "ply-key-file.h"
#include "ply-logger.h"
#include "ply-utils.h"
#include "plymouthd-policy-private.h"
#include "plymouthd-theme-resolver-private.h"

static void initialize_settings (plymouthd_settings_t *settings);
static void clear_settings (plymouthd_settings_t *settings);

plymouthd_settings_t *
plymouthd_settings_new (void)
{
        plymouthd_settings_t *settings;

        settings = calloc (1, sizeof(plymouthd_settings_t));
        initialize_settings (settings);

        return settings;
}

static void
initialize_settings (plymouthd_settings_t *settings)
{
        *settings = (plymouthd_settings_t) {
                .splash_delay = NAN,
                .device_timeout = NAN,
                .device_scale = -1,
                .extra_esc_key = XKB_KEY_NoSymbol,
                .use_simpledrm = -1,
        };
}

static void
clear_settings (plymouthd_settings_t *settings)
{
        free (settings->override_splash_path);
        free (settings->system_default_splash_path);
        free (settings->distribution_default_splash_path);

        settings->override_splash_path = NULL;
        settings->system_default_splash_path = NULL;
        settings->distribution_default_splash_path = NULL;
}

void
plymouthd_settings_free (plymouthd_settings_t *settings)
{
        if (settings == NULL)
                return;

        clear_settings (settings);
        free (settings);
}

bool
plymouthd_settings_apply_config_file (plymouthd_settings_t *settings,
                                      const char           *path,
                                      char                **theme_path)
{
        ply_key_file_t *key_file = NULL;
        bool settings_loaded = false;
        char *theme_name = NULL;

        ply_trace ("Trying to load %s", path);
        key_file = ply_key_file_new (path);

        if (!ply_key_file_load (key_file))
                goto out;

        theme_name = ply_key_file_get_value (key_file, "Daemon", "Theme");

        if (theme_name != NULL) {
                char *configured_theme_dir;

                configured_theme_dir = ply_key_file_get_value (key_file,
                                                               "Daemon",
                                                               "ThemeDir");
                plymouthd_resolve_theme_path (theme_name,
                                              configured_theme_dir,
                                              theme_path);
                free (configured_theme_dir);
        }

        if (isnan (settings->splash_delay)) {
                settings->splash_delay = ply_key_file_get_double (key_file,
                                                                  "Daemon",
                                                                  "ShowDelay",
                                                                  NAN);
                ply_trace ("Splash delay is set to %lf", settings->splash_delay);
        }

        if (isnan (settings->device_timeout)) {
                settings->device_timeout = ply_key_file_get_double (key_file,
                                                                    "Daemon",
                                                                    "DeviceTimeout",
                                                                    NAN);
                ply_trace ("Device timeout is set to %lf", settings->device_timeout);
        }

        if (settings->device_scale == -1) {
                settings->device_scale = ply_key_file_get_ulong (key_file,
                                                                 "Daemon",
                                                                 "DeviceScale",
                                                                 -1);
        }

        if (settings->extra_esc_key == XKB_KEY_NoSymbol) {
                settings->extra_esc_key = ply_key_file_get_ulong (key_file,
                                                                  "Daemon",
                                                                  "XkbExtraEscButton",
                                                                  XKB_KEY_NoSymbol);
        }

        /*
         * Check the special UseSimpledrmNoLuks config file keyword this enables
         * simpledrm use except when using LUKS. Showing the LUKS unlock screen
         * using simpledrm has 2 problems:
         * 1. If the GPU drivers are built into the initrd then typically the
         *    unlock screen will briefly show and then the screen goes black
         *    while the native GPU driver loads leading to a jarring experience.
         * 2. The i915 driver uses the firmware framebuffer as fallback when
         *    userspace has not installed a fb to scan out from. This happens
         *    e.g. on logout between the user-session and the display-manager.
         *    Drawing the unlock screen on the simpledrm fb results in it briefly
         *    showing when logging out, which looks quite ugly. Also see:
         *    https://bugzilla.redhat.com/show_bug.cgi?id=2359283
         */
        if (settings->use_simpledrm == -1) {
                int direct_setting;
                int setting_without_encryption;
                bool uses_disk_encryption;

                direct_setting = ply_key_file_get_ulong (key_file,
                                                         "Daemon",
                                                         "UseSimpledrm",
                                                         -1);
                setting_without_encryption = ply_key_file_get_ulong (key_file,
                                                                     "Daemon",
                                                                     "UseSimpledrmNoLuks",
                                                                     -1);
                uses_disk_encryption =
                        ply_kernel_command_line_get_string_after_prefix ("rd.luks.uuid=") != NULL;

                settings->use_simpledrm =
                        plymouthd_select_simpledrm_config (settings->use_simpledrm,
                                                           direct_setting,
                                                           setting_without_encryption,
                                                           uses_disk_encryption);

                if (direct_setting == -1 &&
                    setting_without_encryption != -1 &&
                    uses_disk_encryption)
                        ply_trace ("Ignoring UseSimpledrmNoLuks because of LUKS use");
        }

        settings_loaded = true;
out:
        free (theme_name);
        ply_key_file_free (key_file);

        return settings_loaded;
}

void
plymouthd_settings_apply_kernel_command_line (plymouthd_settings_t *settings)
{
        char *theme_name;

        if (settings->override_splash_path != NULL)
                return;

        theme_name = ply_kernel_command_line_get_key_value ("plymouth.splash=");

        if (theme_name != NULL) {
                ply_trace ("Splash is configured to be '%s'", theme_name);
                plymouthd_resolve_theme_path (theme_name,
                                              NULL,
                                              &settings->override_splash_path);
                free (theme_name);
        }

        if (isnan (settings->splash_delay)) {
                const char *delay_string;

                delay_string = ply_kernel_command_line_get_string_after_prefix ("plymouth.splash-delay=");

                if (delay_string != NULL)
                        settings->splash_delay = ply_strtod (delay_string);
        }

        if (settings->device_scale == -1)
                settings->device_scale =
                        ply_kernel_command_line_get_ulong ("plymouth.force-scale=", -1);

        if (settings->use_simpledrm == -1) {
                int numeric_setting;

                numeric_setting =
                        ply_kernel_command_line_get_ulong ("plymouth.use-simpledrm=", -1);
                settings->use_simpledrm =
                        plymouthd_select_simpledrm_command_line (
                                settings->use_simpledrm,
                                numeric_setting,
                                ply_kernel_command_line_has_argument ("plymouth.use-simpledrm"),
                                ply_kernel_command_line_has_argument ("nomodeset"));
        }
}

static void
load_system_defaults (plymouthd_settings_t *settings)
{
        if (settings->system_default_splash_path != NULL)
                return;

        if (!plymouthd_settings_apply_config_file (
                    settings,
                    PLYMOUTH_CONF_DIR "plymouthd.conf",
                    &settings->system_default_splash_path)) {
                ply_trace ("failed to load " PLYMOUTH_CONF_DIR "plymouthd.conf");
                return;
        }

        if (settings->system_default_splash_path != NULL)
                ply_trace ("System configured theme file is '%s'",
                           settings->system_default_splash_path);
}

static void
load_distribution_defaults (plymouthd_settings_t *settings)
{
        if (settings->distribution_default_splash_path != NULL)
                return;

        if (!plymouthd_settings_apply_config_file (
                    settings,
                    PLYMOUTH_RUNTIME_DIR "/plymouthd.defaults",
                    &settings->distribution_default_splash_path)) {
                ply_trace ("failed to load " PLYMOUTH_RUNTIME_DIR
                           "/plymouthd.defaults, trying " PLYMOUTH_POLICY_DIR);
                if (!plymouthd_settings_apply_config_file (
                            settings,
                            PLYMOUTH_POLICY_DIR "plymouthd.defaults",
                            &settings->distribution_default_splash_path)) {
                        ply_trace ("failed to load " PLYMOUTH_POLICY_DIR
                                   "plymouthd.defaults");
                        return;
                }
        }

        if (settings->distribution_default_splash_path != NULL)
                ply_trace ("Distribution default theme file is '%s'",
                           settings->distribution_default_splash_path);
}

void
plymouthd_settings_load (plymouthd_settings_t *settings)
{
        plymouthd_settings_apply_kernel_command_line (settings);
        load_system_defaults (settings);
        load_distribution_defaults (settings);
}

void
plymouthd_settings_reload_theme_paths (plymouthd_settings_t *settings)
{
        free (settings->override_splash_path);
        settings->override_splash_path = NULL;
        free (settings->system_default_splash_path);
        settings->system_default_splash_path = NULL;
        free (settings->distribution_default_splash_path);
        settings->distribution_default_splash_path = NULL;

        plymouthd_settings_load (settings);
}
