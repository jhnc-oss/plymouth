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

#include "ply-trigger.h"

typedef struct
{
        int         order[16];
        int         count;
        const void *expected_data;
        bool        data_matched;
} call_log_t;

typedef struct
{
        call_log_t *log;
        int         id;
} handler_data_t;

typedef struct
{
        handler_data_t        handler_data;
        ply_trigger_handler_t target_handler;
        void                 *target_user_data;
} removing_handler_data_t;

typedef struct
{
        handler_data_t               handler_data;
        void                        *expected_instance;
        bool                         instance_matched;
        ply_trigger_handler_result_t result;
} instance_handler_data_t;

typedef struct
{
        int  call_count;
        bool nested_pull_started;
} nested_handler_data_t;

static void
record_handler (void          *user_data,
                const void    *trigger_data,
                ply_trigger_t *trigger)
{
        handler_data_t *data = user_data;

        data->log->order[data->log->count++] = data->id;
        data->log->data_matched = trigger_data == data->log->expected_data;
}

static void
remove_later_handler (void          *user_data,
                      const void    *trigger_data,
                      ply_trigger_t *trigger)
{
        removing_handler_data_t *data = user_data;

        record_handler (&data->handler_data, trigger_data, trigger);
        ply_trigger_remove_handler (trigger,
                                    data->target_handler,
                                    data->target_user_data);
}

static ply_trigger_handler_result_t
record_instance_handler (void          *user_data,
                         void          *instance,
                         const void    *trigger_data,
                         ply_trigger_t *trigger)
{
        instance_handler_data_t *data = user_data;

        record_handler (&data->handler_data, trigger_data, trigger);
        data->instance_matched = instance == data->expected_instance;
        return data->result;
}

static void
pull_nested_once (void          *user_data,
                  const void    *trigger_data,
                  ply_trigger_t *trigger)
{
        nested_handler_data_t *data = user_data;

        data->call_count++;
        if (!data->nested_pull_started) {
                data->nested_pull_started = true;
                ply_trigger_pull (trigger, trigger_data);
        }
}

static bool
test_handlers_run_in_registration_order (void)
{
        int trigger_data = 42;
        call_log_t log = { .expected_data = &trigger_data };
        handler_data_t first = { .log = &log, .id = 1 };
        handler_data_t second = { .log = &log, .id = 2 };
        ply_trigger_t *trigger;

        trigger = ply_trigger_new (NULL);
        ply_trigger_add_handler (trigger, record_handler, &first);
        ply_trigger_add_handler (trigger, record_handler, &second);
        ply_trigger_pull (trigger, &trigger_data);

        PLY_TEST_ASSERT (log.count == 2);
        PLY_TEST_ASSERT (log.order[0] == 1);
        PLY_TEST_ASSERT (log.order[1] == 2);
        PLY_TEST_ASSERT (log.data_matched);

        ply_trigger_free (trigger);
        return true;
}

static bool
test_remove_handler_matches_user_data (void)
{
        call_log_t log = { 0 };
        handler_data_t removed = { .log = &log, .id = 1 };
        handler_data_t retained = { .log = &log, .id = 2 };
        ply_trigger_t *trigger;

        trigger = ply_trigger_new (NULL);
        ply_trigger_add_handler (trigger, record_handler, &removed);
        ply_trigger_add_handler (trigger, record_handler, &retained);
        ply_trigger_remove_handler (trigger, record_handler, &removed);
        ply_trigger_pull (trigger, NULL);

        PLY_TEST_ASSERT (log.count == 1);
        PLY_TEST_ASSERT (log.order[0] == 2);

        ply_trigger_free (trigger);
        return true;
}

static bool
test_ignored_pulls_are_counted (void)
{
        call_log_t log = { 0 };
        handler_data_t handler = { .log = &log, .id = 1 };
        ply_trigger_t *trigger;

        trigger = ply_trigger_new (NULL);
        ply_trigger_add_handler (trigger, record_handler, &handler);
        ply_trigger_ignore_next_pull (trigger);
        ply_trigger_ignore_next_pull (trigger);

        ply_trigger_pull (trigger, NULL);
        ply_trigger_pull (trigger, NULL);
        ply_trigger_pull (trigger, NULL);

        PLY_TEST_ASSERT (log.count == 1);

        ply_trigger_free (trigger);
        return true;
}

static bool
test_instance_handler_can_abort_dispatch (void)
{
        int instance;
        call_log_t log = { 0 };
        instance_handler_data_t first = {
                .handler_data      = { .log = &log, .id = 1 },
                .expected_instance = &instance,
                .result            = PLY_TRIGGER_HANDLER_RESULT_ABORT,
        };
        instance_handler_data_t second = {
                .handler_data      = { .log = &log, .id = 2 },
                .expected_instance = &instance,
                .result            = PLY_TRIGGER_HANDLER_RESULT_CONTINUE,
        };
        ply_trigger_t *trigger;

        trigger = ply_trigger_new (NULL);
        ply_trigger_set_instance (trigger, &instance);
        ply_trigger_add_instance_handler (trigger, record_instance_handler, &first);
        ply_trigger_add_instance_handler (trigger, record_instance_handler, &second);
        ply_trigger_pull (trigger, NULL);

        PLY_TEST_ASSERT (ply_trigger_get_instance (trigger) == &instance);
        PLY_TEST_ASSERT (first.instance_matched);
        PLY_TEST_ASSERT (log.count == 1);
        PLY_TEST_ASSERT (log.order[0] == 1);

        ply_trigger_free (trigger);
        return true;
}

static bool
test_callback_can_remove_later_handler (void)
{
        call_log_t log = { 0 };
        handler_data_t removed = { .log = &log, .id = 2 };
        handler_data_t retained = { .log = &log, .id = 3 };
        removing_handler_data_t remover = {
                .handler_data     = { .log = &log, .id = 1 },
                .target_handler   = record_handler,
                .target_user_data = &removed,
        };
        ply_trigger_t *trigger;

        trigger = ply_trigger_new (NULL);
        ply_trigger_add_handler (trigger, remove_later_handler, &remover);
        ply_trigger_add_handler (trigger, record_handler, &removed);
        ply_trigger_add_handler (trigger, record_handler, &retained);

        ply_trigger_pull (trigger, NULL);

        PLY_TEST_ASSERT (log.count == 2);
        PLY_TEST_ASSERT (log.order[0] == 1);
        PLY_TEST_ASSERT (log.order[1] == 3);

        ply_trigger_free (trigger);
        return true;
}

static bool
test_auto_free_waits_for_nested_pull (void)
{
        nested_handler_data_t data = { 0 };
        ply_trigger_t *trigger;

        trigger = ply_trigger_new (&trigger);
        ply_trigger_add_handler (trigger, pull_nested_once, &data);
        ply_trigger_pull (trigger, NULL);

        PLY_TEST_ASSERT (data.call_count == 2);
        PLY_TEST_ASSERT (trigger == NULL);

        return true;
}

static const ply_test_case_t test_cases[] =
{
        PLY_TEST_CASE (test_handlers_run_in_registration_order),
        PLY_TEST_CASE (test_remove_handler_matches_user_data),
        PLY_TEST_CASE (test_ignored_pulls_are_counted),
        PLY_TEST_CASE (test_instance_handler_can_abort_dispatch),
        PLY_TEST_CASE (test_callback_can_remove_later_handler),
        PLY_TEST_CASE (test_auto_free_waits_for_nested_pull),
};

PLY_TEST_MAIN (test_cases)
