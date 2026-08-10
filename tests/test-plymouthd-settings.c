/*
 * Copyright (C) 2026 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */

#include "ply-test.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "ply-utils.h"
#include "plymouthd-settings-private.h"
#include "plymouthd-theme-resolver-private.h"

static bool
write_file (const char *path,
            const char *contents)
{
        FILE *file;

        file = fopen (path, "w");
        if (file == NULL)
                return false;

        if (fputs (contents, file) == EOF) {
                fclose (file);
                return false;
        }

        return fclose (file) == 0;
}

static bool
create_theme (const char *base_directory,
              const char *theme_name,
              char      **theme_directory,
              char      **theme_path)
{
        if (asprintf (theme_directory,
                      "%s/%s",
                      base_directory,
                      theme_name) < 0)
                return false;

        if (mkdir (*theme_directory, 0700) < 0)
                return false;

        if (asprintf (theme_path,
                      "%s/%s.plymouth",
                      *theme_directory,
                      theme_name) < 0)
                return false;

        return write_file (*theme_path,
                           "[Plymouth Theme]\n"
                           "ModuleName=details\n");
}

static bool
test_settings_start_unset (void)
{
        plymouthd_settings_t settings;

        plymouthd_settings_init (&settings);

        PLY_TEST_ASSERT (isnan (settings.splash_delay));
        PLY_TEST_ASSERT (isnan (settings.device_timeout));
        PLY_TEST_ASSERT (settings.device_scale == -1);
        PLY_TEST_ASSERT (settings.extra_esc_key == XKB_KEY_NoSymbol);
        PLY_TEST_ASSERT (settings.use_simpledrm == -1);
        PLY_TEST_ASSERT (settings.override_splash_path == NULL);
        PLY_TEST_ASSERT (settings.system_default_splash_path == NULL);
        PLY_TEST_ASSERT (settings.distribution_default_splash_path == NULL);

        plymouthd_settings_free (&settings);
        return true;
}

static bool
test_theme_directories_are_searched_in_order (void)
{
        char temporary_directory[] = "/tmp/plymouth-theme-test-XXXXXX";
        const char *directories[3];
        char *first_directory = NULL;
        char *second_directory = NULL;
        char *third_directory = NULL;
        char *first_theme_directory = NULL;
        char *second_theme_directory = NULL;
        char *first_theme_path = NULL;
        char *second_theme_path = NULL;
        char *resolved_path = NULL;

        PLY_TEST_ASSERT (mkdtemp (temporary_directory) != NULL);
        PLY_TEST_ASSERT (asprintf (&first_directory,
                                   "%s/runtime",
                                   temporary_directory) >= 0);
        PLY_TEST_ASSERT (asprintf (&second_directory,
                                   "%s/configured",
                                   temporary_directory) >= 0);
        PLY_TEST_ASSERT (asprintf (&third_directory,
                                   "%s/system",
                                   temporary_directory) >= 0);
        PLY_TEST_ASSERT (mkdir (first_directory, 0700) == 0);
        PLY_TEST_ASSERT (mkdir (second_directory, 0700) == 0);
        PLY_TEST_ASSERT (mkdir (third_directory, 0700) == 0);
        PLY_TEST_ASSERT (create_theme (second_directory,
                                       "spinner",
                                       &second_theme_directory,
                                       &second_theme_path));

        directories[0] = first_directory;
        directories[1] = second_directory;
        directories[2] = third_directory;

        PLY_TEST_ASSERT (plymouthd_resolve_theme_path_in_directories (
                                 "spinner", directories, 3, &resolved_path));
        PLY_TEST_ASSERT (strcmp (resolved_path, second_theme_path) == 0);
        free (resolved_path);
        resolved_path = NULL;

        PLY_TEST_ASSERT (create_theme (first_directory,
                                       "spinner",
                                       &first_theme_directory,
                                       &first_theme_path));
        PLY_TEST_ASSERT (plymouthd_resolve_theme_path_in_directories (
                                 "spinner", directories, 3, &resolved_path));
        PLY_TEST_ASSERT (strcmp (resolved_path, first_theme_path) == 0);

        unlink (first_theme_path);
        unlink (second_theme_path);
        rmdir (first_theme_directory);
        rmdir (second_theme_directory);
        rmdir (first_directory);
        rmdir (second_directory);
        rmdir (third_directory);
        rmdir (temporary_directory);
        free (resolved_path);
        free (first_theme_path);
        free (second_theme_path);
        free (first_theme_directory);
        free (second_theme_directory);
        free (first_directory);
        free (second_directory);
        free (third_directory);
        return true;
}

static bool
test_first_config_source_keeps_scalar_precedence (void)
{
        char temporary_directory[] = "/tmp/plymouth-settings-test-XXXXXX";
        plymouthd_settings_t settings;
        char *alpha_directory = NULL;
        char *beta_directory = NULL;
        char *alpha_theme_directory = NULL;
        char *beta_theme_directory = NULL;
        char *alpha_theme_path = NULL;
        char *beta_theme_path = NULL;
        char *first_config_path = NULL;
        char *second_config_path = NULL;
        char *first_config = NULL;
        char *second_config = NULL;
        char *resolved_theme = NULL;

        PLY_TEST_ASSERT (mkdtemp (temporary_directory) != NULL);
        PLY_TEST_ASSERT (asprintf (&alpha_directory,
                                   "%s/alpha-themes",
                                   temporary_directory) >= 0);
        PLY_TEST_ASSERT (asprintf (&beta_directory,
                                   "%s/beta-themes",
                                   temporary_directory) >= 0);
        PLY_TEST_ASSERT (mkdir (alpha_directory, 0700) == 0);
        PLY_TEST_ASSERT (mkdir (beta_directory, 0700) == 0);
        PLY_TEST_ASSERT (create_theme (alpha_directory,
                                       "alpha",
                                       &alpha_theme_directory,
                                       &alpha_theme_path));
        PLY_TEST_ASSERT (create_theme (beta_directory,
                                       "beta",
                                       &beta_theme_directory,
                                       &beta_theme_path));
        PLY_TEST_ASSERT (asprintf (&first_config_path,
                                   "%s/first.conf",
                                   temporary_directory) >= 0);
        PLY_TEST_ASSERT (asprintf (&second_config_path,
                                   "%s/second.conf",
                                   temporary_directory) >= 0);
        PLY_TEST_ASSERT (asprintf (&first_config,
                                   "[Daemon]\n"
                                   "Theme=alpha\n"
                                   "ThemeDir=%s\n"
                                   "ShowDelay=1.5\n"
                                   "DeviceTimeout=8.0\n"
                                   "DeviceScale=2\n"
                                   "XkbExtraEscButton=123\n"
                                   "UseSimpledrm=1\n",
                                   alpha_directory) >= 0);
        PLY_TEST_ASSERT (asprintf (&second_config,
                                   "[Daemon]\n"
                                   "Theme=beta\n"
                                   "ThemeDir=%s\n"
                                   "ShowDelay=9.5\n"
                                   "DeviceTimeout=12.0\n"
                                   "DeviceScale=4\n"
                                   "XkbExtraEscButton=456\n"
                                   "UseSimpledrm=0\n",
                                   beta_directory) >= 0);
        PLY_TEST_ASSERT (write_file (first_config_path, first_config));
        PLY_TEST_ASSERT (write_file (second_config_path, second_config));

        plymouthd_settings_init (&settings);
        PLY_TEST_ASSERT (plymouthd_settings_apply_config_file (
                                 &settings, first_config_path, &resolved_theme));
        PLY_TEST_ASSERT (strcmp (resolved_theme, alpha_theme_path) == 0);
        free (resolved_theme);
        resolved_theme = NULL;

        PLY_TEST_ASSERT (plymouthd_settings_apply_config_file (
                                 &settings, second_config_path, &resolved_theme));
        PLY_TEST_ASSERT (strcmp (resolved_theme, beta_theme_path) == 0);
        PLY_TEST_ASSERT (settings.splash_delay == 1.5);
        PLY_TEST_ASSERT (settings.device_timeout == 8.0);
        PLY_TEST_ASSERT (settings.device_scale == 2);
        PLY_TEST_ASSERT (settings.extra_esc_key == 123);
        PLY_TEST_ASSERT (settings.use_simpledrm == 1);

        plymouthd_settings_free (&settings);
        unlink (first_config_path);
        unlink (second_config_path);
        unlink (alpha_theme_path);
        unlink (beta_theme_path);
        rmdir (alpha_theme_directory);
        rmdir (beta_theme_directory);
        rmdir (alpha_directory);
        rmdir (beta_directory);
        rmdir (temporary_directory);
        free (resolved_theme);
        free (alpha_directory);
        free (beta_directory);
        free (alpha_theme_directory);
        free (beta_theme_directory);
        free (alpha_theme_path);
        free (beta_theme_path);
        free (first_config_path);
        free (second_config_path);
        free (first_config);
        free (second_config);
        return true;
}

static bool
test_kernel_command_line_keeps_precedence (void)
{
        plymouthd_settings_t settings;

        ply_kernel_command_line_override (
                "plymouth.splash-delay=2.5 "
                "plymouth.force-scale=3 "
                "plymouth.use-simpledrm=0");

        plymouthd_settings_init (&settings);
        plymouthd_settings_apply_kernel_command_line (&settings);

        PLY_TEST_ASSERT (settings.splash_delay == 2.5);
        PLY_TEST_ASSERT (isnan (settings.device_timeout));
        PLY_TEST_ASSERT (settings.device_scale == 3);
        PLY_TEST_ASSERT (settings.use_simpledrm == 0);

        plymouthd_settings_free (&settings);
        return true;
}

static const ply_test_case_t test_cases[] =
{
        PLY_TEST_CASE (test_settings_start_unset),
        PLY_TEST_CASE (test_theme_directories_are_searched_in_order),
        PLY_TEST_CASE (test_first_config_source_keeps_scalar_precedence),
        PLY_TEST_CASE (test_kernel_command_line_keeps_precedence),
};

PLY_TEST_MAIN (test_cases)
