/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (C) 2026 OKTET Labs Ltd. All rights reserved. */
/** @file
 * @brief Test for strict variable expansion
 *
 * Testing te_string_expand_kvpairs_strict().
 */

/** @page tools_expand_strict te_expand.h strict expansion test
 *
 * @objective Testing strict variable expansion.
 *
 * Check that an undefined reference without a default value is
 * reported as an error, and that everything else expands exactly as
 * the non-strict expansion does.
 *
 * @par Test sequence:
 */

/** Logging subsystem entity name */
#define TE_TEST_NAME    "tools/expand_strict"

/** The name every failing template of the package refers to */
#define UNDEF_NAME      "unknown"

#include <string.h>
#include "te_config.h"

#include "tapi_test.h"
#include "te_kvpair.h"
#include "te_expand.h"
#include "te_str.h"
#include "te_string.h"

int
main(int argc, char **argv)
{
    const char *template;
    const char *expanded;
    bool must_fail;
    te_kvpair_h kvpairs;
    te_string actual = TE_STRING_INIT;
    te_string undef_ref = TE_STRING_INIT;
    te_errno expand_rc;

    te_kvpair_init(&kvpairs);

    TEST_START;
    TEST_GET_STRING_PARAM(template);
    TEST_GET_BOOL_PARAM(must_fail);
    TEST_GET_OPT_STRING_PARAM(expanded);

    te_kvpair_push(&kvpairs, "var1", "%s", "value1");
    te_kvpair_push(&kvpairs, "var2", "%s", "value2");

    TEST_STEP("Expand the template in strict mode");
    expand_rc = te_string_expand_kvpairs_strict(template, NULL, &kvpairs,
                                                &actual, &undef_ref);

    if (must_fail)
    {
        TEST_STEP("Check that an undefined reference is reported");
        if (expand_rc != TE_ENOENT)
        {
            ERROR("Expected TE_ENOENT, got %r", expand_rc);
            TEST_VERDICT("Undefined reference was not reported");
        }
        /*
         * Every failing template of the package refers to the same
         * undefined name, so the expected report is known without
         * a parameter of its own.
         */
        if (strcmp(te_string_value(&undef_ref), UNDEF_NAME) != 0)
        {
            ERROR("Expected the undefined reference '%s', got '%s'",
                  UNDEF_NAME, te_string_value(&undef_ref));
            TEST_VERDICT("Unexpected undefined reference reported");
        }
    }
    else
    {
        TEST_STEP("Check the expansion result");
        if (expand_rc != 0)
        {
            ERROR("Unexpected failure: %r", expand_rc);
            TEST_VERDICT("Strict expansion failed unexpectedly");
        }
        if (strcmp(te_str_empty_if_null(expanded),
                   te_string_value(&actual)) != 0)
        {
            ERROR("Expected '%s', got '%s'", te_str_empty_if_null(expanded),
                  te_string_value(&actual));
            TEST_VERDICT("Unexpected expansion");
        }
        if (undef_ref.len != 0)
        {
            ERROR("A reference '%s' was reported undefined",
                  te_string_value(&undef_ref));
            TEST_VERDICT("An undefined reference was reported on success");
        }
    }

    TEST_STEP("Check that the non-strict expansion still succeeds");
    te_string_reset(&actual);
    CHECK_RC(te_string_expand_kvpairs(template, NULL, &kvpairs, &actual));

    TEST_SUCCESS;

cleanup:

    te_kvpair_fini(&kvpairs);
    te_string_free(&actual);
    te_string_free(&undef_ref);
    TEST_END;
}
