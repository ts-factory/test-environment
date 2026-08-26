/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (C) 2026 OKTET Ltd. */
/** @file
 * @brief Replay of the scenario recorded in the te_scenario section
 */

#define TE_LGR_USER     "Scenario"

#include "te_config.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "te_defs.h"
#include "te_string.h"
#include "logger_api.h"
#include "tapi_test_log.h"

#ifdef __GNUC__
extern const char __start_te_scenario[] __attribute__((weak));
extern const char __stop_te_scenario[]  __attribute__((weak));
#endif

/* Log level and user for a record kind, NULL user for unknown. */
static void
kind_to_log(const char *kind, unsigned int *level, const char **user)
{
    *level = TE_LL_CONTROL | TE_LL_RING;
    if (strcmp(kind, "STEP") == 0)
        *user = TE_USER_STEP;
    else if (strcmp(kind, "SUBSTEP") == 0)
        *user = TE_USER_SUBSTEP;
    else if (strcmp(kind, "PUSH") == 0)
        *user = TE_USER_STEP_PUSH;
    else if (strcmp(kind, "POP") == 0)
        *user = TE_USER_STEP_POP;
    else if (strcmp(kind, "NEXT") == 0)
        *user = TE_USER_STEP_NEXT;
    else if (strcmp(kind, "RESET") == 0)
        *user = TE_USER_STEP_RESET;
    else if (strcmp(kind, "PUSH_INFO") == 0)
    {
        *level = TE_LL_CONTROL | TE_LL_INFO;
        *user = TE_USER_STEP_PUSH;
    }
    else if (strcmp(kind, "POP_INFO") == 0)
    {
        *level = TE_LL_CONTROL | TE_LL_INFO;
        *user = TE_USER_STEP_POP;
    }
    else
        *user = NULL;
}

/* Value of a name=value argv pair, NULL if absent. */
static const char *
arg_value(int argc, char **argv, const char *name, size_t name_len)
{
    int i;

    for (i = 0; i < argc; i++)
    {
        if (strncmp(argv[i], name, name_len) == 0 &&
            argv[i][name_len] == '=')
            return argv[i] + name_len + 1;
    }
    return NULL;
}

/* Append text to dest substituting @p refs from argv pairs. */
static void
subst_params(te_string *dest, const char *text, int argc, char **argv)
{
    const char *p = text;

    while (*p != '\0')
    {
        const char *ref = strstr(p, "@p ");
        const char *name;
        size_t      len = 0;
        const char *value;

        if (ref == NULL)
            break;
        name = ref + strlen("@p ");
        while (isalnum((unsigned char)name[len]) || name[len] == '_')
            len++;
        value = len == 0 ? NULL : arg_value(argc, argv, name, len);
        te_string_append(dest, "%.*s", (int)(ref - p), p);
        if (value != NULL)
            te_string_append(dest, "%s", value);
        else
            te_string_append(dest, "%.*s", (int)(name + len - ref), ref);
        p = name + len;
    }
    te_string_append(dest, "%s", p);
}

bool
tapi_test_scenario_replay(int argc, char **argv)
{
#ifdef __GNUC__
    const char *mode = getenv("TE_TEST_SCENARIO");
    bool        params;
    const char *rec;

    if (mode == NULL || *mode == '\0')
        return false;
    params = (strcmp(mode, "params") == 0);

    if (&__start_te_scenario == NULL)
        return true;

    for (rec = __start_te_scenario; rec < __stop_te_scenario; )
    {
        const char  *kind;
        const char  *file;
        const char  *line;
        const char  *text;
        unsigned int level;
        const char  *user;

        /* Skip inter-record padding defensively. */
        while (rec < __stop_te_scenario && *rec == '\0')
            rec++;
        if (rec >= __stop_te_scenario)
            break;

        kind = rec;
        file = kind + strlen(kind) + 1;
        line = file + strlen(file) + 1;
        text = line + strlen(line) + 1;

        rec = text + strlen(text) + 1;

        kind_to_log(kind, &level, &user);
        if (user == NULL)
        {
            WARN("Unknown scenario record kind '%s' at %s:%s",
                 kind, file, line);
            continue;
        }
        if (params)
        {
            te_string subst = TE_STRING_INIT;

            subst_params(&subst, text, argc, argv);
            LGR_MESSAGE(level, user, "%s", subst.ptr == NULL ? ""
                                                             : subst.ptr);
            te_string_free(&subst);
        }
        else
        {
            LGR_MESSAGE(level, user, "%s", text);
        }
    }
    return true;
#else
    UNUSED(argc);
    UNUSED(argv);
    return false;
#endif
}
