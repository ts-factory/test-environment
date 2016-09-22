/** @file
 * @brief Test API to use setjmp/longjmp.
 *
 * Definition of API to deal with thread-safe stack of jumps.
 *
 *
 * Copyright (C) 2004 Test Environment authors (see file AUTHORS
 * in the root directory of the distribution).
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA  02111-1307  USA
 *
 *
 * @author Andrew Rybchenko <Andrew.Rybchenko@oktetlabs.ru>
 *
 * $Id: tapi_jmp.h 33201 2006-11-03 14:36:37Z arybchik $
 */

#ifndef __TE_TAPI_SH_ENV_H__
#define __TE_TAPI_SH_ENV_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Set shell environment for the given agent and may be restart
 * a PCO so it's aware.
 *
 * @param pco            PCO handle
 * @param env_name       Name of the environment variable
 * @param env_value      New value
 * @param force          Should we only add or overwrite?
 * @param restart        Should the PCO be restarted
 *
 * @result errno
 */
static inline te_errno
tapi_sh_env_set(rcf_rpc_server *pco,
                const char *env_name, const char *env_value,
                te_bool force, te_bool restart)
{
    cfg_handle handle;
    te_errno rc = 0;

    rc = cfg_find_fmt(&handle, "/agent:%s/env:%s", pco->ta, env_name);
    if (rc != 0)
    {
        rc = 0;
        /* add the instance */
        CHECK_RC(cfg_add_instance_fmt(&handle,
                                      CFG_VAL(STRING, env_value),
                                      "/agent:%s/env:%s",
                                      pco->ta, env_name));
    }
    else if (force)
        /* exists */
        CHECK_RC(cfg_set_instance_fmt(CFG_VAL(STRING, env_value),
                                      "/agent:%s/env:%s",
                                      pco->ta, env_name));
    else
        rc = TE_EEXIST;

    if (rc == 0 && restart)
        rcf_rpc_server_restart(pco);

    return rc;
}

/**
 * Get shell environment for the given agent.
 *
 * @param pco            PCO handle
 * @param env_name       Name of the environment variable
 * @param val            Location for value, memory is allocated with
 *                       malloc, so the obtained string must be freed after
 *                       using
 *
 * @result errno
 */
static inline te_errno
tapi_sh_env_get(rcf_rpc_server *pco, const char *env_name, char **val)
{
    cfg_val_type  val_type = CVT_STRING;
    te_errno      rc;

    rc = cfg_get_instance_fmt(&val_type, val, "/agent:%s/env:%s",
                              pco->ta, env_name);
    if (rc != 0)
        ERROR("Failed to get env %s", env_name);

    return rc;
}

/**
 * Get int shell environment for the given agent.
 *
 * @param pco            PCO handle
 * @param env_name       Name of the environment variable
 * @param val            Location for value
 *
 * @result errno
 */
static inline te_errno
tapi_sh_env_get_int(rcf_rpc_server *pco, const char *env_name, int *val)
{
    te_errno rc;
    char *strval;

    if ((rc = tapi_sh_env_get(pco, env_name, &strval)) != 0)
        return rc;

    *val = atoi(strval);
    free(strval);

    return 0;
}

/**
 * Set integer shell environment for the given agent and may be restart
 * a PCO so it's aware.
 *
 * @param pco            PCO handle
 * @param env_name       Name of the environment variable
 * @param env_value      New value
 * @param force          Should we only add or overwrite?
 * @param restart        Should the PCO be restarted
 *
 * @result errno
 */
static inline te_errno
tapi_sh_env_set_int(rcf_rpc_server *pco,
                    const char *env_name, int env_value,
                    te_bool force, te_bool restart)
{
    char env_value_str[32];
    int  res;

    if ((res = snprintf(env_value_str, sizeof(env_value_str),
                        "%d", env_value)) < 0 ||
        res > (int)sizeof(env_value_str))
    {
        ERROR("Failed to convert int value to string to set env");
        return TE_RC(TE_TAPI, TE_EINVAL);
    }

    return tapi_sh_env_set(pco, env_name, env_value_str, force, restart);
}

/**
 * Check whether environment variable exists, save its current value
 * if it does, then set a new value.
 *
 * @param pco               PCO handle
 * @param env_name          Name of the environment variable
 * @param existed     [out] Whether this variable already existed
 *                          in an environment or not
 * @param old_value   [out] Where to save existing value
 * @param new_value         New value
 * @param restart           Should the PCO be restarted
 *
 * @result errno
 */
static inline te_errno
tapi_sh_env_save_set(rcf_rpc_server *pco,
                     const char *env_name, te_bool *existed,
                     char **old_value,
                     const char *new_value,
                     te_bool restart)
{
    cfg_handle  handle;
    te_errno    rc = 0;

    cfg_val_type     type;

    if (existed != NULL)
        *existed = FALSE;
    if (old_value != NULL)
        *old_value = NULL;

    rc = cfg_find_fmt(&handle, "/agent:%s/env:%s", pco->ta, env_name);
    if (rc != 0)
    {
        /* add the instance */
        CHECK_RC(cfg_add_instance_fmt(&handle,
                                      CFG_VAL(STRING, new_value),
                                      "/agent:%s/env:%s",
                                      pco->ta, env_name));
    }
    else
    {
        /* exists */
        if (existed != NULL)
            *existed = TRUE;

        if (old_value != NULL)
        {
            type = CVT_STRING;
            CHECK_RC(cfg_get_instance(handle, &type, old_value));
        }
        CHECK_RC(cfg_set_instance(handle, CVT_STRING, new_value));
    }

    if (restart)
        CHECK_RC(rcf_rpc_server_restart(pco));

    return 0;
}

/**
 * Unset environment for the agent and may be restart given PCO.
 *
 * @param pco         PCO handle
 * @param env_name    Name of the environment variable
 * @param force       Ignore if the variable was not set
 * @param restart     Should the PCO be restarted?
 */
static inline te_errno
tapi_sh_env_unset(rcf_rpc_server *pco, const char *env_name,
                   te_bool force, te_bool restart)
{
    cfg_handle handle;
    te_errno rc = 0;

    rc = cfg_find_fmt(&handle, "/agent:%s/env:%s", pco->ta, env_name);
    if (rc != 0)
        rc = TE_ENOENT;
    else
        CHECK_RC(cfg_del_instance(handle, 1));

    /* no need to restart if nothing was changed */
    if (rc == 0 && restart)
        rcf_rpc_server_restart(pco);

    return (force && rc == TE_ENOENT) ? 0 : rc;
}

/**
 * Set integer shell environment and save previous value if it is necessary.
 *
 * @param pco            PCO handle
 * @param env_name       Name of the environment variable
 * @param env_value      New value
 * @param force          Should we only add or overwrite?
 * @param restart        Should the PCO be restarted
 * @param existed        [out] Whether this variable already existed
 *                       in an environment or not
 * @param old_value      [out] Where to save existing value
 *
 * @result Status code
 */
static inline te_errno
tapi_sh_env_save_set_int(rcf_rpc_server *pco,
                         const char *env_name, int env_value,
                         te_bool restart, te_bool *existed,
                         int *old_value)
{
    char env_value_str[32];
    char *old_value_str = NULL;
    int  res;

    if ((res = snprintf(env_value_str, sizeof(env_value_str),
                        "%d", env_value)) < 0 ||
        res > (int)sizeof(env_value_str))
    {
        ERROR("Failed to convert int value to string to set env");
        return TE_RC(TE_TAPI, TE_EINVAL);
    }

    if ((res = tapi_sh_env_save_set(pco, env_name, existed, &old_value_str,
                                   env_value_str, restart)) != 0)
        return res;

    if (old_value != NULL && old_value_str != NULL)
        *old_value = strtol(old_value_str, NULL, 0);

    free(old_value_str);

    return 0;
}

/**
 * Rollback environment variable
 *
 * @param pco            PCO handle
 * @param env_name       The environment variable name
 * @param existed        Did this variable exist in an environment or not
 * @param env_value      Value to set if the env existed
 * @param restart        Should the PCO be restarted
 *
 * @result Status code
 */
static inline te_errno
tapi_sh_env_rollback_int(rcf_rpc_server *pco, const char *env_name,
                         te_bool existed, int env_value, te_bool restart)
{
    if (!existed)
        return tapi_sh_env_unset(pco, env_name, FALSE, restart);

    return tapi_sh_env_set_int(pco, env_name, env_value, TRUE, restart);
}

/**
 * Get boolean environment variable on engine. The function stops the test
 * if the variable keeps unexpected value.
 *
 * @param var_name  The variable name
 *
 * @return Boolean value
 */
static inline te_bool
tapi_getenv_bool(const char *var_name)
{
    const char *val;
    val = getenv(var_name);
    if (val == NULL)
        return FALSE;

    if (strcmp(val, "0") == 0)
        return FALSE;

    if (strcmp(val, "1") != 0)
        TEST_FAIL("Environment variable %s keeps non-boolean value '%s'",
                  var_name, val);

    return TRUE;
}

#ifdef __cplusplus
}
#endif

#endif
