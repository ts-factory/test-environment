/** @file
 * @brief Linux Test Agent
 *
 * Linux daemons configuring implementation
 *
 *
 * Copyright (C) 2004, 2005 Test Environment authors (see file AUTHORS
 * in the root directory of the distribution).
 *
 * Test Environment is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * Test Environment is distributed in the hope that it will be useful,
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
 * @author Elena A. Vengerova <Elena.Vengerova@oktetlabs.ru>
 *
 * $Id$
 */

#include "linuxconf_daemons.h"

/* Array of daemons/services names */
static struct {
    char   *config_file;
    char   *backup;
    te_bool changed;
} ds[LINUX_SERVICE_MAX];

/* Number of services registered */
static int n_ds = 0;

/** Auxiliary buffer */
static char buf[2048];

/** /etc/hosts backup index */
static int hosts_index;

/** 
 * Get configuration file name for the daemon/service.
 *
 * @param index index returned by the ds_create_backup
 *
 * @return Pathname of the configuration file
 */
const char *
ds_config(int index)
{
    return (index >= n_ds || index < 0) ? "" : ds[index].config_file;
}

/**
 * Look for registered service with specified configuration directory
 * and file name.
 *
 * @param dir   configuration directory name
 * @param name  service name
 *
 * @return index or -1
 */
int 
ds_lookup(const char *dir, const char *name)
{
    int dirlen = strlen(dir);
    int i;
    
    for (i = 0; i < n_ds; i++)
    {
        if (strncmp(dir, ds[i].config_file, dirlen) == 0 &&
            strcmp(name, ds[i].config_file + dirlen) == 0)
        {
            return i;
        }
    }
    
    return -1;
}

/** 
 * Get name of the configuration file name backup for the daemon/service.
 *
 * @param index index returned by the ds_create_backup
 *
 * @return Pathname of the configuration file
 */
const char *
ds_backup(int index)
{
    return (index >= n_ds || index < 0) ? "" : ds[index].backup;
}

/** 
 * Check, if the daemon/service configuration file was changed.
 *
 * @param index index returned by the ds_create_backup
 *
 * @return TRUE, if the file was changed
 */
te_bool 
ds_config_changed(int index)
{
    return (index >= n_ds || index < 0) ? FALSE : ds[index].changed;
}

/** 
 * Notify backup manager that the configuration file was touched.
 *
 * @param index         daemon/service index
 */
void 
ds_config_touch(int index)
{
    if (index < n_ds && index >= 0)
        ds[index].changed = TRUE;
}

/*
 * Creates a copy of service configuration file in TMP directory
 * to restore it after Agent finishes
 *
 * @param dir      configuration directory (with trailing '/')
 * @param name     backup file name
 * @param index    index in the services array
 *
 * @return status code
 */
int
ds_create_backup(char *dir, char *name, int *index)
{
    if (n_ds == sizeof(ds) / sizeof(ds[0]))          
    {                                                              
        WARN("Too many services of xinetd are registered\n");     
        return TE_RC(TE_TA_LINUX, EMFILE);                         
    }
    sprintf(buf, TE_TMP_PATH"%s"TE_TMP_BKP_SUFFIX, name);
    ds[n_ds].backup = strdup(buf);
    sprintf(buf, "%s%s", dir, name);
    ds[n_ds].config_file = strdup(buf);
    ds[n_ds].changed = FALSE; 
    
    if (ds[n_ds].backup == NULL || ds[n_ds].config_file == NULL)
    {
        free(ds[n_ds].config_file);
        free(ds[n_ds].backup);
        return TE_RC(TE_TA_LINUX, ENOMEM);
    }
    
    sprintf(buf, "cp %s %s >/dev/null 2>&1", 
            ds[n_ds].config_file, ds[n_ds].backup);
        
    if (ta_system(buf) != 0)
    {                                                              
        WARN("Cannot create backup file %s", ds[n_ds].backup);
        free(ds[n_ds].config_file);
        free(ds[n_ds].backup);
        return TE_RC(TE_TA_LINUX, ENOENT); 
    }                      
    if (index != NULL)                                        
        *index = n_ds;
    n_ds++;
    return 0;                               
} 

/** Restore initial state of the services */
void
ds_restore_backup()
{
    int i;

    for (i = 0; i < n_ds; i++)
    {
        if (ds[i].changed)
        {
            sprintf(buf, "mv %s %s >/dev/null 2>&1", 
                    ds_backup(i), ds_config(i));
        }
        else
        {
            sprintf(buf, "rm %s >/dev/null 2>&1", ds_backup(i));
        }
        ta_system(buf);
        free(ds[i].backup);
        free(ds[i].config_file);
    }
    
    n_ds = 0;
}

/**
 * Get current state daemon.
 *
 * @param gid   unused
 * @param oid   daemon name
 * @param value value location
 *
 * @return Status code
 */
int
daemon_get(unsigned int gid, const char *oid, char *value)
{
    const char *daemon_name = get_ds_name(oid);

    UNUSED(gid);

    if (daemon_name == NULL)
    {
        return TE_RC(TE_TA_LINUX, ENOENT);
    }
    sprintf(buf, "find /var/run/ -name %s.pid | grep pid >/dev/null 2>&1", 
            daemon_name);
    if (ta_system(buf) == 0)
    {
         sprintf(value, "1");
         return 0;
    }
    sprintf(buf, "killall -CONT %s >/dev/null 2>&1", daemon_name);
    if (ta_system(buf) == 0)
    {
         sprintf(value, "1");
         return 0;
    }
    sprintf(value, "0");

    return 0;
}

/**
 * Get current state daemon.
 *
 * @param gid   unused
 * @param oid   daemon name
 * @param value value location
 *
 * @return Status code
 */
int
daemon_set(unsigned int gid, const char *oid, const char *value)
{
    const char *daemon_name = get_ds_name(oid);
    
    char value0[2];
    int  rc;

    UNUSED(gid);

    if ((rc = daemon_get(gid, oid, value0)) != 0)
        return rc;
    
    if (strlen(value) != 1 || (*value != '0' && *value != '1'))
        return TE_RC(TE_TA_LINUX, EINVAL);

    if (daemon_name == NULL)
    {
        return TE_RC(TE_TA_LINUX, ENOENT);
    }

    if (value0[0] == value[0])
        return 0;
        
    if (strncmp(daemon_name, "exim", strlen("exim")) == 0)
        sprintf(buf, "/etc/init.d/%s* %s >/dev/null 2>&1", daemon_name,
               *value == '0' ? "stop" : "start");
    else               
        sprintf(buf, "/etc/init.d/%s %s >/dev/null 2>&1", daemon_name,
               *value == '0' ? "stop" : "start");

    if (ta_system(buf) != 0)
    {
        ERROR("Command '%s' failed", buf);
        return TE_RC(TE_TA_LINUX, ETESHCMD);
    }
    
    if (value[0] == '0')
    {
        sprintf(buf, "rm `find /var/run/ -name %s.pid` >/dev/null 2>&1", 
                daemon_name);
        ta_system(buf);
    }

    return 0;
}

#ifdef WITH_XINETD

/* Get current state of xinetd service */
static int
xinetd_get(unsigned int gid, const char *oid, char *value)
{
    int   index = ds_lookup(XINETD_ETC_DIR, get_ds_name(oid));
    FILE *f;

    UNUSED(gid);
    
    if (index < 0)
        return TE_RC(TE_TA_LINUX, ENOENT);
        
    if ((f = fopen(ds_config(index), "r")) == NULL)
        return TE_RC(TE_TA_LINUX, errno);

    strcpy(value, "1");
    while (fgets(buf, sizeof(buf), f) != NULL)
    {
        char *tmp = strstr(buf, "disable");
        char *comment = strstr(buf, "#");
        
        if (tmp == NULL || (comment != NULL && comment < tmp))
            continue;
            
        if (strstr(tmp, "yes") != NULL)
        {
            strcpy(value, "0");
            break;
        }
    }
    fclose(f);

    return 0;
}

/* On/off xinetd service */
static int
xinetd_set(unsigned int gid, const char *oid, const char *value)
{
    int   index = ds_lookup(XINETD_ETC_DIR, get_ds_name(oid));
    FILE *f, *g;
    int   rc;
    
    te_bool inside = FALSE;

    UNUSED(gid);

    if (index < 0)
        return TE_RC(TE_TA_LINUX, ENOENT);

    if (strlen(value) != 1 || (*value != '0' && *value != '1'))
        return TE_RC(TE_TA_LINUX, EINVAL);

    if ((f = fopen(ds_backup(index), "r")) == NULL) 
    {
        rc = TE_RC(TE_TA_LINUX, errno);
        ERROR("Cannot open file %s for reading", ds_backup(index));
        return rc;                                            
    }

    if ((g = fopen(ds_config(index), "w")) == NULL) 
    {
        rc = TE_RC(TE_TA_LINUX, errno);
        ERROR("Cannot open file %s for writing", ds_config(index));
        return rc;                                            
    }
    ds_config_touch(index);

    while (fgets(buf, sizeof(buf), f) != NULL)
    {
        if (strstr(buf, "disable") == NULL)
            fwrite(buf, 1, strlen(buf), g);
            
        if (strstr(buf, "{") && !inside)
        {
            inside = TRUE;
            fprintf(g, "disable = %s\n", *value == '0' ? "yes" : "no");
        }
    }
    fclose(f);
    fclose(g);

    ta_system("/etc/init.d/xinetd reload >/dev/null 2>&1");

    return 0;
}

/**
 * Updates "bind" ("interface") attribute of an xinetd service.
 * Attribute "bind" allows a service to be bound to a specific interface
 * on the machine.
 *
 * @param service  xinetd service name (MUST be the same as the service
 *                 configuration file located in /etc/xinetd.d directory)
 * @param value    IP address of interface to which server to be bound
 *
 * @alg This function reads service configuration file located
 * under XINETD_ETC_DIR directory and copies it into temorary file
 * string by string with one exception - strings that include "bind" and
 * "interface" words are not copied.
 * After all it appends "bind" attribute to the end of the temporay file and
 * replace service configuration file with that file.
 */
static int
ds_xinetd_service_addr_set(const char *service, const char *value)
{
    unsigned int  addr;
    char         *DS_path = NULL; /* Path to xinetd service
                                          configuration file */
    char         *tmp_path = NULL; /* Path to temporary file */
    char         *cmd = NULL;
    int           path_len;
    const char   *fmt = "mv %s %s >/dev/null 2>&1";
    FILE         *f;
    FILE         *g;

    if (inet_aton(value, (struct in_addr *)&addr) == 0)
        return EINVAL;

    path_len = ((strlen(TE_TMP_PATH) > strlen(XINETD_ETC_DIR)) ?
                strlen(TE_TMP_PATH) : strlen(XINETD_ETC_DIR)) +
               strlen(service) + strlen(TE_TMP_FILE_SUFFIX) + 1;

#define FREE_BUFFERS \
    do {                     \
        free(DS_path);  \
        free(tmp_path);      \
        free(cmd);           \
    } while (0)

    if ((DS_path = (char *)malloc(path_len)) == NULL ||
        (tmp_path = (char *)malloc(path_len)) == NULL ||
        (cmd = (char *)malloc(2 * path_len + strlen(fmt))) == NULL)
    {
        FREE_BUFFERS;
        return TE_RC(TE_TA_LINUX, ENOMEM);
    }

    snprintf(DS_path, path_len, "%s%s", XINETD_ETC_DIR, service);
    snprintf(tmp_path, path_len, "%s%s%s",
             TE_TMP_PATH, service, TE_TMP_FILE_SUFFIX);
    snprintf(cmd, 2 * path_len + strlen(fmt), fmt, tmp_path, DS_path);

    if ((f = fopen(DS_path, "r")) == NULL)
    {
        FREE_BUFFERS;
        return TE_RC(TE_TA_LINUX, errno);
    }
    if ((g = fopen(tmp_path, "w")) == NULL)
    {
        FREE_BUFFERS;
        fclose(f);
        return 0;
    }

    while (fgets(buf, sizeof(buf), f) != NULL)
    {
        char *comments = strchr(buf, '#');

        if (comments != NULL)
            sprintf(comments, "\n");

        if (strchr(buf, '}') != NULL)
        {
            if (addr != 0xFFFFFFFF)
                fprintf(g, "bind = %s\n}", value);
            else
                fprintf(g, "}");
            break;
        }

        if (strstr(buf, "bind") == NULL &&
            strstr(buf, "interface") == NULL)
        {
            fwrite(buf, 1, strlen(buf), g);
        }
    }
    fclose(f);
    fclose(g);

    /* Update service configuration file */
    ta_system(cmd);
    ta_system("/etc/init.d/xinetd reload >/dev/null 2>&1");

    FREE_BUFFERS;

#undef FREE_BUFFERS

    return 0;
}

/**
 * Gets value of "bind" ("interface") attribute of an xinetd service.
 *
 * @param service  xinetd service name (MUST be the same as the service
 *                 configuration file located in /etc/xinetd.d directory)
 * @param value    Buffer for IP address to return (OUT)
 *
 * @se It is assumed that value buffer is big enough.
 */
static int
ds_xinetd_service_addr_get(const char *service, char *value)
{
    unsigned int  addr;
    int           path_len;
    char         *path;
    FILE         *f;

    path_len = strlen(XINETD_ETC_DIR) + strlen(service) + 1;
    if ((path = (char *)malloc(path_len)) == NULL)
        return TE_RC(TE_TA_LINUX, ENOMEM);

    snprintf(path, path_len, "%s%s", XINETD_ETC_DIR, service);
    if ((f = fopen(path, "r")) == NULL)
    {
        free(path);
        return TE_RC(TE_TA_LINUX, errno);
    }
    free(path);

    while (fgets(buf, sizeof(buf), f) != NULL)
    {
        char *comments = strchr(buf, '#');
        char *tmp;

        if (comments != NULL)
            sprintf(comments, "\n");

        if ((tmp = strstr(buf, "bind")) != NULL ||
            (tmp = strstr(buf, "interface")) != NULL)
        {
            char *val = value;

            while (!isdigit(*tmp))
                tmp++;

            while (isdigit(*tmp) || *tmp == '.')
                *val++ = *tmp++;

            *val = 0;

            if (inet_aton(value, (struct in_addr *)&addr) == 0)
                break;

            fclose(f);
            return 0;
        }
    }
    fclose(f);

    sprintf(value, "255.255.255.255");

    return 0;
}

#endif /* WITH_XINETD */

#ifdef WITH_TFTP_SERVER

static int tfpt_server_index;

/**
 * Get address which TFTP daemon should bind to.
 *
 * @param gid   group identifier (unused)
 * @param oid   full instance identifier (unused)
 * @param value value location
 *
 * @return status code
 */
static int
ds_tftpserver_addr_get(unsigned int gid, const char *oid, char *value)
{
    unsigned int addr;

    FILE *f;

    UNUSED(gid);
    UNUSED(oid);

    if ((f = fopen(ds_config(tftp_server_index), "r")) == NULL)
        return TE_RC(TE_TA_LINUX, errno);

    while (fgets(buf, sizeof(buf), f) != NULL)
    {
        char *comments = strchr(buf, '#');

        if (comments != NULL)
            sprintf(comments, "\n");

        if (strstr(buf, "server_args") != NULL)
        {
            char *tmp;
            char *val = value;

            if ((tmp = strstr(buf, "-a")) == NULL)
                break;

            for (tmp += 2; isspace(*tmp); tmp++);

            while (isdigit(*tmp) || *tmp == '.')
                *val++ = *tmp++;

            *val = 0;

            if (inet_aton(value, (struct in_addr *)&addr) == 0)
                break;

            fclose(f);

            return 0;
        }
    }

    sprintf(value, "255.255.255.255");
    fclose(f);

    return 0;
}

/**
 * Change address which TFTP daemon should bind to.
 *
 * @param gid   group identifier (unused)
 * @param oid   full instance identifier (unused)
 * @param value new address
 *
 * @return status code
 */
static int
ds_tftpserver_addr_set(unsigned int gid, const char *oid, const char *value)
{
    unsigned int addr;
    te_bool      addr_set = FALSE;

    FILE *f;
    FILE *g;

    UNUSED(gid);
    UNUSED(oid);

    if (inet_aton(value, (struct in_addr *)&addr) == 0)
        return TE_RC(TE_TA_LINUX, EINVAL);

    if ((f = fopen("/tmp/tftp.te_backup", "r")) == NULL)
        return TE_RC(TE_TA_LINUX, errno);

    if ((g = fopen("/etc/xinetd.d/tftp", "w")) == NULL)
    {
        fclose(f);
        return 0;
    }

    while (fgets(buf, sizeof(buf), f) != NULL)
    {
        char *comments = strchr(buf, '#');
        char *tmp;

        if (comments != NULL)
            sprintf(comments, "\n");

        if (!addr_set && (tmp = strchr(buf, '}')) != NULL)
        {
            if (addr != 0xFFFFFFFF)
                fprintf(g, "server_args -a %s\n}", value);
            else
                fprintf(g, "}");
            break;
        }

        if (!addr_set && strstr(buf, "server_args") != NULL)
        {
            char *opt;

            addr_set = TRUE;

            if ((opt = strstr(buf, "-a")) != NULL)
            {
                for (opt += 2; isspace(*opt); opt++);

                for (tmp = opt; isdigit(*tmp) || *tmp == '.'; tmp++);

               fwrite(buf, 1, opt - buf, g);
               if (addr != 0xFFFFFFFF)
                   fwrite(value, 1, strlen(value), g);
               fwrite(tmp, 1, strlen(tmp), g);
               continue;
            }
            else if (addr != 0xFFFFFFFF)
            {
                tmp = strchr(buf, '\n');
                if (tmp == NULL)
                    tmp = buf + strlen(buf);
                sprintf(tmp, " -a %s\n",  value);
            }
        }
        fwrite(buf, 1, strlen(buf), g);
    }
    fclose(f);
    fclose(g);

    ta_system("/etc/init.d/xinetd reload >/dev/null 2>&1");

    return 0;
}

/** Get home directory of the TFTP server */
static int
ds_tftpserver_root_get(unsigned int gid, const char *oid, char *value)
{
    UNUSED(gid);
    UNUSED(oid);

    /* @todo Use the same directory as on daemon startup (option -s) */
    strcpy(value, "/tftpboot");

    return 0;
}

/**
 * Parses buf according to the following format:
 * "Month (3 symbol abbreviation) Day Hour:Min:Sec"
 * and updates 'last_tm' with these values.
 *
 * @param buf      Location of the string with date/time value
 * @param last_tm  Location for the time value
 *
 * @return Status of the operation
 *
 * @se Function uses current year as a year stamp of the message, because
 * message does not contain year value.
 */
static int
ds_log_get_timestamp(const char *buf, struct tm *last_tm)
{
    struct tm tm;
    time_t    cur_time;

    if (strptime(buf, "%b %e %T", last_tm) == NULL)
    {
        assert(0);
        return TE_RC(TE_TA_LINUX, EINVAL);
    }

    /*
     * TFTP logs does not containt year stamp, so that we get current
     * local time, and use extracted year for the log message timstamp.
     */

    /* Get current UTC time */
    if ((cur_time = time(NULL)) == ((time_t)-1))
        return TE_RC(TE_TA_LINUX, errno);

    if (gmtime_r(&cur_time, &tm) == NULL)
        return TE_RC(TE_TA_LINUX, EINVAL);

    /* Use current year for the messsage */
    last_tm->tm_year = tm.tm_year;

    return TE_RC(TE_TA_LINUX, EINVAL);
}

/**
 * Extracts parameters (file name and timestamp) of the last successful
 * access to TFTP server
 *
 * @param fname     Location for the file name
 * @param time_val  Location for access time value
 *
 * @return  Status of the operation
 */
static int
ds_tftpserver_last_access_params_get(char *fname, time_t *time_val)
{
    struct tm  last_tm;
    struct tm  prev_tm;
    te_bool    again = FALSE;
    FILE      *f;
    int        last_sess_id = -1; /* TFTP last transaction id */
    int        sess_id;
    char      *prev_fname = NULL;

    if (fname != NULL)
        *fname = 0;

    again:

    if ((f = fopen(again ? "./messages.1.txt" :
                           "./messages.txt", "r")) == NULL)
    {
        return 0;
    }

    memset(&last_tm, 0, sizeof(last_tm));

    while (fgets(buf, sizeof(buf), f) != NULL)
    {
        char *tmp;

        if ((tmp = strstr(buf, "tftpd[")) != NULL)
        {
            if (sscanf(tmp, "tftpd[%d]:", &sess_id) != 1)
                continue;

            if (last_sess_id == sess_id)
            {
                if (strstr(tmp, "NAK") != NULL)
                {
                    if (fname != NULL)
                    {
                        strcpy(fname, prev_fname);
                               free(prev_fname);
                        prev_fname = NULL;
                    }
                    last_tm = prev_tm;
                }
            }
            else
            {
                /* We've found a log message from a new TFTP transaction */
                if ((tmp = strstr(tmp, "filename")) == NULL)
                    continue;

                   if (fname != NULL)
                {
                    char *val = fname;

                    free(prev_fname);
                    /* make a backup of fname of the previous transaction */
                    if ((prev_fname = strdup(fname)) == NULL)
                    {
                        fclose(f);
                        return TE_RC(TE_TA_LINUX, ENOMEM);
                    }

                    for (tmp += strlen("filename"); isspace(*tmp); tmp++);

                    /* Fill in filename value */
                    while (!isspace(*tmp) && *tmp != 0 && *tmp != '\n')
                        *val++ = *tmp++;

                    *val = 0;
                }
                /*
                 * make a backup of access time of the previous
                 * transaction
                 */
                prev_tm = last_tm;

                /*
                 * Update month, day, hour, min, sec apart from the year,
                 * because it is not provided in the log message.
                 */
                ds_log_get_timestamp(buf, &last_tm);

                last_sess_id = sess_id;
            }

            /* Continue the search to find the last record */
        }
    }

    free(prev_fname);
    fclose(f);

    if (fname != NULL && *fname == '\0' && !again)
    {
        again = TRUE;
        goto again;
    }

    if (time_val != NULL)
        *time_val = mktime(&last_tm);

    return 0;
}

/** Get name of the last file obtained via TFTP */
static int
ds_tftpserver_file_get(unsigned int gid, const char *oid, char *value)
{
    UNUSED(gid);
    UNUSED(oid);

    return ds_tftpserver_last_access_params_get(value, NULL);
}

/** Get last time of the TFTP access */
static int
ds_tftpserver_time_get(unsigned int gid, const char *oid, char *value)
{
    int    rc;
    time_t time_val;

    UNUSED(gid);
    UNUSED(oid);

    if ((rc = ds_tftpserver_last_access_params_get(NULL, &time_val)) == 0)
    {
        sprintf(value, "%ld", time_val);
    }
    else
    {
        *value = '\0';
    }
    return TE_RC(TE_TA_LINUX, rc);
}

RCF_PCH_CFG_NODE_RO(node_ds_tftppserver_root_directory, "root_dir",
                    NULL, NULL,
                    ds_tftpserver_root_get);

RCF_PCH_CFG_NODE_RO(node_ds_tftppserver_last_time, "last_time",
                    NULL, &node_ds_tftppserver_root_directory,
                    ds_tftpserver_time_get);

RCF_PCH_CFG_NODE_RO(node_ds_tftppserver_last_fname, "last_fname",
                    NULL, &node_ds_tftppserver_last_time,
                    ds_tftpserver_file_get);

RCF_PCH_CFG_NODE_RW(node_ds_tftppserver_addr, "net_addr",
                    NULL, &node_ds_tftppserver_last_fname,
                    ds_tftpserver_addr_get, ds_tftpserver_addr_set);

RCF_PCH_CFG_NODE_RW(node_ds_tftpserver, "tftpserver",
                    &node_ds_tftppserver_addr, NULL,
                    xinetd_get, xinetd_set);

/** 
 * Patch TFTP server configuration file.
 *
 * @param last  configuration tree node
 */
void
ds_init_tftp_server(rcf_pch_cfg_object **last)
{
    FILE *f = NULL;
    FILE *g = NULL;
    
    if (ds_create_backup(XINETD_ETC_DIR, "tftp", &tftp_index) != 0)
        return;
    
    ds_config_touch(tftp_index);

    /* Set -v option to tftp */
    OPEN_BACKUP(tftp_index, f);
    OPEN_CONFIG(tftp_index, g);

    while (fgets(buf, sizeof(buf), f) != NULL)
    {
        if (strstr(buf, "server_args") != NULL &&
            strstr(buf, "-vv") == NULL)
        {
            char *tmp = strchr(buf, '\n');
            if (tmp == NULL)
                tmp = buf + strlen(buf);
            sprintf(tmp, " -vv\n");
        }
        fwrite(buf, 1, strlen(buf), g);
    }
    fclose(f);
    fclose(g);

    DS_REGISTER(tftpserver);
    
    return 0;
}

#endif /* WITH_TFTP_SERVER */

#ifdef WITH_TODUDP_SERVER

/**
 * Get address which TOD UDP daemon should bind to.
 *
 * @param gid   group identifier (unused)
 * @param oid   full instance identifier (unused)
 * @param value value location
 *
 * @return status code
 */
static int
ds_todudpserver_addr_get(unsigned int gid, const char *oid, char *value)
{
    UNUSED(gid);
    UNUSED(oid);

    return ds_xinetd_service_addr_get("daytime-udp", value);
}

/**
 * Get address which TOD UDP daemon should bind to.
 *
 * @param gid   group identifier (unused)
 * @param oid   full instance identifier (unused)
 * @param value new address
 *
 * @return status code
 */
static int
ds_todudpserver_addr_set(unsigned int gid, const char *oid,
                         const char *value)
{
    UNUSED(gid);
    UNUSED(oid);

    return ds_xinetd_service_addr_set("daytime-udp", value);
}

#endif /* WITH_TODUDP_SERVER */

#ifdef WITH_ECHO_SERVER

/** Get protocol type used by echo server (tcp or udp) */
static int
ds_echoserver_proto_get(unsigned int gid, const char *oid, char *value)
{
    UNUSED(gid);
    UNUSED(oid);
    UNUSED(value);

    return 0;
}

/** Set protocol type used by echo server (tcp or udp) */
static int
ds_echoserver_proto_set(unsigned int gid, const char *oid,
                        const char *value)
{
    UNUSED(gid);
    UNUSED(oid);
    UNUSED(value);

    return 0;
}

/** Get IPv4 address echo server is attached to */
static int
ds_echoserver_addr_get(unsigned int gid, const char *oid, char *value)
{
    UNUSED(gid);
    UNUSED(oid);

    return ds_xinetd_service_addr_get("echo", value);
}

/** Attach echo server to specified IPv4 address */
static int
ds_echoserver_addr_set(unsigned int gid, const char *oid, const char *value)
{
    UNUSED(gid);
    UNUSED(oid);

    return ds_xinetd_service_addr_set("echo", value);
}

#endif /* WITH_ECHO_SERVER */

#ifdef WITH_FTP_SERVER

#define FTPD_CONF           "vsftpd.conf"

static int ftp_index;

RCF_PCH_CFG_NODE_RW(node_ds_ftpserver, "ftpserver",
                    NULL, NULL,
                    daemon_get, daemon_set);

/**
 * Initialize FTP daemon.
 *
 * @param last  configuration tree node
 */
void
ds_init_ftp_server(rcf_pch_cfg_object **last)
{
    FILE *f = NULL;
    FILE *g = NULL;
    
    char *dir = NULL;

    struct stat stat_buf;

    if (stat("/etc/vsftpd/" FTPD_CONF, &stat_buf) == 0)
        dir = "/etc/vsftpd/";
    else if (stat("/etc/" FTPD_CONF, &stat_buf) == 0)
        dir = "/etc/";
    else
    {
        WARN("Failed to locate VSFTPD configuration file");
        return;
    }
    
    if (ds_create_backup(dir, FTPD_CONF, &ftp_index) != 0)
        return;

    /* Enable anonymous upload for ftp */
    ds_config_touch(ftp_index);
    OPEN_BACKUP(ftp_index, f);
    OPEN_CONFIG(ftp_index, g);

    while (fgets(buf, sizeof(buf), f) != NULL)
    {
        if (strstr(buf, "anonymous_enable") != NULL ||
            strstr(buf, "anon_mkdir_write_enable") != NULL ||
            strstr(buf, "write_enable") != NULL ||
            strstr(buf, "anon_upload_enable") != NULL)
        {
            continue;
        }
        fwrite(buf, 1, strlen(buf), g);
    }
    fprintf(g, "anonymous_enable=YES\n");
    fprintf(g, "anon_mkdir_write_enable=YES\n");
    fprintf(g, "write_enable=YES\n");
    fprintf(g, "anon_upload_enable=YES\n");
    fclose(f);
    fclose(g);
    ta_system("mkdir -p /var/ftp/pub");
    ta_system("chmod o+w /var/ftp/pub");
    if (daemon_running("ftpserver"))
    {
        daemon_set(0, "ftpserver", "0");
        daemon_set(0, "ftpserver", "1");
    }
    DS_REGISTER(ftpserver);
}

/* Restart FTP server, if necesary */
void
ds_shutdown_ftp_server()
{
    ta_system("chmod o-w /var/ftp/pub");
    if (daemon_running("ftpserver"))
    {
        daemon_set(0, "ftpserver", "0");
        daemon_set(0, "ftpserver", "1");
    }
}

#endif /* WITH_FTP_SERVER */

/*--------------------------- SSH daemon ---------------------------------*/

/** 
 * Check if the SSH daemon with specified port is running. 
 *
 * @param port  port in string representation
 *
 * @return pid of the daemon or 0
 */
static uint32_t
sshd_exists(char *port)
{
    FILE *f = popen("ps ax | grep 'sshd -p' | grep -v grep", "r");
    char  line[128];
    int   len = strlen(port);
    
    buf[0] = 0;
    while (fgets(line, sizeof(line), f) != NULL)
    {
        char *tmp = strstr(line, "sshd");
        
        tmp = strstr(tmp, "-p") + 2;
        while (*++tmp == ' ');
        
        if (strncmp(tmp, port, len) == 0 && !isdigit(*(tmp + len)))
        {
            fclose(f);
            return atoi(line);
        }
    }
    
    fclose(f);

    return 0;
}

/**
 * Add a new SSH daemon with specified port.
 *
 * @param gid           group identifier (unused)
 * @param oid           full object instence identifier (unused)
 * @param value         unused
 * @param addr          SSHD port
 *
 * @return error code
 */
static int
ds_sshd_add(unsigned int gid, const char *oid, const char *value,
            const char *port)
{
    uint32_t pid = sshd_exists((char *)port);
    uint32_t p;
    char    *tmp;

    UNUSED(gid);
    UNUSED(oid);
    UNUSED(value);
    
    p = strtol(port, &tmp, 10);
    if (tmp == port || *tmp != 0)
        return TE_RC(TE_TA_LINUX, EINVAL);
    
    if (pid != 0)
        return TE_RC(TE_TA_LINUX, EEXIST);
        
    sprintf(buf, "/usr/sbin/sshd -p %s", port);

    if (ta_system(buf) != 0)
    {
        ERROR("Command '%s' failed", buf);
        return TE_RC(TE_TA_LINUX, ETESHCMD);
    }
    
    return 0;
}

/**
 * Stop SSHD with specified port.
 *
 * @param gid           group identifier (unused)
 * @param oid           full object instence identifier (unused)
 * @param addr          
 *
 * @return error code
 */
static int
ds_sshd_del(unsigned int gid, const char *oid, const char *port)
{
    uint32_t pid = sshd_exists((char *)port);

    UNUSED(gid);
    UNUSED(oid);

    if (pid == 0)
        return TE_RC(TE_TA_LINUX, ENOENT);
        
    if (kill(pid, SIGTERM) != 0)
    {
        int kill_errno = errno;
        ERROR("Failed to send SIGTERM "
              "to process SSH daemon with PID=%u: %X",
              pid, kill_errno);
        /* Just to make sure */
        kill(pid, SIGKILL);
    }
    
    return 0;
}

/**
 * Return list of SSH daemons.
 *
 * @param gid           group identifier (unused)
 * @param oid           full object instence identifier (unused)
 * @param list          location for the list pointer
 *
 * @return error code
 */
static int
ds_sshd_list(unsigned int gid, const char *oid, char **list)
{
    FILE *f = popen("ps ax | grep 'sshd -p' | grep -v grep", "r");
    char  line[128];
    char *s = buf;

    UNUSED(gid);
    UNUSED(oid);
    
    buf[0] = 0;
    while (fgets(line, sizeof(line), f) != NULL)
    {
        char *tmp = strstr(line, "sshd");
        
        tmp = strstr(tmp, "-p") + 2;
        while (*++tmp == ' ');
        
        s += sprintf(s, "%u ", atoi(tmp));
    }
    
    fclose(f);

    if ((*list = strdup(buf)) == NULL)
        return TE_RC(TE_TA_LINUX, ENOMEM);
    
    return 0;
}

/*--------------------------- X server ---------------------------------*/

/** 
 * Check if the Xvfb daemon with specified display number is running. 
 *
 * @param number  display number
 *
 * @return pid of the daemon or 0
 */
static uint32_t
xvfb_exists(char *number)
{
    FILE *f = popen("ps ax | grep 'Xvfb' | grep -v grep", "r");
    char  line[128];
    int   len = strlen(number);
    
    buf[0] = 0;
    while (fgets(line, sizeof(line), f) != NULL)
    {
        char *tmp = strstr(line, "Xvfb");

        if ((tmp  = strstr(tmp, ":")) == NULL)
            continue;
        
        tmp++;
        
        if (strncmp(tmp, number, len) == 0 && !isdigit(*(tmp + len)))
        {
            fclose(f);
            return atoi(line);
        }
    }
    
    fclose(f);

    return 0;
}

/**
 * Add a new Xvfb daemon with specified display number.
 *
 * @param gid           group identifier (unused)
 * @param oid           full object instence identifier (unused)
 * @param value         unused
 * @param number        display number
 *
 * @return error code
 */
static int
ds_xvfb_add(unsigned int gid, const char *oid, const char *value,
            const char *number)
{
    uint32_t pid = xvfb_exists((char *)number);
    uint32_t n;
    char    *tmp;
    
    UNUSED(gid);
    UNUSED(oid);
    UNUSED(value);
    
    n = strtol(number, &tmp, 10);
    if (tmp == number || *tmp != 0)
        return TE_RC(TE_TA_LINUX, EINVAL);
    
    if (pid != 0)
        return TE_RC(TE_TA_LINUX, EEXIST);
        
    sprintf(buf, "Xvfb :%s -ac &", number);

    if (ta_system(buf) != 0)
    {
        ERROR("Command '%s' failed", buf);
        return TE_RC(TE_TA_LINUX, ETESHCMD);
    }
    
    return 0;
}

/**
 * Stop Xvfb with specified display number.
 *
 * @param gid           group identifier (unused)
 * @param oid           full object instence identifier (unused)
 * @param number        display number          
 *
 * @return error code
 */
static int
ds_xvfb_del(unsigned int gid, const char *oid, const char *number)
{
    uint32_t pid = xvfb_exists((char *)number);

    UNUSED(gid);
    UNUSED(oid);

    if (pid == 0)
        return TE_RC(TE_TA_LINUX, ENOENT);
        
    if (kill(pid, SIGTERM) != 0)
    {
        int kill_errno = errno;
        ERROR("Failed to send SIGTERM "
              "to process SSH daemon with PID=%u: %X",
              pid, kill_errno);
        /* Just to make sure */
        kill(pid, SIGKILL);
    }
    
    return 0;
}

/**
 * Return list of Xvfb servers.
 *
 * @param gid    group identifier (unused)
 * @param oid    full object instence identifier (unused)
 * @param list   location for the list pointer
 *
 * @return error code
 */
static int
ds_xvfb_list(unsigned int gid, const char *oid, char **list)
{
    FILE *f = popen("ps ax | grep 'Xvfb' | grep -v grep", "r");
    char  line[128];
    char *s = buf;

    UNUSED(gid);
    UNUSED(oid);
    
    buf[0] = 0;
    while (fgets(line, sizeof(line), f) != NULL)
    {
        char *tmp = strstr(line, "Xvfb");
        int   n;

        if ((tmp  = strstr(tmp, ":")) == NULL || (n = atoi(tmp + 1)) == 0)
            continue;
        
        s += sprintf(s, "%u ", n);
    }
    
    fclose(f);
    
    if ((*list = strdup(buf)) == NULL)
        return TE_RC(TE_TA_LINUX, ENOMEM);
    
    return 0;
}

#ifdef WITH_VNCSERVER

/** Read VNC password */
static int
ds_vncpasswd_get(unsigned int gid, const char *oid, char *value)
{
    FILE *f;
    int   rc;

    UNUSED(gid);
    UNUSED(oid);
    
    if ((f = fopen("/tmp/.vnc/passwd", "r")) == NULL)
    {
        rc = errno;
        ERROR("Failed to create /tmp/.vnc directory");
        return TE_RC(TE_TA_LINUX, errno);
    }
    
    memset(value, 0, RCF_MAX_VAL);
    fread(value, 1, RCF_MAX_VAL - 1, f);
    fclose(f);
    
    return 0;
}

/** 
 * Check if the VNC server with specified display is running. 
 *
 * @param number  display number
 *
 * @return TRUE, if the server exists
 */
static te_bool
vncserver_exists(char *number)
{
    sprintf(buf, "ls /tmp/.vnc/*.pid | grep %s >/dev/null 2>&1", number);
    return system(buf) == 0;
}

/**
 * Add a new VNC server with specified display number.
 *
 * @param gid           group identifier (unused)
 * @param oid           full object instence identifier (unused)
 * @param value         unused
 * @param number        display number
 *
 * @return error code
 */
static int
ds_vncserver_add(unsigned int gid, const char *oid, const char *value,
                 const char *number)
{
    uint32_t n;
    char    *tmp;
    
    UNUSED(gid);
    UNUSED(oid);
    UNUSED(value);
    
    n = strtol(number, &tmp, 10);
    if (tmp == number || *tmp != 0)
        return TE_RC(TE_TA_LINUX, EINVAL);
    
    if (vncserver_exists((char *)number))
        return TE_RC(TE_TA_LINUX, EEXIST);
        
    sprintf(buf, "HOME=/tmp vncserver :%s", number);

    if (ta_system(buf) != 0)
    {
        ERROR("Command '%s' failed", buf);
        return TE_RC(TE_TA_LINUX, ETESHCMD);
    }

    sprintf(buf, "DISPLAY=:%s xhost +", number);

    if (ta_system(buf) != 0)
    {
        ERROR("Command '%s' failed", buf);
        sprintf(buf, "vncserver -kill :%s", number);
        system(buf);
        return TE_RC(TE_TA_LINUX, ETESHCMD);
    }
    
    return 0;
}

/**
 * Stop VNC server with specified display number.
 *
 * @param gid           group identifier (unused)
 * @param oid           full object instence identifier (unused)
 * @param number        display number          
 *
 * @return error code
 */
static int
ds_vncserver_del(unsigned int gid, const char *oid, const char *number)
{
    UNUSED(gid);
    UNUSED(oid);

    if (!vncserver_exists((char *)number))
        return TE_RC(TE_TA_LINUX, ENOENT);
        
    sprintf(buf, "HOME=/tmp vncserver -kill :%s", number);

    if (ta_system(buf) != 0)
    {
        ERROR("Command '%s' failed", buf);
        return TE_RC(TE_TA_LINUX, ETESHCMD);
    }
    
    return 0;
}

/**
 * Return list of VNC servers.
 *
 * @param gid    group identifier (unused)
 * @param oid    full object instence identifier (unused)
 * @param list   location for the list pointer
 *
 * @return error code
 */
static int
ds_vncserver_list(unsigned int gid, const char *oid, char **list)
{
    FILE *f = popen("ls /tmp/.vnc/*.pid 2>/dev/null", "r");
    char  line[128];
    char *s = buf;

    UNUSED(gid);
    UNUSED(oid);
    
    buf[0] = 0;
    while (fgets(line, sizeof(line), f) != NULL)
    {
        char *tmp;
        int   n;

        if ((tmp  = strstr(tmp, ":")) == NULL || (n = atoi(tmp + 1)) == 0)
            continue;
        
        s += sprintf(s, "%u ", n);
    }
    
    fclose(f);
    
    if ((*list = strdup(buf)) == NULL)
        return TE_RC(TE_TA_LINUX, ENOMEM);
    
    return 0;
}

RCF_PCH_CFG_NODE_RW(node_ds_vncpasswd, "vncpasswd",
                    NULL, NULL, ds_vncpasswd_get, NULL);

RCF_PCH_CFG_NODE_COLLECTION(node_ds_vncserver, "vncserver",
                            &node_ds_vncpasswd, NULL, 
                            ds_vncserver_add, ds_vncserver_del, 
                            ds_vncserver_list, NULL);

/**
 * Initialize VNC password file.
 *
 * @param last  configuration tree node
 */
void
ds_init_vncserver(rcf_pch_cfg_object **last)
{
    struct stat st;
    int         fd;
    
    /* Real fake password generated during first vncserver spawning */
    uint8_t passwd[] = { 0xE0, 0xFD, 0x04, 0x72, 0x49, 0x29, 0x35, 0xDA };
    
    if (stat("/tmp/.vnc", &st) == 0)
    {
        DS_REGISTER(vncserver);
        return; /* Already exists, do nothing */
    }
        
    if (mkdir("/tmp/.vnc", 0751) < 0)
    {
        WARN("Failed to create /tmp/.vnc directory");
        return;
    } 
    
    if ((fd = open("/tmp/.vnc/passwd", O_CREAT, 0600)) <= 0)
    {
        WARN("Failed to create file /tmp/.vnc/passwd");
        return;
    }
    
    if (write(fd, passwd, sizeof(passwd)) < 0)
    {
        WARN("write() failed for the file /tmp/.vnc/passwd");
        return;
    }
    
    if (close(fd) < 0)
    {
        WARN("close() failed for the file /tmp/.vnc/passwd");
        return;
    }

    DS_REGISTER(vncserver);
}

#endif /* WITH_VNCSERVER */

#ifdef WITH_SMTP

/** sendmail configuration location */
#define SENDMAIL_CONF_DIR   "/etc/mail/"

#define SMTP_EMPTY_SMARTHOST    "0.0.0.0"

/* Possible kinds of SMTP servers */
static char *smtp_servers[] = {
    "sendmail",
    "exim",
    "exim3",
    "exim4",
    "postfix"
};    

static int sendmail_index = -1;

static char *smtp_initial;
static char *smtp_current;
static char *smtp_current_smarthost;

/** 
 * Update /etc/hosts with entry te_tester <IP>. 
 *
 * @return status code
 */
static int
update_etc_hosts(char *ip)
{
    FILE *f = NULL;
    FILE *g = NULL;
    int   rc;
    
    if (strcmp(ip, SMTP_EMPTY_SMARTHOST) == 0)
        return 0;

    if ((f = fopen(ds_backup(hosts_index), "r")) == NULL) 
    {
        rc = TE_RC(TE_TA_LINUX, errno);
        ERROR("Cannot open file %s for reading", ds_backup(hosts_index));
        return rc;                                            
    }

    if ((g = fopen(ds_config(hosts_index), "w")) == NULL) 
    {
        rc = TE_RC(TE_TA_LINUX, errno);
        ERROR("Cannot open file %s for writing", ds_config(hosts_index));
        return rc;                                            
    }
    ds_config_touch(hosts_index);

    while (fgets(buf, sizeof(buf), f) != NULL)
    {
        if (strstr(buf, "te_tester") == NULL)
            fwrite(buf, 1, strlen(buf), g);
    }
    fprintf(g, "%s te_tester", ip);
    fclose(f);
    fclose(g);
    
    return 0;
}

#define SENDMAIL_SMARTHOST_OPT  "define(`SMART_HOST',`te_tester')\n"

/** Check if smarthost option presents in the sendmail configuration file */
static int
sendmail_smarthost_get(te_bool *enable)
{
    FILE *f;
    int   rc;

    if ((f = fopen(ds_config(sendmail_index), "r")) == NULL)
    {
        rc = TE_RC(TE_TA_LINUX, errno);
        ERROR("Cannot open file %s for reading",
              ds_config(sendmail_index));
        fclose(f);
        return rc;
    }

    while (fgets(buf, sizeof(buf), f) != NULL)
    {
        if (strncmp(buf, SENDMAIL_SMARTHOST_OPT, 
                    strlen(SENDMAIL_SMARTHOST_OPT)) == 0)
        {
            fclose(f);
            *enable = 1;
            return 0;
        }
    }
    *enable = 0;
    fclose(f);
    return 0;
}

/** Enable/disable smarthost option in the sendmail configuration file */
static int
sendmail_smarthost_set(te_bool enable)
{
    FILE *f = NULL;
    FILE *g = NULL;
    int   rc;
    
    if (sendmail_index < 0)
    {
        ERROR("Cannot find sendmail configuration file");
        return ENOENT;
    }
    
    ds_config_touch(sendmail_index);
    if ((f = fopen(ds_backup(sendmail_index), "r")) == NULL) 
    {
        rc = TE_RC(TE_TA_LINUX, errno);
        ERROR("Cannot open file %s for reading", ds_backup(sendmail_index));
        return rc;                                            
    }

    if ((g = fopen(ds_config(sendmail_index), "w")) == NULL) 
    {
        rc = TE_RC(TE_TA_LINUX, errno);
        ERROR("Cannot open file %s for writing", ds_config(sendmail_index));
        return rc;                                            
    }

    while (fgets(buf, sizeof(buf), f) != NULL)
    {
        if (strstr(buf, "SMARTHOST") == NULL)
            fwrite(buf, 1, strlen(buf), g);
    }
    if (enable != 0)
        fwrite(SENDMAIL_SMARTHOST_OPT, 1, 
               strlen(SENDMAIL_SMARTHOST_OPT), g);
    fclose(f);
    fclose(g);
    
    ta_system("cd " SENDMAIL_CONF_DIR "; make");
    
    return 0;
}

/** Get SMTP smarthost */
static int
ds_smtp_smarthost_get(unsigned int gid, const char *oid, char *value)
{
    UNUSED(gid);
    UNUSED(oid);

    strcpy(value, SMTP_EMPTY_SMARTHOST);
    if (smtp_current == NULL)
        return 0;
        
    if (strcmp(smtp_current, "sendmail") == 0)
    {
        te_bool enable;
        int     rc;
        
        if ((rc = sendmail_smarthost_get(&enable)) != 0)
            return rc;
            
        if (enable)
            strcpy(value, smtp_current_smarthost);
    }
    
    return 0;
}

/** Set SMTP smarthost */
static int
ds_smtp_smarthost_set(unsigned int gid, const char *oid,
                      const char *value)
{
    uint32_t addr;
    char    *new_host = NULL;
    int      rc;
    
    UNUSED(gid);
    UNUSED(oid);
    
    if (inet_pton(AF_INET, value, (void *)&addr) <= 0)
        return TE_RC(TE_TA_LINUX, EINVAL);
    
    if (smtp_current == NULL)
        return TE_RC(TE_TA_LINUX, EPERM);

    if ((new_host = strdup(value)) == NULL)
        return TE_RC(TE_TA_LINUX, ENOMEM);
        
    if ((rc = update_etc_hosts(new_host)) != 0)
    {
         free(new_host);
         return rc;
    }
    
    if (strcmp(smtp_current, "sendmail") == 0)
    {
        if ((rc = sendmail_smarthost_set(addr != 0)) != 0)
            goto error;
    }
    else
        goto error;
        
    free(smtp_current_smarthost);
    smtp_current_smarthost = new_host;

    return 0;
    
error:
    update_etc_hosts(smtp_current_smarthost);
    free(new_host);    
    return rc;
}

/** Get SMTP server program */
static int
ds_smtp_server_get(unsigned int gid, const char *oid, char *value)
{
    UNUSED(gid);
    UNUSED(oid);
    
    if (smtp_current == NULL)
        value[0] = 0;
    else
        strcpy(value, smtp_current);

    return 0;
}

/** Set SMTP server program */
static int
ds_smtp_server_set(unsigned int gid, const char *oid, const char *value)
{
    unsigned int i;
    
    UNUSED(gid);
    UNUSED(oid);
    
    if (smtp_current != NULL && daemon_running(smtp_current))
        return TE_RC(TE_TA_LINUX, EPERM);

    for (i = 0; i < sizeof(smtp_servers) / sizeof(char *); i++)
    {
        if (strcmp(smtp_servers[i], value) == 0)
        {
            if (smtp_current == NULL && 
                strcmp(smtp_servers[i], "sendmail") == 0)
            {
                int rc = sendmail_smarthost_set(FALSE);
                
                if (rc != 0)
                    return rc;
            }
            smtp_current = smtp_servers[i];
            return 0;
        }
    }
    
    return TE_RC(TE_TA_LINUX, EINVAL);
}

/** Check if SMTP server is enabled */
static int
ds_smtp_get(unsigned int gid, const char *oid, char *value)
{
    UNUSED(oid);
    if (smtp_current == NULL)
    {
        value[0] = 0;
        return 0;
    }
    return daemon_get(gid, smtp_current, value);
}

/** Enable/disable SMTP server */
static int
ds_smtp_set(unsigned int gid, const char *oid, const char *value)
{
    UNUSED(oid);
    if (smtp_current == NULL)
    {
        if (value[0] == '0')
            return 0;
        else if (value[0] == '1')
            return TE_RC(TE_TA_LINUX, EPERM);
        else
            return TE_RC(TE_TA_LINUX, EINVAL);
    }
    return daemon_set(gid, smtp_current, value);
}

RCF_PCH_CFG_NODE_RW(node_ds_smtp_smarthost, "smarthost",
                    NULL, NULL,
                    ds_smtp_smarthost_get, ds_smtp_smarthost_set);

RCF_PCH_CFG_NODE_RW(node_ds_smtp_server, "server",
                    NULL, &node_ds_smtp_smarthost,
                    ds_smtp_server_get, ds_smtp_server_set);

RCF_PCH_CFG_NODE_RW(node_ds_smtp, "smtp",
                    &node_ds_smtp_server, NULL,
                    ds_smtp_get, ds_smtp_set);

/** 
 * Initialize SMTP-related variables. 
 *
 * @param last  configuration tree node
 */
void
ds_init_smtp(rcf_pch_cfg_object **last)
{
    unsigned int i;
    
    if (file_exists(SENDMAIL_CONF_DIR "sendmail.mc") &&
        ds_create_backup(SENDMAIL_CONF_DIR, "sendmail.mc", 
                         &sendmail_index) != 0)
    {
        return;
    }
        
    smtp_current_smarthost = strdup(SMTP_EMPTY_SMARTHOST);
    for (i = 0; i < sizeof(smtp_servers) / sizeof(char *); i++)
    {
        if (daemon_running(smtp_servers[i]))
        {
            smtp_current = smtp_initial = smtp_servers[i];
            break;
        }
    }
    DS_REGISTER(smtp);
}

/** Restore SMTP */
void
ds_shutdown_smtp()
{
    if (ds_config_changed(sendmail_index))
    {
        if (file_exists(SENDMAIL_CONF_DIR))
            ta_system("cd " SENDMAIL_CONF_DIR "; make");
    }
    if (smtp_current != NULL)
        daemon_set(0, smtp_current, "0");

    if (smtp_initial != NULL)
        daemon_set(0, smtp_initial, "1");

    free(smtp_current_smarthost);        
}

#endif /* WITH_SMTP */

/*
 * Daemons configuration tree in reverse order.
 */

#ifdef WITH_ECHO_SERVER

RCF_PCH_CFG_NODE_RW(node_ds_echoserver_addr, "net_addr",
                    NULL, NULL,
                    ds_echoserver_addr_get, ds_echoserver_addr_set);

RCF_PCH_CFG_NODE_RW(node_ds_echoserver_proto, "proto",
                    NULL, &node_ds_echoserver_addr,
                    ds_echoserver_proto_get, ds_echoserver_proto_set);

RCF_PCH_CFG_NODE_RW(node_ds_echoserver, "echoserver",
                    &node_ds_echoserver_proto, NULL,
                    xinetd_get, xinetd_set);

#endif /* WITH_ECHO_SERVER */

#ifdef WITH_TELNET
RCF_PCH_CFG_NODE_RW(node_ds_telnet, "telnetd",
                    NULL, NULL, xinetd_get, xinetd_set);
#endif /* WITH_TELNET */

#ifdef WITH_RSH
RCF_PCH_CFG_NODE_RW(node_ds_rsh, "rshd",
                    NULL, NULL, xinetd_get, xinetd_set);
#endif /* WITH_RSH */

#ifdef WITH_TODUDP_SERVER

RCF_PCH_CFG_NODE_RW(node_ds_todudpserver_addr, "net_addr",
                    NULL, NULL,
                    ds_todudpserver_addr_get, ds_todudpserver_addr_set);

RCF_PCH_CFG_NODE_RW(node_ds_todudpserver, "todudpserver",
                    &node_ds_todudpserver_addr, NULL,
                    xinetd_get, xinetd_set);

#endif /* WITH_TODUDP_SERVER */

RCF_PCH_CFG_NODE_COLLECTION(node_ds_sshd, "sshd",
                            NULL, NULL, 
                            ds_sshd_add, ds_sshd_del, ds_sshd_list, NULL);

RCF_PCH_CFG_NODE_COLLECTION(node_ds_xvfb, "Xvfb",
                            NULL, NULL, 
                            ds_xvfb_add, ds_xvfb_del, ds_xvfb_list, NULL);

/**
 * Initializes linuxconf_daemons support.
 *
 * @param last  node in configuration tree (last sun of /agent) to be
 *              updated
 *
 * @return status code (see te_errno.h)
 */
int
linuxconf_daemons_init(rcf_pch_cfg_object **last)
{
    if (ds_create_backup("/etc/", "hosts", &hosts_index) != 0)
        return 0;

#ifdef WITH_ECHO_SERVER
    if (ds_create_backup(XINETD_ETC_DIR, "echo", NULL) == 0)
        DS_REGISTER(echoserver);
#endif /* WITH_ECHO_SERVER */

#ifdef WITH_TODUDP_SERVER
    if (ds_create_backup(XINETD_ETC_DIR, "daytime-udp", NULL) == 0)
        DS_REGISTER(todudpserver);
#endif /* WITH_TODUDP_SERVER */

#ifdef WITH_TELNET
    if (ds_create_backup(XINETD_ETC_DIR, "telnet", NULL) == 0)
        DS_REGISTER(telnet);
#endif /* WITH_TELNET */

#ifdef WITH_RSH
    if (ds_create_backup(XINETD_ETC_DIR, "rsh", NULL) == 0)
        DS_REGISTER(rsh);
#endif /* WITH_RSH */

#ifdef WITH_SMTP
    ds_init_smtp(last);
#endif /* WITH_SMTP */

#ifdef WITH_TFTP_SERVER
    ds_init_tftp_server(last);
#endif /* WITH_TFTP_SERVER */

#ifdef WITH_FTP_SERVER
    ds_init_ftp_server(last);
#endif /* WITH_FTP_SERVER */

#ifdef WITH_VNCSERVER
    ds_init_vncserver(last);
#endif

#ifdef WITH_DHCP_SERVER
    ds_init_dhcp_server(last);
#endif /* WITH_DHCP_SERVER */

#ifdef WITH_DNS_SERVER
    ds_init_dns_server(last);
#endif /* WITH_DMS_SERVER */


    DS_REGISTER(sshd);

    DS_REGISTER(xvfb);

    return 0;

#undef CHECK_RC
}


/**
 * Release resources allocated for the configuration support.
 */
void
linux_daemons_release()
{
    ds_restore_backup();

#ifdef WITH_DHCP_SERVER
    ds_shutdown_dhcp_server();
#endif /* WITH_DHCP_SERVER */

#ifdef WITH_FTP_SERVER
    ds_shutdown_ftp_server();
#endif    

#ifdef WITH_SMTP
    ds_shutdown_smtp();
#endif

    ta_system("/etc/init.d/xinetd reload >/dev/null 2>&1");
}

