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

#include "conf_defs.h"
#include "rcf_api.h"


#define TA_LIST_SIZE    64
char *cfg_ta_list = NULL;
static size_t ta_list_size = TA_LIST_SIZE;

#define TA_BUF_SIZE     8192
char *cfg_get_buf = NULL;
static int cfg_get_buf_len = TA_BUF_SIZE;


te_bool local_cmd_seq = FALSE;
char max_commit_subtree[CFG_INST_NAME_MAX] = {};
char local_cmd_bkp[1024];

/**
 * Initialize list of Test Agemts.
 *
 * @return status code (see errno.h)
 */
static int
ta_list_init()
{
    const char *ta;
    int         rc;
    
    if ((cfg_get_buf = (char *)malloc(cfg_get_buf_len)) == NULL)
    {
        ERROR("Out of memory");
        return TE_ENOMEM;
    }
        
    while (TRUE)
    {
        if ((cfg_ta_list = (char *)calloc(ta_list_size, 1)) == NULL)
        {
            ERROR("Out of memory");
            return TE_ENOMEM;
        }

        rc = rcf_get_ta_list(cfg_ta_list, &ta_list_size);
        if (rc == 0)
            break;

        free(cfg_ta_list);
        cfg_ta_list = NULL;
        if (TE_RC_GET_ERROR(rc) != TE_ESMALLBUF)
        {
            ERROR("rcf_get_ta_list() returned %r", rc);
            return rc;
        }

        ta_list_size += TA_LIST_SIZE;
    }
    for (ta = cfg_ta_list;
         ta < cfg_ta_list + ta_list_size;
         ta += strlen(ta) + 1)
        if (strlen(ta) >= CFG_INST_NAME_MAX)
        {
            ERROR("Too long Test Agent name");
            return TE_EINVAL;
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
    const char *ta;
    int         rc;
    int         i = 1;
    
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
            ERROR("Out of memory");
            return TE_ENOMEM;
        }
        strcpy(cfg_all_inst[i]->name, ta);
        sprintf(cfg_all_inst[i]->oid, "/agent:%s", ta);
        cfg_all_inst[i]->handle = i | (cfg_inst_seq_num++) << 16;
        cfg_all_inst[i]->obj = cfg_all_obj[1];
        if (i == 1)
            cfg_all_inst[0]->son = cfg_all_inst[i];
        else
            cfg_all_inst[i - 1]->brother = cfg_all_inst[i];
        cfg_all_inst[i]->father = &cfg_inst_root;
    }
    return 0;
}


/**
 * Reboot all Test Agents (before re-initializing of the Configurator).
 */
void
cfg_ta_reboot_all(void)
{
    const char *ta;

    for (ta = cfg_ta_list;
         ta < cfg_ta_list + ta_list_size;
         ta += strlen(ta) + 1)
    {
        rcf_ta_reboot(ta, NULL, NULL);
    }
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
sync_ta_instance(const char *ta, const char *oid)
{
    cfg_object   *obj = cfg_get_object(oid);
    cfg_handle    handle = CFG_HANDLE_INVALID;
    cfg_inst_val  val;
    int           rc;
    
    if (obj == NULL)
        return 0;

    rc = cfg_db_find(oid, &handle);
    if (rc != 0 && TE_RC_GET_ERROR(rc) != TE_ENOENT)
    {
        return rc;
    }

    VERB("Add TA '%s' object instance '%s'", ta, oid);

    if (obj->type == CVT_NONE)
    {
        if (rc != 0)
        {
            /*
             * There is new instance on Test Agent, which we should 
             * put into our local DB.
             */
            rc = cfg_db_add(oid, &handle, CVT_NONE,
                            (cfg_inst_val)0);
            if (rc == 0)
            {
                /* Mark it as synchronized */
                CFG_GET_INST(handle)->added = TRUE;
            }
        }
        return rc;
    }

    while (TRUE)
    {
        rc = rcf_ta_cfg_get(ta, 0, oid, cfg_get_buf, cfg_get_buf_len);
        if (TE_RC_GET_ERROR(rc) == TE_ESMALLBUF)
        {
            cfg_get_buf_len <<= 1;

            cfg_get_buf = (char *)realloc(cfg_get_buf, cfg_get_buf_len);
            if (cfg_get_buf == NULL)
            {
                ERROR("Memory allocation failure");
                return TE_ENOMEM;
            }
        }
        else if (TE_RC_GET_ERROR(rc) == TE_ENOENT || rc == 0 ||
                 (TE_RC_GET_ERROR(rc) == TE_ENOENT && obj->vol))
        {
            break;
        }
        else
        {
            ERROR("Failed(%r) to get '%s' from TA '%s'", rc, oid, ta);
            return rc;
        }
    }

    if (rc != 0)
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
    {
        rc = cfg_db_add(oid, &handle, obj->type, val);
        VERB("%s() cfg_db_add(%s) returns %r, handle %x",
             __FUNCTION__, oid, rc, handle);
    }
    else
    {
        rc = cfg_db_set(handle, val);
        VERB("%s() cfg_db_set() returns %r", __FUNCTION__, rc);
    }

    if (rc == 0)
    {
        /* Mark it as synchronized */
        CFG_GET_INST(handle)->added = TRUE;
    }

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
    
    if (cfg_inst_agent(inst))
        return;

    for (s = strstr(list, inst->oid);
         s != NULL;
         s = strstr(s + 1, inst->oid))
    {
        if (*(s + len) == ' ' || *(s + len) == 0)
            break;
    }
    if (s == NULL)
        cfg_db_del(inst->handle);
}

/** Sorted list element */
typedef struct olist {
    struct olist *next;
    char          oid[RCF_MAX_ID];
} olist;    

/** Insert the entry in lexico-graphical order */
static int
insert_entry(char *oid, olist **list)
{
    olist *cur, *prev, *tmp;
    
    if ((tmp = malloc(sizeof(olist))) == NULL)
        return TE_ENOMEM;
        
    strcpy(tmp->oid, oid);
        
    for (prev = NULL, cur = *list; 
         cur != NULL && strcmp(cur->oid, oid) < 0; 
         prev = cur, cur = cur->next);
         
    tmp->next = cur;
    if (!prev)
        *list = tmp;
    else
        prev->next = tmp;
        
    return 0;
}

/** Free all entries of the list */
static inline void
free_list(olist *list)
{
    olist *tmp;
    
    for (; list != NULL; list = tmp)
    {
        tmp = list->next;
        free(list);
    } 
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
sync_ta_subtree(const char *ta, const char *oid)
{
    char  *tmp;
    char  *next;
    char  *limit;
    char  *wildcard_oid;
    int    rc;
    
    olist *list = NULL, *entry;

    cfg_handle    handle;

    VERB("Synchronize TA '%s' subtree '%s'", ta, oid);

    /* Take all instances from the TA */
    if ((wildcard_oid = malloc(strlen(oid) + sizeof("/..."))) == NULL)
    {
        ERROR("Out of memory");
        return TE_ENOMEM;
    }
    sprintf(wildcard_oid, "%s/...", oid);

    rc = rcf_ta_cfg_group(ta, 0, TRUE);
    if (rc != 0)
    {
        ERROR("rcf_ta_cfg_group() failed");
        free(wildcard_oid);
        return TE_ENOMEM;
    }

    cfg_get_buf[0] = 0;
    while (TRUE)
    {
        rc = rcf_ta_cfg_get(ta, 0, wildcard_oid, cfg_get_buf, 
                            cfg_get_buf_len);
        if (TE_RC_GET_ERROR(rc) == TE_ESMALLBUF)
        {
            cfg_get_buf_len <<= 1;

            cfg_get_buf = (char *)realloc(cfg_get_buf, cfg_get_buf_len);
            if (cfg_get_buf == NULL)
            {
                ERROR("Memory allocation failure");
                rcf_ta_cfg_group(ta, 0, FALSE);
                free(wildcard_oid);
                return TE_ENOMEM;
            }
        }
        else if (rc == 0)
            break;
        else
        {
            ERROR("rcf_ta_cfg_get() failed: TA=%s, error=%r", ta, rc);
            rcf_ta_cfg_group(ta, 0, FALSE);
            free(wildcard_oid);
            return rc;
        }
    }
    free(wildcard_oid);
    
    VERB("%s instances:\n%s", ta, cfg_get_buf);
    
    rc = cfg_db_find(oid, &handle);

    if (rc != 0 && TE_RC_GET_ERROR(rc) != TE_ENOENT)
    {
        rcf_ta_cfg_group(ta, 0, FALSE);
        return rc;
    }

    limit = cfg_get_buf + strlen(cfg_get_buf);
    sprintf(limit, " %s", oid);
    if (rc == 0)
        remove_excessive(CFG_GET_INST(handle), cfg_get_buf);
        
    /* Calculate number of OIDs to be synchronized */
    for (tmp = cfg_get_buf; tmp < limit; tmp = next)
    {
        next = strchr(tmp, ' ');
        if (next != NULL)
            *next++ = 0;
        else
            next = limit;
            
        if ((rc = insert_entry(tmp, &list)) != 0)
        {
            free_list(list);
            return rc;
        }
    }

    for (entry = list; entry != NULL; entry = entry->next)
        if ((rc = sync_ta_instance(ta, entry->oid)) != 0)
            break; 

    rcf_ta_cfg_group(ta, 0, FALSE);
    
    free_list(list);

    return rc;
}

/**
 * Synchronize object instances tree with Test Agents.
 *
 * @param oid           identifier of the object instance or subtree
 * @param subtree       1 if the subtree of the specified node should
 *                      be synchronized
 *
 * @return status code (see te_errno.h)
 */
int
cfg_ta_sync(char *oid, te_bool subtree)
{
    cfg_oid *tmp_oid;
    char    *ta;
    int      rc = 0;

    if ((tmp_oid = cfg_convert_oid_str(oid)) == NULL ||
        !tmp_oid->inst || 
        (tmp_oid->len > 1 && strcmp_start("/agent", oid) != 0))
    {
        cfg_free_oid(tmp_oid);
        return TE_EINVAL;
    }
    
    if (tmp_oid->len == 1 || strcmp_start("/agent:*", oid) == 0)
    {
        for (ta = cfg_ta_list;
             ta < cfg_ta_list + ta_list_size;
             ta += strlen(ta) + 1)
        {
            char agent_oid[CFG_OID_MAX];

            TE_SPRINTF(agent_oid, "/agent:%s%s", ta, 
                       tmp_oid->len == 1 ? "" : 
                       oid + strlen("/agent:*"));
            if ((rc = sync_ta_subtree(ta, agent_oid)) != 0)
                break;
        }
    }
    else
    {
        ta = ((cfg_inst_subid *)(tmp_oid->ids))[1].name;

        rc = subtree ? sync_ta_subtree(ta, oid) :
                       sync_ta_instance(ta, oid);
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
    char           *val_str = NULL;
    
    ENTRY("ta=%s inst=0x%X", ta, inst);
    VERB("Commit to '%s' instance '%s'", ta, inst->oid);
    if ((inst->added && obj->type == CVT_NONE) ||
        (obj->access != CFG_READ_WRITE && obj->access != CFG_READ_CREATE))
    {
        VERB("Skip object with type %d(%d) and access %d(%d,%d)",
                obj->type, CVT_NONE,
                obj->access, CFG_READ_WRITE, CFG_READ_CREATE);
        EXIT("0");
        return 0;
    }

    if (obj->type != CVT_NONE)
    {
        /* Get value from Configurator DB */
        rc = cfg_db_get(inst->handle, &val);
        if (rc != 0)
        {
            ERROR("Failed to get object instance '%s' value", inst->oid);
            EXIT("%r", rc);
            return rc;
        }

        /* Convert got value to string */
        rc = cfg_types[obj->type].val2str(val, &val_str);
        /* Free memory allocated for value in any case */
        cfg_types[obj->type].free(val);
        /* Check conversion return code */
        if (rc != 0)
        {
            VERB("Failed to convert object instance '%s' value of type %d "
                 "to string", inst->oid, obj->type);
            EXIT("%r", rc);
            return rc;
        }
    }

    if (!inst->added && obj->access == CFG_READ_CREATE)
    {
        /*
         * We need to add a new instance to the Test Agent - 
         * postponed add operation.
         */
        if ((rc = rcf_ta_cfg_add(ta, 0, inst->oid, val_str)) != 0)
        {
            ERROR("Cannot add '%s' with value '%s' via RCF, rc = %r",
                  inst->oid, val_str, rc);
        }
    }
    else
    {
        assert(obj->type != CVT_NONE);

        if ((rc = rcf_ta_cfg_set(ta, 0, inst->oid, val_str)) != 0)
        {
            ERROR("Failed to set '%s' to value '%s' via RCF, rc = %r",
                  inst->oid, val_str, rc);
        }
    }

    free(val_str);

    if (rc == 0)
    {
        inst->added = TRUE;
    }

    EXIT("%r", rc);
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
    te_bool       need_sync = FALSE;

    assert(ta != NULL);
    assert(inst != NULL);

    ENTRY("ta=%s inst=0x%X", ta, inst);
    VERB("Commit to TA '%s' start at '%s'", ta, inst->oid);

    rc = rcf_ta_cfg_group(ta, 0, TRUE);
    if (rc != 0)
    {
        ERROR("Failed(%r) to start group on TA '%s'", rc, ta);
        EXIT("%r", rc);
        return rc;
    }

    for (commit_root = inst, p = inst, forward = TRUE; p != NULL; )
    {
        if (forward)
        {
            /*
             * We need to sync subtree in case local add operation
             * is performed.
             */
            if (!p->added && p->obj->access == CFG_READ_CREATE)
                need_sync = TRUE;

            rc = cfg_ta_commit_instance(ta, p);
            if (rc != 0)
            {
                ERROR("Failed(%r) to commit '%s'", rc, p->oid);
                ret = rc;
                break;
            }
        }

        if (forward && (p->son != NULL))
        {
            /* At first go to all children */
            p = p->son;
        }
        else if (p->brother != NULL && p != commit_root)
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
        ERROR("Failed(%r) to end group on TA '%s'", rc, ta);
        if (ret == 0)
            ret = rc;
    }

    if (ret == 0 && need_sync)
    {
        if ((rc = sync_ta_subtree(ta, inst->oid)) != 0)
        {
            ERROR("Failed(%r) to synchronize %s instance", rc, inst->oid);
            if (ret == 0)
                ret = rc;
        }
    }

    VERB("Commit to TA '%s' end %r - %s", ta, ret,
         (ret == 0) ? "success" : "failed");
    
    if (ret != 0 && local_cmd_seq)
    {
        /* Restore configuration before the first local SET/ADD */
        local_cmd_seq = FALSE;
        rc = cfg_dh_restore_backup(local_cmd_bkp, FALSE);
        WARN("Restore backup to configuration which was before "
             "the first local ADD/SET commands restored with code %r", rc);
    }

    EXIT("%r", ret);
    return ret;
}


/**
 * Commit changes in local Configurator database to all Test Agents.
 *
 * @param oid   subtree OID or NULL if whole database should be synchronized
 *
 * @return Status code (see te_errno.h)
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
            if (!cfg_inst_agent(inst))
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
            ERROR("Failed(%r) to find object instance '%s'", rc, oid);
            EXIT("%r", rc);
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

        if (local_cmd_seq && 
            (strlen(max_commit_subtree) < strlen(oid) ||
             strncmp(max_commit_subtree, oid, strlen(oid)) != 0))
        {
            rc = TE_EPERM;
            ERROR("Failed(%r) to commit %s instance - "
                  "configurator allows committing not deeper than %s",
                  rc, oid, max_commit_subtree);
            return rc;
        }

        /* Find TA root instance to get name */
        ta_inst = inst;
        while (ta_inst->father != &cfg_inst_root)
        {
            ta_inst = ta_inst->father;
            assert(ta_inst != NULL);
        }
        VERB("Found name of TA to commit to: %s", ta_inst->name);

        rc = cfg_ta_commit(ta_inst->name, inst);
    }
    
    if (rc == 0 && local_cmd_seq)
    {
        /* Save configuration changes */
        local_cmd_seq = FALSE;
        cfg_dh_release_backup(local_cmd_bkp);
    }

    EXIT("%r", rc);
    return rc;
}
