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

#include <stdlib.h>
#include <string.h>

#include "script-execute.h"
#include "script-object.h"
#include "script-parse.h"
#include "script.h"

typedef struct
{
        script_state_t *state;
        script_op_t *op;
} executed_script_t;

typedef struct
{
        int calls;
        double offset;
        double argument_count;
} native_context_t;

static bool
execute_script (const char        *source,
                executed_script_t *script)
{
        script_return_t result;

        memset (script, 0, sizeof(*script));
        script->op = script_parse_string (source, "test.script");
        if (script->op == NULL)
                return false;

        script->state = script_state_new (NULL);
        result = script_execute (script->state, script->op);
        script_obj_unref (result.object);

        return result.type != SCRIPT_RETURN_TYPE_FAIL;
}

static void
free_executed_script (executed_script_t *script)
{
        script_state_destroy (script->state);
        script_parse_op_free (script->op);
}

static bool
test_arithmetic_assignment_comparison_and_strings (void)
{
        static const char source[] =
                "value = 2 + 3 * 4;"
                "value += 2;"
                "quotient = value / 4;"
                "remainder = value % 5;"
                "text = \"boot\" + \" splash\";"
                "matches = value == 16 && quotient == 4;";
        executed_script_t script;
        char *text;

        PLY_TEST_ASSERT (execute_script (source, &script));
        PLY_TEST_ASSERT (script_obj_hash_get_number (script.state->global,
                                                      "value") == 16);
        PLY_TEST_ASSERT (script_obj_hash_get_number (script.state->global,
                                                      "quotient") == 4);
        PLY_TEST_ASSERT (script_obj_hash_get_number (script.state->global,
                                                      "remainder") == 1);
        PLY_TEST_ASSERT (script_obj_hash_get_bool (script.state->global,
                                                   "matches"));
        text = script_obj_hash_get_string (script.state->global, "text");
        PLY_TEST_ASSERT (text != NULL);
        PLY_TEST_ASSERT (strcmp (text, "boot splash") == 0);

        free (text);
        free_executed_script (&script);
        return true;
}

static bool
test_loops_break_and_continue_update_state (void)
{
        static const char source[] =
                "sum = 0;"
                "for (index = 0; index < 6; index++) {"
                "  if (index == 2) continue;"
                "  if (index == 5) break;"
                "  sum += index;"
                "}"
                "count = 0;"
                "while (count < 3) count++;"
                "do count--; while (count > 1);";
        executed_script_t script;

        PLY_TEST_ASSERT (execute_script (source, &script));
        PLY_TEST_ASSERT (script_obj_hash_get_number (script.state->global,
                                                      "sum") == 8);
        PLY_TEST_ASSERT (script_obj_hash_get_number (script.state->global,
                                                      "index") == 5);
        PLY_TEST_ASSERT (script_obj_hash_get_number (script.state->global,
                                                      "count") == 1);

        free_executed_script (&script);
        return true;
}

static bool
test_functions_use_local_parameters_and_global_state (void)
{
        static const char source[] =
                "global.total = 1;"
                "fun add(value) {"
                "  local.scaled = value * 2;"
                "  global.total += scaled;"
                "  return global.total;"
                "}"
                "first = add(3);"
                "second = add(4);";
        executed_script_t script;

        PLY_TEST_ASSERT (execute_script (source, &script));
        PLY_TEST_ASSERT (script_obj_hash_get_number (script.state->global,
                                                      "first") == 7);
        PLY_TEST_ASSERT (script_obj_hash_get_number (script.state->global,
                                                      "second") == 15);
        PLY_TEST_ASSERT (script_obj_hash_get_number (script.state->global,
                                                      "total") == 15);

        free_executed_script (&script);
        return true;
}

static bool
test_sets_and_dynamic_hash_keys_store_values (void)
{
        static const char source[] =
                "values = [10, 20, 30];"
                "selected = values[1];"
                "values[1] = 25;"
                "record.name = \"plymouth\";"
                "key = \"version\";"
                "record[key] = 26;"
                "updated = values[1];";
        executed_script_t script;
        script_obj_t *record;
        char *name;

        PLY_TEST_ASSERT (execute_script (source, &script));
        PLY_TEST_ASSERT (script_obj_hash_get_number (script.state->global,
                                                      "selected") == 20);
        PLY_TEST_ASSERT (script_obj_hash_get_number (script.state->global,
                                                      "updated") == 25);

        record = script_obj_hash_get_element (script.state->global, "record");
        name = script_obj_hash_get_string (record, "name");
        PLY_TEST_ASSERT (name != NULL);
        PLY_TEST_ASSERT (strcmp (name, "plymouth") == 0);
        PLY_TEST_ASSERT (script_obj_hash_get_number (record, "version") == 26);

        free (name);
        script_obj_unref (record);
        free_executed_script (&script);
        return true;
}

static script_return_t
native_add_with_offset (script_state_t *state,
                        void           *user_data)
{
        native_context_t *context = user_data;
        script_obj_t *arguments;
        double left;
        double right;

        context->calls++;
        arguments = script_obj_hash_get_element (state->local, "_args");
        context->argument_count = script_obj_hash_get_number (arguments,
                                                              "count");
        script_obj_unref (arguments);
        left = script_obj_hash_get_number (state->local, "left");
        right = script_obj_hash_get_number (state->local, "right");

        return script_return_obj (
                script_obj_new_number (left + right + context->offset));
}

static bool
test_native_function_receives_named_arguments (void)
{
        native_context_t context = {
                .offset = 0.5,
        };
        script_return_t result;
        script_state_t *state;
        script_op_t *op;

        state = script_state_new (NULL);
        script_add_native_function (state->global,
                                    "NativeAdd",
                                    native_add_with_offset,
                                    &context,
                                    "left",
                                    "right",
                                    NULL);
        op = script_parse_string ("result = NativeAdd(4, 5);",
                                  "native.script");
        PLY_TEST_ASSERT (op != NULL);

        result = script_execute (state, op);
        PLY_TEST_ASSERT (result.type == SCRIPT_RETURN_TYPE_NORMAL);
        PLY_TEST_ASSERT (context.calls == 1);
        PLY_TEST_ASSERT (context.argument_count == 2);
        PLY_TEST_ASSERT (script_obj_hash_get_number (state->global,
                                                      "result") == 9.5);

        script_obj_unref (result.object);
        script_state_destroy (state);
        script_parse_op_free (op);
        return true;
}

static int native_free_count;

static void
on_native_free (script_obj_t *object)
{
        if (object->data.native.object_data == NULL)
                native_free_count = -100;
        else
                native_free_count++;
}

static bool
test_native_objects_match_class_and_release_once (void)
{
        script_obj_native_class_t *class;
        script_obj_t *object;
        int native_data = 42;

        native_free_count = 0;
        class = script_obj_native_class_new (on_native_free,
                                             "test-native",
                                             NULL);
        object = script_obj_new_native (&native_data, class);

        PLY_TEST_ASSERT (script_obj_is_native (object));
        PLY_TEST_ASSERT (script_obj_is_native_of_class (object, class));
        PLY_TEST_ASSERT (script_obj_is_native_of_class_name (object,
                                                             "test-native"));
        PLY_TEST_ASSERT (script_obj_as_native_of_class (object, class) ==
                         &native_data);

        script_obj_unref (object);
        PLY_TEST_ASSERT (native_free_count == 1);
        script_obj_native_class_destroy (class);
        return true;
}

static bool
test_parser_rejects_incomplete_constructs (void)
{
        script_op_t *op;

        op = script_parse_string ("value = ;", "missing-value.script");
        PLY_TEST_ASSERT (op == NULL);

        op = script_parse_string ("if (1) { value = 2;",
                                  "missing-brace.script");
        PLY_TEST_ASSERT (op == NULL);

        op = script_parse_string ("fun broken(a b) { return a; }",
                                  "bad-parameters.script");
        PLY_TEST_ASSERT (op == NULL);
        return true;
}

static bool
parse_shipped_script (const char *path)
{
        script_op_t *op;

        op = script_parse_file (path);
        if (op == NULL)
                return false;

        script_parse_op_free (op);
        return true;
}

static bool
test_shipped_image_library_parses (void)
{
        return parse_shipped_script (
                PLYMOUTH_SOURCE_ROOT
                "/src/plugins/splash/script/script-lib-image.script");
}

static const ply_test_case_t test_cases[] =
{
        PLY_TEST_CASE (test_arithmetic_assignment_comparison_and_strings),
        PLY_TEST_CASE (test_loops_break_and_continue_update_state),
        PLY_TEST_CASE (test_functions_use_local_parameters_and_global_state),
        PLY_TEST_CASE (test_sets_and_dynamic_hash_keys_store_values),
        PLY_TEST_CASE (test_native_function_receives_named_arguments),
        PLY_TEST_CASE (test_native_objects_match_class_and_release_once),
        PLY_TEST_CASE (test_parser_rejects_incomplete_constructs),
        PLY_TEST_CASE (test_shipped_image_library_parses),
};

PLY_TEST_MAIN (test_cases)
