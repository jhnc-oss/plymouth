/*
 * Copyright (C) 2026 Red Hat, Inc.
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
 */

#include "ply-test.h"
#include "ply-test-seeded-random.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include "ply-image.h"
#include "ply-pixel-buffer.h"

#define FUZZ_ITERATION_COUNT 512
#define MAX_FUZZ_INPUT_SIZE 512

typedef struct
{
        const uint8_t *bytes;
        size_t         size;
} image_input_example_t;

static const uint8_t rgba_png[] = {
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d,
        0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01,
        0x08, 0x06, 0x00, 0x00, 0x00, 0xf4, 0x22, 0x7f, 0x8a, 0x00, 0x00, 0x00,
        0x11, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9c, 0x63, 0x10, 0x54, 0x32, 0xfe,
        0xff, 0xbf, 0x81, 0xa1, 0x01, 0x00, 0x0d, 0xa8, 0x03, 0x65, 0xc1, 0xf2,
        0xb1, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42,
        0x60, 0x82,
};

static const uint8_t bottom_up_bmp[] = {
        0x42, 0x4d, 0x46, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x36, 0x00,
        0x00, 0x00, 0x28, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02, 0x00,
        0x00, 0x00, 0x01, 0x00, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0xff, 0x00, 0x00, 0xff, 0xff, 0xff, 0x00, 0x00,
        0x00, 0x00, 0xff, 0x00, 0xff, 0x00, 0x00, 0x00,
};

static const image_input_example_t image_input_examples[] = {
        { rgba_png,      sizeof(rgba_png)      },
        { bottom_up_bmp, sizeof(bottom_up_bmp) },
};

static bool
write_all (int            fd,
           const uint8_t *bytes,
           size_t         size)
{
        size_t offset = 0;

        while (offset < size) {
                ssize_t bytes_written;

                bytes_written = write (fd, bytes + offset, size - offset);
                if (bytes_written < 0) {
                        if (errno == EINTR)
                                continue;

                        return false;
                }

                if (bytes_written == 0)
                        return false;

                offset += (size_t) bytes_written;
        }

        return true;
}

static bool
write_input (const char    *path,
             const uint8_t *bytes,
             size_t         size)
{
        bool written;
        int fd;

        fd = open (path, O_WRONLY | O_TRUNC | O_CLOEXEC);
        if (fd < 0)
                return false;

        written = write_all (fd, bytes, size);
        if (close (fd) < 0)
                written = false;

        return written;
}

static size_t
generate_input (ply_test_seeded_random_t *random,
                uint8_t                  *bytes)
{
        const image_input_example_t *example;
        size_t input_size;
        size_t copied_size;
        size_t mutation_count;
        size_t i;

        if (ply_test_seeded_random_range (random, 4) == 0) {
                input_size = ply_test_seeded_random_range (
                        random,
                        MAX_FUZZ_INPUT_SIZE + 1);
                ply_test_seeded_random_fill (random, bytes, input_size);
                return input_size;
        }

        example = &image_input_examples[
                ply_test_seeded_random_range (
                        random,
                        sizeof(image_input_examples) /
                        sizeof(image_input_examples[0]))];

        switch (ply_test_seeded_random_range (random, 3)) {
        case 0:
                input_size = example->size;
                break;
        case 1:
                input_size = ply_test_seeded_random_range (random,
                                                           example->size + 1);
                break;
        default:
                input_size = ply_test_seeded_random_range (
                        random,
                        MAX_FUZZ_INPUT_SIZE + 1);
                break;
        }

        copied_size = input_size < example->size ? input_size : example->size;
        memcpy (bytes, example->bytes, copied_size);
        ply_test_seeded_random_fill (random,
                                     bytes + copied_size,
                                     input_size - copied_size);

        mutation_count = ply_test_seeded_random_range (random, 9);
        for (i = 0; i < mutation_count && input_size > 0; i++) {
                bytes[ply_test_seeded_random_range (random, input_size)] =
                        (uint8_t) (ply_test_seeded_random_next (random) >> 24);
        }

        return input_size;
}

static bool
exercise_image (const char *path)
{
        ply_image_t *image;
        ply_pixel_buffer_t *buffer;
        long width;
        long height;
        bool valid = true;

        image = ply_image_new (path);
        if (!ply_image_load (image)) {
                ply_image_free (image);
                return true;
        }

        width = ply_image_get_width (image);
        height = ply_image_get_height (image);
        buffer = ply_image_get_buffer (image);

        if (width <= 0 || height <= 0 || buffer == NULL ||
            (unsigned long) width != ply_pixel_buffer_get_width (buffer) ||
            (unsigned long) height != ply_pixel_buffer_get_height (buffer) ||
            ply_image_get_data (image) !=
            ply_pixel_buffer_get_argb32_data (buffer))
                valid = false;

        ply_image_free (image);
        return valid;
}

static bool
test_generated_images_complete_loading (void)
{
        ply_test_seeded_random_t random = {
                .state = UINT32_C (0x905f3c1d),
        };
        uint8_t input[MAX_FUZZ_INPUT_SIZE];
        char path[] = "/tmp/plymouth-image-fuzz-XXXXXX";
        size_t iteration;
        bool passed = true;
        int fd;

        fd = mkstemp (path);
        PLY_TEST_ASSERT (fd >= 0);
        PLY_TEST_ASSERT (close (fd) == 0);

        for (iteration = 0; iteration < FUZZ_ITERATION_COUNT; iteration++) {
                size_t input_size;

                input_size = generate_input (&random, input);
                if (!write_input (path, input, input_size) ||
                    !exercise_image (path)) {
                        passed = false;
                        break;
                }
        }

        if (unlink (path) < 0)
                passed = false;

        return passed;
}

static const ply_test_case_t test_cases[] =
{
        PLY_TEST_CASE (test_generated_images_complete_loading),
};

PLY_TEST_MAIN (test_cases)
