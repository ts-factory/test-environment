/** @file
 * @brief Configurator
 *
 * TA interaction auxiliary routines
 *
 * Copyright (C) 2003 Test Environment authors (see file AUTHORS in the
 * root directory of the distribution).
 *
 * Test Environment is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * Test Environment is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA  02111-1307  USA
 *
 *
 * @author Elena Vengerova <Elena.Vengerova@oktetlabs.ru>
 *
 * $Id$
 */

#include "rcf_api.h"
#include "conf_defs.h"


#define TA_LIST_SIZE    64
char *cfg_ta_list = NULL;
static size_t ta_list_size = TA_LIST_SIZE;

#define TA_BUF_SIZE     8192
char *cfg_get_buf = NULL;
static int cfg_get_buf_len = TA_BUF_SIZE;

/**
 * Initialize list of Test Agemts.
 *
 * @return status code (see errno.h)
 */
static int
ta_list_init()
{
    char *ta;
    int   rc;
    
    if ((cfg_get_buf = (char *)malloc(cfg_get_buf_len)) == NULL)
        return ENOMEM;
        
    while (TRUE)
    {
        if ((cfg_ta_list = (char *)calloc(ta_list_size, 1)) == NULL)
            return ENOMEM;

        rc = rcf_get_ta_list(cfg_ta_list, &ta_list_size);
        if (rc == 0)
            break;

        free(cfg_ta_list);
        cfg_ta_list = NULL;
        if (rc != ETESMALLBUF)
            return rc;

        ta_list_size += TA_LIST_SIZE;
    }
    for (ta = cfg_ta_list;
         ta < cfg_ta_list + ta_list_size;
         ta += strlen(ta) + 1)
        if (strlen(ta) >= CFG_INST_NAME_MAX)
        {
            ERROR("Too long Test Agent name");
            return EINVAL;
        }
    return 0;
}

/**
 * Add instances for all agents.
 *
 * @return status code (see te_errno.h)
 */
int
cfg_ta_add_agent_instances()
{
    char *ta;
    int   rc;
    int   i = 1;
    
    if (cfg_ta_list == NULL && (rc = ta_list_init()) != 0)
        return rc;

    for (ta = cfg_ta_list;
         ta < cfg_ta_list + ta_list_size;
         ta += strlen(ta) + 1, ++i)
    {
        if ((cfg_all_inst[i] =
                 (cfg_instance *)calloc(sizeof(cfg_instance), 1)) == NULL ||
            (cfg_all_inst[i]->oid = (char *)malloc(strlen("/agent:") +
                                                   strlen(ta) + 1)) == NULL)
        {
            for (; i > 0; i--)
            {
                free(cfg_all_inst[i]->oid);
                free(cfg_all_inst[i]);
            }
            return ENOMEM;
        }
        strcpy(cfg_all_inst[i]->name, ta);
        sprintf(cfg_all_inst[i]->oid, "/agent:%s", ta);
        cfg_all_inst[i]->handle = i | (cfg_inst_seq_num++) << 16;
        cfg_all_inst[i]->obj = cfg_all_obj[1];
        if (i == 1)
            cfg_all_inst[0]->son = cfg_all_inst[i];
        else
            cfg_all_inst[i - 1]->brother = cfg_all_inst[i];
    }
    return 0;
}


/**
 * Reboot all Test Agents (before re-initializing of the Configurator).
 *
 * @return status code (see te_errno.h)
 */
int
cfg_ta_reboot_all(void)
{
    char *ta;
    int   rc;

    for (ta = cfg_ta_list;
         ta < cfg_ta_list + ta_list_size;
         ta += strlen(ta) + 1)
    {
        rc = rcf_ta_reboot(ta, NULL, NULL);
        if (rc != 0 && rc != EINPROGRESS)
            return rc;
    }

    return 0;
}

/**
 * Synchronize one object instance on the TA.
 *
 * @param ta      Test Agent name
 * @param oid     object instance identifier
 *
 * @return status code (see te_errno.h)
 */
static int
sync_ta_instance(char *ta, char *oid)
{
    cfg_object   *obj = cfg_get_object(oid);
    cfg_handle    handle = CFG_HANDLE_INVALID;
    cfg_inst_val  val;
    int           rc;

    if (obj == NULL)
        return 0;

    rc = cfg_db_find(oid, &handle);
    if (rc != 0 && rc != ENOENT)
        return rc;

    VERB("Add TA '%s' object instance '%s'", ta, oid);

    if (obj->type == CVT_NONE)
        return rc == 0 ? rc :  cfg_db_add(oid, &handle, CVT_NONE,
                                          (cfg_inst_val)0);

    while (TRUE)
    {
        rc = rcf_ta_cfg_get(ta, 0, oid, cfg_get_buf, cfg_get_buf_len);
        if (rc == ETESMALLBUF)
        {
            cfg_get_buf_len <<= 1;

            cfg_get_buf = (char *)realloc(cfg_get_buf, cfg_get_buf_len);
            if (cfg_get_buf == NULL)
            {
                ERROR("Memory allocation failure");
                return ENOMEM;
            }
        }
        else if (TE_RC_GET_ERROR(rc) == ENOENT || rc == 0)
            break;
        else
        {
            ERROR("Failed(0x%x) to get '%s' from TA '%s'", rc, oid, ta);
            return rc;
        }
    }

    if (rc == ENOENT)
    {
        if (handle != CFG_HANDLE_INVALID)
            cfg_db_del(handle);
        return 0;
    }

    if ((rc = cfg_types[obj->type].str2val(cfg_get_buf, &val)) != 0)
    {
        ERROR("Conversion of '%s' to value type %d for OID '%s' "
                "failed", cfg_get_buf, obj->type, oid);
        return rc;
    }

    if (handle == CFG_HANDLE_INVALID)
        rc = cfg_db_add(oid, &handle, obj->type, val);
    else
        rc = cfg_db_set(handle, val);

    cfg_types[obj->type].free(val);

    return rc;
}

/* Remove entries, which do not mention in the list, from database */
static void
remove_excessive(cfg_instance *inst, char *list)
{
    cfg_instance *tmp;
    cfg_instance *next;
    int len = strlen(inst->oid);
    char *s;

    for (tmp = inst->son; tmp != NULL; tmp = next)
    {
        next = tmp->brother;
        remove_excessive(tmp, list);
    }
    
    if (strcmp(inst->obj->subid, "agent") == 0)
        return;

    for (s = strstr(list, inst->oid); s != NULL; s = strstr(s + 1, inst->oid))
    {
        if (*(s + len) == ' ' || *(s + len) == 0)
            break;
    }
    if (s == NULL)
        cfg_db_del(inst->handle);
}

/**
 * Synchronize tree of object instances on the TA.
 *
 * @param ta      Test Agent name
 * @param oid     root object instance identifier
 *
 * @return status code (see te_errno.h)
 */
static int
sync_ta_subtree(char *ta, char *oid)
{
    char  *list;
    char  *tmp;
    char  *next;
    char  *limit;
    char **sorted;
    int    n = 0;
    int    rc;

    cfg_handle    handle;

    VERB("Synchronize TA '%s' subtree '%s'", ta, oid);

    rc = rcf_ta_cfg_group(ta, 0, TRUE);
    if (rc != 0)
    {
        ERROR("Failed(0x%x) to start group on TA '%s'", rc, ta);
        EXIT("%d", rc);
        return rc;
    }

    /* Take all instances from the TA */
    cfg_get_buf[0] = 0;
    while (TRUE)
    {
        rc = rcf_ta_cfg_get(ta, 0, "*:*", cfg_get_buf, cfg_get_buf_len);
        if (TE_RC_GET_ERROR(rc) == ETESMALLBUF)
        {
            cfg_get_buf_len <<= 1;

            cfg_get_buf = (char *)realloc(cfg_get_buf, cfg_get_buf_len);
            if (cfg_get_buf == NULL)
            {
                ERROR("Memory allocation failure");
                rcf_ta_cfg_group(ta, 0, FALSE);
                return ENOMEM;
            }
        }
        else if (TE_RC_GET_ERROR(rc) == ENOENT || rc == 0)
            break;
        else
        {
            ERROR("rcf_ta_cfg_get() failed: TA=%s, error=%x", ta, rc);
            rcf_ta_cfg_group(ta, 0, FALSE);
            return rc;
        }
    }

    /*
     * At first, remove all instances of the subtree, which do not present
     * in the list.
     */
    rc = cfg_db_find(oid, &handle);
    if (rc != 0 && rc != ENOENT)
    {
        rcf_ta_cfg_group(ta, 0, FALSE);
        return rc;
    }

    if ((list = strdup(cfg_get_buf)) == NULL)
    {
        ERROR("Memory allocation failure");
        rcf_ta_cfg_group(ta, 0, FALSE);
        return ENOMEM;
    }

    if (rc == 0)
        remove_excessive(CFG_GET_INST(handle), list);

    limit = list + strlen(list);

    /* Calculate number of OIDs to be synchronized */
    for (tmp = list; tmp < limit; tmp = next)
    {
        next = strchr(tmp, ' ');
        if (next != NULL)
            *next++ = 0;
        else
            next = limit;
        if (strncmp(tmp, oid, strlen(oid)) == 0)
            n++;
    }

    if ((sorted = (char **)calloc(sizeof(char **) * n, 1)) == NULL)
    {
        free(list);
        ERROR("Memory allocation failure");
        rcf_ta_cfg_group(ta, 0, FALSE);
        return ENOMEM;
    }

    /* Put matching OIDs to the array */
    for (n = 0, tmp = list; tmp < limit; tmp += strlen(tmp) + 1)
        if (strncmp(tmp, oid, strlen(oid)) == 0)
            sorted[n++] = tmp;
    /*
     * Attention! Temporary solution: assuming that RCF PCH returns
     * OIDs in the order "closest to the root last".
     */
    for (n--; n >= 0; n--)
        if ((rc = sync_ta_instance(ta, sorted[n])) != 0)
            break;

    free(list);
    free(sorted);
    rcf_ta_cfg_group(ta, 0, FALSE);

    return rc;
}

/**
 * Synchronize object instances tree with Test Agents.
 *
 * @param oid           identifier of the object instance or subtree
 *                      or NULL if whole database should be synchronized
 * @param subtree       1 if the subtree of the specified node should
 *                      be synchronized
 *
 * @return status code (see te_errno.h)
 */
int
cfg_ta_sync(char *oid, te_bool subtree)
{
    cfg_oid *tmp_oid;
    int      rc = 0;

    tmp_oid = cfg_convert_oid_str(oid);

    if (tmp_oid == NULL)
        return EINVAL;

    if (!tmp_oid->inst)
    {
        cfg_free_oid(tmp_oid);
        return EINVAL;
    }

    if (tmp_oid->len == 1)
    {
        char *ta;

        for (ta = cfg_ta_list;
             ta < cfg_ta_list + ta_list_size;
             ta += strlen(ta) + 1)
        {
            char agent_oid[CFG_SUBID_MAX + CFG_INST_NAME_MAX + 3];

            sprintf(agent_oid, "/agent:%s", ta);
            if ((rc = sync_ta_subtree(ta, agent_oid)) != 0)
                break;
        }
    }
    else
    {
        char *ta = ((cfg_inst_subid *)(tmp_oid->ids))[1].name;

        if (strcmp(((cfg_inst_subid *)(tmp_oid->ids))[1].subid, "agent") == 0)
            rc = subtree ? sync_ta_subtree(ta, oid) : sync_ta_instance(ta, oid);
    }
    cfg_free_oid(tmp_oid);

    return rc;
}

/**
 * Commit local changes in Configurator database to Test Agent.
 *
 * @param ta    - Test Agent name
 * @param inst  - object instance
 *
 * @return status code (see te_errno.h)
 */
static int
cfg_ta_commit_instance(const char *ta, cfg_instance *inst)
{
    int             rc;
    cfg_object     *obj = inst->obj;
    cfg_inst_val    val;
    char           *val_str;

    
    ENTRY("ta=%s inst=0x%x", ta, inst);
    VERB("Commit to '%s' instance '%s'", ta, inst->oid);
    if ((obj->type == CVT_NONE) || (obj->access != CFG_READ_WRITE &&
                                    obj->access != CFG_READ_CREATE))
    {
        VERB("Skip object with type %d(%d) and access %d(%d,%d)",
                obj->type, CVT_NONE,
                obj->access, CFG_READ_WRITE, CFG_READ_CREATE);
        EXIT("0");
        return 0;
    }

    /* Get value from Configurator DB */
    rc = cfg_db_get(inst->handle, &val);
    if (rc != 0)
    {
        ERROR("Failed to get object instance '%s' value", inst->oid);
        EXIT("%d", rc);
        return rc;
    }

    /* Convert got value to string */
    rc = cfg_types[obj->type].val2str(val, &val_str);
    /* Free memory allocated for value in any case */
    cfg_types[obj->type].free(val);
    /* Check conversion return code */
    if (rc != 0)
    {
        VERB("Failed to convert object instance '%s' value of type "
                "%d to string", inst->oid, obj->type);
        EXIT("%d", rc);
        return rc;
    }

    rc = rcf_ta_cfg_set(ta, 0, inst->oid, val_str);
    if (rc != 0)
    {
        ERROR("Failed to set '%s' to value '%s' via RCF",
                inst->oid, val_str);
    }
    free(val_str);

    EXIT("%d", rc);
    return rc;
}

/**
 * Commit changes in local Configurator database to the Test Agent.
 *
 * @param ta    - Test Agent name
 * @param inst  - object instance of the commit subtree root
 *
 * @return status code (see te_errno.h)
 */
static int
cfg_ta_commit(const char *ta, cfg_instance *inst)
{
    int           rc, ret = 0;
    cfg_instance *commit_root;
    cfg_instance *p;
    te_bool       forward;


    assert(ta != NULL);
    assert(inst != NULL);

    ENTRY("ta=%s inst=0x%x", ta, inst);
    VERB("Commit to TA '%s' start at '%s'", ta, inst->oid);

    rc = rcf_ta_cfg_group(ta, 0, TRUE);
    if (rc != 0)
    {
        ERROR("Failed(0x%x) to start group on TA '%s'", rc, ta);
        EXIT("%d", rc);
        return rc;
    }

    for (commit_root = inst, p = inst, forward = TRUE; p != NULL; )
    {
        if (forward)
        {
            rc = cfg_ta_commit_instance(ta, p);
            if (rc != 0)
            {
                ERROR("Failed(0x%x) to commit '%s'", rc, p->oid);
                ret = rc;
                break;
            }
        }

        if (forward && (p->son != NULL))
        {
            /* At first go to all children */
            p = p->son;
        }
        else if (p->brother != NULL)
        {
            /* There are no children - go to brother */
            p = p->brother;
            forward = TRUE;
        }
        else if ((p != commit_root) && (p->father != commit_root))
        {
            p = p->father;
            assert(p != NULL);
            forward = FALSE;
        }
        else
        {
            p = NULL;
        }
    }

    rc = rcf_ta_cfg_group(ta, 0, FALSE);
    if (rc != 0)
    {
        ERROR("Failed(0x%x) to end group on TA '%s'", rc, ta);
        if (ret == 0)
            ret = rc;
    }

    VERB("Commit to TA '%s' end %d - %s", ta, ret,
            (ret == 0) ? "success" : "failed");

    EXIT("%d", ret);
    return ret;
}


/**
 * Commit changes in local Configurator database to all Test Agents.
 *
 * @param oid   - subtree OID or NULL if whole database should be
 *                synchronized
 *
 * @return status code (see te_errno.h)
 */
int
cfg_tas_commit(const char *oid)
{
    int             rc = 0;
    cfg_instance   *inst;


    ENTRY("oid=%s", (oid == NULL) ? "(null)" : oid);
    if (oid == NULL)
    {
        VERB("Commit all configuration tree");
        /* OID is unspecified - commit all Configurator DB */
        for (inst = cfg_inst_root.son;
             (inst != NULL) && (rc == 0);
             inst = inst->brother)
        {
            if (strcmp(inst->obj->subid, "agent") != 0)
            {
                VERB("Skip not TA subtree '%s'", inst->oid);
            }
            else
            {
                rc = cfg_ta_commit(inst->name, inst);
            }
        }
    }
    else
    {
        cfg_instance   *ta_inst;
        cfg_handle      handle;

        VERB("Commit in subtree '%s'", oid);
        rc = cfg_db_find(oid, &handle);
        if (rc != 0)
        {
            ERROR("Failed(0x%x) to find object instance '%s'", rc, oid);
            EXIT("%d", rc);
            return rc;
        }

        inst = CFG_GET_INST(handle);

        if (strncmp(oid, "/agent:", strlen("/agent:")) != 0)
        {
            /* It's not TA subtree */
            VERB("Skip commit in non-TA subtree");
            EXIT("0");
            return 0;
        }

        /* Find TA root instance to get name */
        ta_inst = inst;
        while (strcmp(ta_inst->obj->subid, "agent") != 0)
        {
            ta_inst = ta_inst->father;
            assert(ta_inst != NULL);
        }
        VERB("Found name of TA to commit to: %s", ta_inst->name);

        rc = cfg_ta_commit(ta_inst->name, inst);
    }

    EXIT("%d", rc);
    return rc;
}
