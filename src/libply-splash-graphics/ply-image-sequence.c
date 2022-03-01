/* ply-image-sequence.c - management for sequences of images
 *
 * Copyright (C) 2022 Hans Christian Schmitz
 *
 * Based on the code of ply-animation.c from project at commit
 * 6f298131a7326608dfc138fd4e9f56a29f35762f
 * Copyright (C) 2009 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Written by: Hans Christian Schmitz <git@hcsch.eu>
 */
#include "config.h"

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <values.h>
#include <unistd.h>
#include <wchar.h>

#include "ply-image-sequence.h"
#include "ply-array.h"
#include "ply-logger.h"
#include "ply-image.h"
#include "ply-pixel-buffer.h"
#include "ply-utils.h"

#include <linux/kd.h>

#ifndef FRAMES_PER_SECOND
#define FRAMES_PER_SECOND 30
#endif

struct _ply_image_sequence
{
        ply_array_t *frames;
        char        *image_dir;
        char        *frames_prefix;
        int          device_scale;

        long         width, height;
};

ply_image_sequence_t *
ply_image_sequence_new (const char *image_dir,
                        const char *frames_prefix,
                        int         device_scale)
{
        ply_image_sequence_t *image_sequence;

        assert (image_dir != NULL);
        assert (frames_prefix != NULL);

        image_sequence = calloc (1, sizeof(ply_image_sequence_t));

        image_sequence->frames = ply_array_new (PLY_ARRAY_ELEMENT_TYPE_POINTER);
        image_sequence->frames_prefix = strdup (frames_prefix);
        image_sequence->image_dir = strdup (image_dir);
        image_sequence->device_scale = device_scale;
        image_sequence->width = 0;
        image_sequence->height = 0;

        return image_sequence;
}

static void
ply_image_sequence_remove_frames (ply_image_sequence_t *image_sequence)
{
        int i;
        ply_pixel_buffer_t **frames;

        frames = (ply_pixel_buffer_t **) ply_array_steal_pointer_elements (image_sequence->frames);
        for (i = 0; frames[i] != NULL; i++) {
                ply_pixel_buffer_free (frames[i]);
        }
        free (frames);
}

void
ply_image_sequence_free (ply_image_sequence_t *image_sequence)
{
        if (image_sequence == NULL)
                return;

        ply_image_sequence_remove_frames (image_sequence);
        ply_array_free (image_sequence->frames);

        free (image_sequence->frames_prefix);
        free (image_sequence->image_dir);
        free (image_sequence);
}

static bool
ply_image_sequence_add_frame (ply_image_sequence_t *image_sequence,
                              const char           *filename,
                              int                   device_scale)
{
        ply_image_t *image;
        ply_pixel_buffer_t *frame;

        image = ply_image_new (filename);

        // Filenames passed in already refer to image versions at correct scale.
        if (!ply_image_load_assuming_scale (image, device_scale)) {
                ply_image_free (image);
                return false;
        }

        frame = ply_image_convert_to_pixel_buffer (image);

        ply_array_add_pointer_element (image_sequence->frames, frame);

        image_sequence->width = MAX (image_sequence->width, (long) ply_pixel_buffer_get_width (frame));
        image_sequence->height = MAX (image_sequence->height, (long) ply_pixel_buffer_get_height (frame));

        return true;
}

static bool
ply_image_sequence_add_frames (ply_image_sequence_t *image_sequence)
{
        struct dirent **entries;
        int number_of_entries;
        int number_of_frames;
        int i;
        bool load_at_scale, load_finished;
        int filename_suffix_len;
        char *at_scale_suffix;
        const char *filename_suffix;

        entries = NULL;

        number_of_entries = scandir (image_sequence->image_dir, &entries, NULL, versionsort);

        if (number_of_entries <= 0)
                return false;

        at_scale_suffix = NULL;
        load_at_scale = false;
        if (image_sequence->device_scale != 1) {
                filename_suffix_len =
                        asprintf (&at_scale_suffix, "@%d.png", image_sequence->device_scale);
                filename_suffix = at_scale_suffix;
                for (i = 0; i < number_of_entries; i++) {
                        if (strncmp (entries[i]->d_name,
                                     image_sequence->frames_prefix,
                                     strlen (image_sequence->frames_prefix)) == 0 &&
                            (strlen (entries[i]->d_name) > (unsigned long) filename_suffix_len) &&
                            strcmp (entries[i]->d_name + strlen (entries[i]->d_name) -
                                    filename_suffix_len,
                                    filename_suffix) == 0) {
                                load_at_scale = true;
                                break;
                        }
                }
        }

        if (!load_at_scale) {
                filename_suffix_len = 4;
                filename_suffix = ".png";
        }

        load_finished = false;
        for (i = 0; i < number_of_entries; i++) {
                if (strncmp (entries[i]->d_name,
                             image_sequence->frames_prefix,
                             strlen (image_sequence->frames_prefix)) == 0
                    && (strlen (entries[i]->d_name) > (unsigned long) filename_suffix_len)
                    && strcmp (entries[i]->d_name + strlen (entries[i]->d_name) - filename_suffix_len, filename_suffix) == 0
                    && (load_at_scale || !strchr (entries[i]->d_name, '@'))) {
                        char *filename;

                        filename = NULL;
                        asprintf (&filename, "%s/%s", image_sequence->image_dir, entries[i]->d_name);

                        if (!ply_image_sequence_add_frame (image_sequence,
                                                           filename,
                                                           load_at_scale ? image_sequence->device_scale : 1))
                                goto out;

                        free (filename);
                }

                free (entries[i]);
                entries[i] = NULL;
        }

        number_of_frames = ply_array_get_size (image_sequence->frames);
        if (number_of_frames == 0) {
                ply_trace ("%s directory had no files starting with %s",
                           image_sequence->image_dir, image_sequence->frames_prefix);
                goto out;
        } else {
                ply_trace ("image_sequence has %d frames", number_of_frames);
        }

        load_finished = true;

out:
        if (!load_finished) {
                ply_image_sequence_remove_frames (image_sequence);

                while (i < number_of_entries) {
                        free (entries[i]);
                        i++;
                }
        }
        free (entries);
        if (at_scale_suffix != NULL) {
                filename_suffix = NULL;
                free (at_scale_suffix);
        }

        return ply_array_get_size (image_sequence->frames) > 0;
}

bool
ply_image_sequence_load (ply_image_sequence_t *image_sequence)
{
        if (ply_array_get_size (image_sequence->frames) != 0) {
                ply_image_sequence_remove_frames (image_sequence);
                ply_trace ("reloading image_sequence with new set of frames");
        } else {
                ply_trace ("loading frames for image_sequence");
        }

        if (!ply_image_sequence_add_frames (image_sequence))
                return false;

        return true;
}

void
ply_image_sequence_draw_area (ply_image_sequence_t *image_sequence,
                              int                   frame_index,
                              ply_pixel_buffer_t   *buffer,
                              long                  x,
                              long                  y,
                              unsigned long         width,
                              unsigned long         height)
{
        ply_pixel_buffer_t *const *frames;
        int number_of_frames;

        number_of_frames = ply_array_get_size (image_sequence->frames);

        assert (frame_index >= 0 && frame_index < number_of_frames);

        frames = (ply_pixel_buffer_t *const *) ply_array_get_pointer_elements (image_sequence->frames);
        ply_pixel_buffer_fill_with_buffer (buffer,
                                           frames[frame_index],
                                           x, y);
}

long
ply_image_sequence_get_width (ply_image_sequence_t *image_sequence)
{
        return image_sequence->width;
}

long
ply_image_sequence_get_height (ply_image_sequence_t *image_sequence)
{
        return image_sequence->height;
}

int
ply_image_sequence_get_number_of_frames (ply_image_sequence_t *image_sequence)
{
        return ply_array_get_size (image_sequence->frames);
}

/* vim: set ts=4 sw=4 expandtab autoindent cindent cino={.5s,(0: */
