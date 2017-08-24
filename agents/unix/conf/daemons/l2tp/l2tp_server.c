#include "te_config.h"
#if HAVE_CONFIG_H
#include "config.h"
#endif
#include <fcntl.h>
#include <dirent.h>
#include <ifaddrs.h>
#include "conf_daemons.h"
#include "te_queue.h"
#include "te_string.h"
#include "te_shell_cmd.h"

/** L2TP global section name */
#define L2TP_GLOBAL            "global"

/** L2TP executable file */
#define L2TP_SERVER_EXEC       "/usr/sbin/xl2tpd"

/** Part of the L2TP config file's name */
#define L2TP_SERVER_CONF_BASIS "/tmp/te.xl2tpd.conf"

/** L2TP TA pid file */
#define L2TP_TA_PIDFILE       "/tmp/te.xl2tpd.pid"

/** L2TP pid file */
#define L2TP_SERVER_PIDFILE   "/var/run/xl2tpd/xl2tpd.pid"

/** L2TP script */
#define L2TP_INIT_SCRIPT      "/etc/init.d/xl2tpd"

/** CHAP secrets file */
#define L2TP_CHAP_SECRETS     "/etc/ppp/chap-secrets"

/** PAP secrets file */
#define L2TP_PAP_SECRETS      "/etc/ppp/pap-secrets"

/** Name of the option in L2TP config file */
#define PPP_OPTIONS            "pppoptfile"

/** Marker which isolate the L2TP secrets from others */
#define L2TP_FENCE              "#L2TP secret tests\n"
#define L2TP_FENCE_DESCRIPTION  "#Do not put any records between such lines " \
                                "since they will be removed by test\n"

/** Temporary file to save work copy of secrets */
#define TMP_CHAP_SECRETS_FILE   "/tmp/chap-secrets"
#define TMP_PAP_SECRETS_FILE    "/tmp/pap-secrets"

/**
 * Default amount of memory allocated for list methods
 * of l2tp subtreee
 */
#define L2TP_SERVER_LIST_SIZE 1024

/** Default buffer size for command-line construction */
#define L2TP_CMDLINE_LENGTH   1024

/** Default buffer size for the full CHAP|PAP secret line */
#define L2TP_SECRETS_LENGTH   512

/** Default size for the pid storage */
#define L2TP_MAX_PID_VALUE_LENGTH 16

/** Default buffer size for the option name storage */
#define L2TP_MAX_OPTNAME_LENGTH 40

/** Max length of the IP address in human dot notation */
#define L2TP_IP_ADDR_LEN 15


/** pid of xl2tpd */
static int l2tp_pid = -1;

/** The class of the secret */
enum l2tp_secret_type {
    L2TP_SECRET_TYPE_SEC,    /**< secret type */
    L2TP_SECRET_TYPE_SERV,   /**< server type */
    L2TP_SECRET_TYPE_IPV4,   /**< ipv4 type */
};

/** The class of the options */
enum l2tp_value_type {
    L2TP_VALUE_TYPE_INT,       /**< integer type */
    L2TP_VALUE_TYPE_ADDRESS   /**< address type */
};

/** Authentication type */
enum l2tp_secret_prot {
    L2TP_SECRET_PROT_CHAP,    /**< CHAP authentication */
    L2TP_SECRET_PROT_PAP,     /**< PAP authentication */
    L2TP_SECRET_PROT_NTYPES   /**< Authentication types number */
};

/** Type of the request for the l2tp_secret_prot */
enum l2tp_auth_policy {
    L2TP_AUTH_POLICY_NONE,      /**< Indicate that no policy was added */
    L2TP_AUTH_POLICY_REFUSE,    /**< Refuse CHAP|PAP */
    L2TP_AUTH_POLICY_REQUIRE    /**< Require CHAP|PAP */
};

/** The class of the options */
enum l2tp_option_type {
    L2TP_OPTION_TYPE_PPP,    /**< PPP options class */
    L2TP_OPTION_TYPE_L2TP,   /**< L2TP options class */
};

/** IP range structure */
typedef struct te_l2tp_ipv4_range {
    SLIST_ENTRY(te_l2tp_ipv4_range)  list;
    char                            *start; /**< Start of the range */
    char                            *end;   /**< End of the range */
} te_l2tp_ipv4_range;

/** CHAP|PAP secret structure */
typedef struct te_l2tp_secret {
    SLIST_ENTRY(te_l2tp_secret) list;
    char                       *name;
    char                       *sipv4;  /**< IP address in human dot notation*/
    char                       *server; /**< Server name */
    char                       *secret; /**< Secret name */
} te_l2tp_secret;

/** Options for L2TP */
typedef struct te_l2tp_option {
    SLIST_ENTRY(te_l2tp_option)      list;
    SLIST_HEAD(, te_l2tp_ipv4_range) l2tp_range; /**< Ranges of the
                                                      (no)lac|(no)ip range
                                                      option */
    enum l2tp_option_type            type;       /**< PPP|L2TP options */
    char                            *name;       /**< Option name */
    char                            *value;      /**< Option value */
} te_l2tp_option;

/** Authentication options for L2TP */
typedef struct te_l2tp_opt_auth {
    SLIST_ENTRY(te_l2tp_opt_auth)    list;
    SLIST_HEAD(, te_l2tp_secret)     secret;     /**< CHAP|PAP secret */
    enum l2tp_auth_policy            policy;     /**< refuse or require secrets */
    te_bool                          is_enabled; /**< TRUE(yes) or FALSE(no) */

} te_l2tp_opt_auth;

/** LNS structure */
typedef struct te_l2tp_section {
    SLIST_ENTRY(te_l2tp_section)  list;
    SLIST_HEAD(, te_l2tp_option)  l2tp_option; /**< Options of the section */
    te_l2tp_opt_auth              l2tp_opt_auth[L2TP_SECRET_PROT_NTYPES];
                                  /**< Array of CHAP|PAP authentication options */

    char                         *secname;   /**< Section name */

} te_l2tp_section;

/** Connected clients structrure */
typedef struct te_l2tp_connected {
    SLIST_ENTRY(te_l2tp_connected)  list;  /**< Next element in list */
    char                           *cname; /**< Client name */
} te_l2tp_connected;

/** L2TP server configuration structure */
typedef struct te_l2tp_server {
    SLIST_HEAD(, te_l2tp_section)   section;     /**< Section of the L2TP
                                                      server structure */
    SLIST_HEAD(, te_l2tp_connected) client;      /**< Connected clients to
                                                      the L2TP server */
    te_bool                         initialized; /**< Flag to determine whether
                                                      the structure is initialized */
    te_bool                         started;     /**< Admin status for the
                                                      L2TP server */
    te_bool                         changed;     /**< Configuration changed flag,
                                                      used to detect if
                                                      L2TP-server restart
                                                      is required */
} te_l2tp_server;

static te_bool
l2tp_is_running(te_l2tp_server *l2tp);

/** Static L2TP server structure */
static te_l2tp_server l2tp_server;

/**
 * Initialize L2TP server structure with default values
 *
 * @param l2tp         L2TP server structure to initialize
 *
 */
static void
l2tp_server_init(te_l2tp_server *l2tp)
{
    ENTRY("initialize l2tp server with default configuration");
    SLIST_INIT(&l2tp->section);
    SLIST_INIT(&l2tp->client);
    l2tp->started = l2tp_is_running(l2tp);
    l2tp->changed = l2tp->started;
    l2tp->initialized = TRUE;
}

/**
 * Return pointer to static L2TP server structure
 *
 * @return L2TP server structure
 */
static te_l2tp_server *
l2tp_server_find()
{
    te_l2tp_server *l2tp = &l2tp_server;
    if (!l2tp->initialized)
        l2tp_server_init(l2tp);
    return l2tp;
}

/**
 * Recover the original CHAP|PAP secrets file
 *
 * @param outname     L2TP_CHAP_SECRETS or L2TP_PAP_SECRETS
 *
 * @return Status code
 */
static te_errno
l2tp_secrets_recover(char *secret_fname)
{
    char     backup_fname[] = "/etc/ppp/secretsXXXXXX";
    char     line [L2TP_SECRETS_LENGTH];
    FILE*    secrets_file;

    FILE*    backup_file;
    int      backup_fd;
    te_bool  fence_flag = FALSE;
    int      rc;

    backup_fd = mkstemp(backup_fname);

    if (backup_fd == -1)
    {
        rc = errno;
        ERROR("mkstemp failed: %s", strerror(rc));
        return TE_OS_RC(TE_TA_UNIX, rc);
    }

    secrets_file = fopen(secret_fname, "r");
    if(secrets_file == NULL)
    {
        rc = errno;
        ERROR("Failed to open file: %s", strerror(rc));
        return TE_OS_RC(TE_TA_UNIX, rc);
    }

    backup_file = fdopen(backup_fd, "w");
    if( backup_file == NULL)
    {
        if (fclose(backup_file) != 0)
        {
            rc = errno;
            ERROR("%s(): fclose() failed: %s",
                  __FUNCTION__, strerror(rc));
            return TE_OS_RC(TE_TA_UNIX, rc);
        }
        rc = errno;
        ERROR("Failed to open file: %s", strerror(rc));
        return TE_OS_RC(TE_TA_UNIX, rc);
    }

    while( fgets(line, sizeof(line), secrets_file) != NULL )
    {
        if (strcmp(line, L2TP_FENCE) == 0)
        {
            fence_flag = !fence_flag;
        }
        else if (!fence_flag)
        {
            fputs(line, backup_file);
        }
    }

    if (fclose(backup_file) != 0)
    {
        rc = errno;
        ERROR("%s(): fclose() failed: %s",
              __FUNCTION__, strerror(rc));
        return TE_OS_RC(TE_TA_UNIX, rc);
    }

    if (fclose(secrets_file) != 0)
    {
        rc = errno;
        ERROR("%s(): fclose() failed: %s",
              __FUNCTION__, strerror(rc));
        return TE_OS_RC(TE_TA_UNIX, rc);
    }

    if (rename(backup_fname, secret_fname) != 0)
    {
        rc = errno;
        ERROR("Failed to rename %s to %s: %s",
              backup_fname, secret_fname, strerror(rc));
        return TE_OS_RC(TE_TA_UNIX, rc);
    }
    return 0;
}


/**
 * Save options to configuration file
 *
 * @param l2tp_file     configuration file
 * @param l2tp_option   'lac' or 'ip range' option
 *
 */
static void
l2tp_server_save_ranges(FILE *l2tp_file, te_l2tp_option  *l2tp_option)
{
    te_l2tp_ipv4_range  *l2tp_range;
    te_string            str = TE_STRING_INIT;

    for (l2tp_range = SLIST_FIRST(&l2tp_option->l2tp_range);
         l2tp_range != NULL;
         l2tp_range = SLIST_NEXT(l2tp_range, list))
    {
        if (l2tp_range->end == NULL)
        {
            te_string_append(&str, "%s%s", l2tp_range->start,
                             l2tp_range->list.sle_next !=
                             NULL ? "," : "");
        }
        else
        {
            te_string_append(&str, "%s-%s%s",
                             l2tp_range->start,
                             l2tp_range->end,
                             l2tp_range->list.sle_next !=
                             NULL ? "," : "");
        }
    }

    fprintf(l2tp_file, "%s = %s\n", l2tp_option->name,
            str.ptr);
    te_string_free(&str);
}

/**
 * Save options to configuration file
 *
 * @param l2tp_file     configuration file
 * @param l2tp_section  lns section
 * @param pppfilename   name of ppp options file
 *
 */
static void
l2tp_server_save_opt(FILE *l2tp_file, te_l2tp_section *l2tp_section, char **pppfilename)
{
    te_l2tp_option      *l2tp_option;

    for (l2tp_option = SLIST_FIRST(&l2tp_section->l2tp_option);
         l2tp_option != NULL; l2tp_option = SLIST_NEXT(l2tp_option, list))
    {
        if (l2tp_option->type == L2TP_OPTION_TYPE_L2TP)
        {
            if (strcmp(l2tp_option->name, PPP_OPTIONS) == 0)
            {
                *pppfilename = l2tp_option->value;
            }
            if (strcmp(l2tp_option->name, "ip range") == 0 ||
                    strcmp(l2tp_option->name, "lac") == 0)
            {
                l2tp_server_save_ranges(l2tp_file, l2tp_option);
            }
            else
            {
                fprintf(l2tp_file, "%s = %s\n", l2tp_option->name,
                        l2tp_option->value);
            }
        }
    }
}

/**
 * Save secrets
 *
 * @param l2tp_file        configuration file
 * @param backup_file      File for secrets' storage
 * @param l2tp_opt_auth    option authentication structure
 * @param protocol         CHAP or PAP
 *
 */
static te_errno
l2tp_server_save_secrets_mode(FILE *l2tp_file,
                              FILE *backup_file,
                              te_l2tp_section *l2tp_section,
                              enum l2tp_secret_prot protocol)
{
    te_l2tp_secret        *l2tp_secret;
    te_l2tp_opt_auth      *l2tp_opt_auth = &l2tp_section->l2tp_opt_auth[protocol];

    if (l2tp_opt_auth != NULL && l2tp_opt_auth->policy != L2TP_AUTH_POLICY_NONE)
    {
        fprintf(l2tp_file, "%s %s = %s\n",
                l2tp_opt_auth->policy == L2TP_AUTH_POLICY_REQUIRE ?
                "require" : "refuse",
                protocol == L2TP_SECRET_PROT_CHAP ? "chap" : "pap",
                l2tp_opt_auth->is_enabled ? "yes" : "no");
    }

    else
    {
        return TE_RC(TE_TA_UNIX, TE_EINVAL);
    }

    for (l2tp_secret = SLIST_FIRST(&l2tp_opt_auth->secret);
             l2tp_secret != NULL; l2tp_secret = SLIST_NEXT(l2tp_secret, list))
    {
        if (backup_file != NULL)
        {
            fprintf(backup_file, "%s %s %s %s\n",
                    l2tp_secret->name,
                    l2tp_secret->server,
                    l2tp_secret->secret,
                    l2tp_secret->sipv4);
        }
    }

    return 0;
}

/**
 * Check if there are configured secrets in L2TP server context.
 *
 * @param l2tp          L2TP server context.
 * @param protocol      Authentication protocol.
 *
 * @return @c TRUE if secrets are set, @c FALSE otherwise.
 */
static te_bool
l2tp_secret_is_set(te_l2tp_server *l2tp, enum l2tp_secret_prot protocol)
{
    te_l2tp_section *section;

    SLIST_FOREACH(section, &l2tp->section, list)
    {
        if (!SLIST_EMPTY(&section->l2tp_opt_auth[protocol].secret))
            return TRUE;
    }

    return FALSE;
}

/**
 * Create a new file, or open an existent one. Note, it truncate the file.
 *
 * @param[in]  file_name    Desired file name.
 * @param[out] file         File stream.
 *
 * @return Status code.
 */
static te_errno
l2tp_create_file(const char *file_name, FILE **file)
{
    FILE *f;
    int   fd;

    ENTRY("create file %s", file_name);

    fd = open(file_name, O_CREAT|O_WRONLY|O_TRUNC, S_IRUSR|S_IWUSR);
    if (fd == -1)
    {
        ERROR("Failed to create file '%s': %s", file_name, strerror(errno));
        return TE_OS_RC(TE_TA_UNIX, errno);
    }
    f = fdopen(fd, "w");
    if (f == NULL)
    {
        close(fd);
        ERROR("Failed to associate a stream with the file descriptor: %s",
              strerror(errno));
        return TE_OS_RC(TE_TA_UNIX, errno);
    }
    *file = f;
    return 0;
}

/**
 * Copy a content of original CHAP/PAP secrets file to the new one. It skips
 * the records are left by previous test run (it is possible in case of agent
 * fault).
 *
 * @param dst       Destination file (should be opened by caller).
 * @param src       Source file name to copy from.
 *
 * @return Status code.
 */
static te_errno
l2tp_secrets_copy(FILE *dst, const char *src)
{
    FILE    *src_file;
    te_bool  skip_line = FALSE;
    char     line[L2TP_SECRETS_LENGTH];

    ENTRY("copy data of file %s", src);

    src_file = fopen(src, "r");
    if (src_file == NULL)
    {
        ERROR("Failed to open file '%s': %s", src, strerror(errno));
        return TE_OS_RC(TE_TA_UNIX, errno);
    }

    while (fgets(line, sizeof(line), src_file) != NULL)
    {
        if (strcmp(line, L2TP_FENCE) == 0)
            skip_line = !skip_line;
        else if (!skip_line)
            fputs(line, dst);
    }

    fclose(src_file);

    return 0;
}

/**
 * Prepare configuration file for L2TP server
 *
 * @param l2tp    l2tp server structure
 *
 * @return Status code
 */
static te_errno
l2tp_server_save_conf(te_l2tp_server *l2tp)
{
    FILE                *chap_file = NULL;
    FILE                *pap_file = NULL;
    FILE                *l2tp_conf_file = NULL;
    FILE                *ppp_file = NULL;
    te_l2tp_option      *ppp_option;
    te_l2tp_section     *l2tp_section;
    char                 chap_fname[sizeof(TMP_CHAP_SECRETS_FILE)
                                    + L2TP_MAX_PID_VALUE_LENGTH];
    char                 pap_fname[sizeof(TMP_PAP_SECRETS_FILE)
                                   + L2TP_MAX_PID_VALUE_LENGTH];
    char                 l2tp_conf_fname[sizeof(L2TP_SERVER_CONF_BASIS)
                                         + L2TP_MAX_PID_VALUE_LENGTH];
    char                *ppp_fname = NULL;
    te_errno             rc;

    ENTRY("create l2tp server configuration files");

    /* Create chap_secret file */
    if (l2tp_secret_is_set(l2tp, L2TP_SECRET_PROT_CHAP))
    {
        TE_SPRINTF(chap_fname, TMP_CHAP_SECRETS_FILE ".%i", getpid());
        rc = l2tp_create_file(chap_fname, &chap_file);
        if (rc != 0)
            goto l2tp_server_save_conf_cleanup;
        rc = l2tp_secrets_copy(chap_file, L2TP_CHAP_SECRETS);
        if (rc != 0)
            goto l2tp_server_save_conf_cleanup;

        /* Add a line to isolate L2TP test specific secrets */
        fputs(L2TP_FENCE, chap_file);
        fputs(L2TP_FENCE_DESCRIPTION, chap_file);
    }

    /* Create pap_secret file */
    if (l2tp_secret_is_set(l2tp, L2TP_SECRET_PROT_PAP))
    {
        TE_SPRINTF(pap_fname, TMP_PAP_SECRETS_FILE ".%i", getpid());
        rc = l2tp_create_file(pap_fname, &pap_file);
        if (rc != 0)
            goto l2tp_server_save_conf_cleanup;
        rc = l2tp_secrets_copy(pap_file, L2TP_PAP_SECRETS);
        if (rc != 0)
            goto l2tp_server_save_conf_cleanup;

        /* Add a line to isolate L2TP test specific secrets */
        fputs(L2TP_FENCE, pap_file);
        fputs(L2TP_FENCE_DESCRIPTION, pap_file);
    }

    /* Create xl2tpd conf file */
    TE_SPRINTF(l2tp_conf_fname, L2TP_SERVER_CONF_BASIS ".%i", getpid());
    rc = l2tp_create_file(l2tp_conf_fname, &l2tp_conf_file);
    if (rc != 0)
        goto l2tp_server_save_conf_cleanup;

    /* Fill conf files with parameters */
    SLIST_FOREACH(l2tp_section, &l2tp->section, list)
    {
        INFO("Iterate on the sections: %s", l2tp_section->secname);
        if (strcmp(l2tp_section->secname, L2TP_GLOBAL) == 0)
            fputs("[global]\n", l2tp_conf_file);
        else
            fprintf(l2tp_conf_file, "[lns %s]\n", l2tp_section->secname);

        l2tp_server_save_opt(l2tp_conf_file, l2tp_section, &ppp_fname);

        l2tp_server_save_secrets_mode(l2tp_conf_file, chap_file,
                                      l2tp_section, L2TP_SECRET_PROT_CHAP);

        l2tp_server_save_secrets_mode(l2tp_conf_file, pap_file,
                                      l2tp_section, L2TP_SECRET_PROT_PAP);

        if (ppp_fname != NULL &&
            strcmp(l2tp_section->secname, L2TP_GLOBAL) != 0)
        {
            ppp_file = fopen(ppp_fname, "w");
            if (ppp_file == NULL)
            {
                ERROR("Failed to open file '%s': %s", ppp_fname, strerror(errno));
                rc = TE_OS_RC(TE_TA_UNIX, errno);
                goto l2tp_server_save_conf_cleanup;
            }
        }

        SLIST_FOREACH(ppp_option, &l2tp_section->l2tp_option, list)
        {
            if (ppp_option->type == L2TP_OPTION_TYPE_PPP)
            {
                fprintf(ppp_file, "%s %s\n",
                        ppp_option->name,
                        ppp_option->value != NULL ? ppp_option->value : "");
            }
        }
    }

l2tp_server_save_conf_cleanup:
    if (l2tp_conf_file != NULL)
        fclose(l2tp_conf_file);
    if (ppp_file != NULL)
        fclose(ppp_file);
    if (chap_file != NULL)
    {
        /* Add a line to isolate L2TP test specific secrets */
        fputs(L2TP_FENCE, chap_file);
        fclose(chap_file);
        if (rc == 0)
        {
            INFO("rename %s to %s", chap_fname, L2TP_CHAP_SECRETS);
            if (rename(chap_fname, L2TP_CHAP_SECRETS) != 0)
            {
                ERROR("Failed to rename %s to %s: %s",
                      chap_fname, L2TP_CHAP_SECRETS, strerror(errno));
                rc = TE_OS_RC(TE_TA_UNIX, errno);
            }
        }
    }
    if (pap_file != NULL)
    {
        /* Add a line to isolate L2TP test specific secrets */
        fputs(L2TP_FENCE, pap_file);
        fclose(pap_file);
        if (rc == 0)
        {
            INFO("rename %s to %s", pap_fname, L2TP_PAP_SECRETS);
            if (rename(pap_fname, L2TP_PAP_SECRETS) != 0)
            {
                ERROR("Failed to rename %s to %s: %s",
                      pap_fname, L2TP_PAP_SECRETS, strerror(errno));
                rc = TE_OS_RC(TE_TA_UNIX, errno);
            }
        }
    }

    return rc;
}

/**
 * Check if L2TP server is running
 *
 * @param l2tp  l2tp server structure
 *
 * @return l2tp server status
 */
static te_bool
l2tp_is_running(te_l2tp_server *l2tp)
{
    te_bool  is_running;
    char     l2tp_ta_pidfile[L2TP_MAX_OPTNAME_LENGTH];
    FILE    *f;

    UNUSED(l2tp);

    TE_SPRINTF(l2tp_ta_pidfile, "%s%i" , L2TP_TA_PIDFILE, getpid());

    if ((f = fopen(L2TP_SERVER_PIDFILE, "r")) != NULL)
    {
        if (fscanf(f, "%u", &l2tp_pid) != 1)
        {
            ERROR("%s(): Failed to parse l2tp_pid", __FUNCTION__);
        }
        fclose(f);
        kill(l2tp_pid, SIGTERM);
        l2tp_pid = -1;
    }
    if ((f = fopen(l2tp_ta_pidfile, "r")) != NULL)
    {
        if (fscanf(f, "%u", &l2tp_pid) != 1)
        {
            ERROR("%s(): Failed to parse l2tp_pid", __FUNCTION__);
        }
        fclose(f);
    }
    is_running = l2tp_pid > 0;

    INFO("L2TP server is%s running with pid %d", (is_running) ? "" : " not", l2tp_pid);
    return is_running;
}


/**
 * Stop l2tp server process
 *
 * @param l2tp   l2tp server structure
 *
 * @return status code
 */
static te_errno
l2tp_server_stop(te_l2tp_server *l2tp)
{
    ENTRY("stop l2tp server");

    if (l2tp_pid > 0)
    {
        kill(l2tp_pid, SIGTERM);
        l2tp_pid = 0;
    }
    else if (l2tp_is_running(l2tp))
    {
        if (!access(L2TP_INIT_SCRIPT, X_OK))
        {
            if (ta_system(L2TP_INIT_SCRIPT "stop") != 0)
            {
                ERROR("Command %s failed", L2TP_INIT_SCRIPT "stop");
                return TE_RC(TE_TA_UNIX, TE_ESHCMD);
            }
        }
        else
        {
            kill(l2tp_pid, SIGTERM);
            l2tp_pid = -1;
        }
    }

    return 0;
}

/**
 * Start l2tp server process
 *
 * @param l2tp   l2tp server structure
 *
 * @return status code
 */
static te_errno
l2tp_server_start(te_l2tp_server *l2tp)
{
    char     buf[L2TP_CMDLINE_LENGTH];
    te_errno res;
    int      ta_pid = getpid();

    l2tp_server_stop(l2tp);

    ENTRY("start l2tp server");

    res = l2tp_server_save_conf(l2tp);
    if (res != 0)
    {
        ERROR("Failed to save L2TP server configuration files");
        return res;
    }

    TE_SPRINTF(buf, "%s -D -c %s.%i -p %s%i", L2TP_SERVER_EXEC,
               L2TP_SERVER_CONF_BASIS, ta_pid, L2TP_TA_PIDFILE, ta_pid);
    if ((l2tp_pid = te_shell_cmd(buf, (uid_t) -1, NULL, NULL, NULL) < 0))
    {
        ERROR("Command %s failed", buf);
        return TE_RC(TE_TA_UNIX, TE_ESHCMD);
    }
    return 0;
}

/**
 * Method to get the L2TP server status
 *
 * @param gid           group identifier
 * @param oid           full identifier of the father instance
 * @param value         L2TP server status result
 *
 * @return status code
 */
static te_errno
l2tp_server_get(unsigned int gid, const char *oid, char *value)
{
    te_l2tp_server *l2tp = l2tp_server_find();
    UNUSED(gid);
    UNUSED(oid);

    ENTRY("get l2tp server status");

    strcpy(value, l2tp_is_running(l2tp) ? "1" : "0");
    return 0;
}

/**
 * Set desired status to L2TP server
 *
 * @param gid           group identifier
 * @param oid           full identifier of the father instance
 * @param value         L2TP server status result
 *
 * @return status code
 */
static te_errno
l2tp_server_set(unsigned int gid, const char *oid, const char *value)
{
    te_l2tp_server *l2tp = l2tp_server_find();

    UNUSED(gid);
    UNUSED(oid);

    ENTRY("set l2tp server status to %s", value);

    l2tp->started = (strcmp(value, "1") == 0);
    if (l2tp->started != l2tp_is_running(l2tp))
    {
        l2tp->changed = TRUE;
    }

    return 0;
}

/**
 * Commit changes in L2TP server configuration.
 * (Re)start/stop L2TP server if required
 *
 * @param gid           group identifier
 * @param oid           full identifier of the father instance
 * @param value         L2TP server status result
 *
 * @return status code
 */
static te_errno
l2tp_server_commit(unsigned int gid, const char *oid)
{
    te_l2tp_server *l2tp = l2tp_server_find();
    te_errno        res;

    UNUSED(gid);
    UNUSED(oid);

    ENTRY("apply changes in l2tp server configuration");

    if ((res = l2tp_server_stop(l2tp)) != 0)
    {
        ERROR("Failed to stop L2TP server");
        return res;
    }

    if (l2tp->started)
    {
        if ((res = l2tp_server_start(l2tp)) != 0)
        {
            ERROR("Failed to start L2TP server");
            return res;
        }
    }
    return 0;
}

/**
 * Return pointer to L2TP section structure
 * @param l2tp         L2TP server structure
 * @param name         Name of the section in the following
 *                     format - [lns default]|[global]
 *
 * @return L2TP section structure
 */
static te_l2tp_section *
l2tp_find_section(te_l2tp_server *l2tp, const char *name)
{
    l2tp = l2tp_server_find();
    te_l2tp_section *l2tp_section = NULL;

    for (l2tp_section = SLIST_FIRST(&l2tp->section);
         l2tp_section != NULL;
         l2tp_section = SLIST_NEXT(l2tp_section, list))
    {
        if (strcmp(l2tp_section->secname, name) == 0)
            break;
    }

    return l2tp_section;
}

/**
 * Find l2tp server option in options list
 *
 * @param l2tp          l2tp server structure
 * @param section       lns section where certain option is located
 * @param name          option name to look for
 *
 * @return l2tp server option structure
 */
static te_l2tp_option *
l2tp_find_option(te_l2tp_server *l2tp, const char *section, const char *name)
{
    te_l2tp_option  *opt = NULL;
    te_l2tp_section *sec = l2tp_find_section(l2tp, section);

    if (sec != NULL && strcmp(sec->secname, section) == 0)
    {
        for (opt = SLIST_FIRST(&sec->l2tp_option);
             opt != NULL;
             opt = SLIST_NEXT(opt, list))
        {
            if (strcmp(opt->name, name) == 0)
                break;
        }
    }

    return opt;
}

/**
 * Find l2tp server's client in options list
 *
 * @param l2tp          l2tp server structure
 * @param section       lns section where certain option is located
 * @param name          client name to look for
 *
 * @return l2tp server option structure
 */
static te_l2tp_secret *
l2tp_find_client(te_l2tp_server *l2tp, const char *section,
                 const char *name, enum l2tp_secret_prot type)
{
    te_l2tp_section  *sec = l2tp_find_section(l2tp, section);
    te_l2tp_opt_auth *opt_auth = &sec->l2tp_opt_auth[type];
    te_l2tp_secret   *secret = NULL;

    if (sec != NULL && strcmp(sec->secname, section) == 0)
    {
        for (secret = SLIST_FIRST(&opt_auth->secret);
             secret != NULL;
             secret = SLIST_NEXT(secret, list))
        {
            if (strcmp(secret->name, name) == 0)
            {
                break;
            }
        }
    }
    return secret;
}

/**
 * Find l2tp server's range in ranges list
 *
 * @param l2tp          l2tp server structure
 * @param section       lns section
 * @param opt_name      option such as 'no ip range',
 *                      'no lac range', 'ip range', 'no lac range'
 * @param range         range to look for
 *
 * @return te_l2tp_ipv4_range structure
 */
static te_l2tp_ipv4_range *
l2tp_find_range(te_l2tp_server *l2tp, const char *section,
                const char *opt_name, const char *range)
{
    te_l2tp_ipv4_range  *ip_range = NULL;
    te_l2tp_option      *option = NULL;
    te_l2tp_section     *sec = l2tp_find_section(l2tp, section);

    if (sec != NULL && strcmp(sec->secname, section) == 0)
    {
        for (option = SLIST_FIRST(&sec->l2tp_option);
             option != NULL;
             option = SLIST_NEXT(option, list))
        {
            if (option != NULL && strcmp(option->name, opt_name) == 0)
            {
                char buf_range[2*L2TP_IP_ADDR_LEN + 1];

                for (ip_range = SLIST_FIRST(&option->l2tp_range);
                     ip_range != NULL;
                     ip_range = SLIST_NEXT(ip_range, list))
                {
                    if (ip_range->end != NULL && strchr(range, '-') != NULL)
                    {
                        TE_SPRINTF(buf_range, "%s-%s", ip_range->start,
                                   ip_range->end);
                        if (strcmp(range, buf_range) == 0)
                            break;
                    }
                    else if (strchr(range, '-') == NULL)
                    {
                        strcpy(buf_range, ip_range->start);
                        if (strcmp(range, buf_range) == 0)
                            break;
                    }
                }
            }
        }
    }

    return ip_range;
}

/**
 * Get method routine.
 *
 * @param value         returned option value
 * @param option_name   name of the option
 * @param lns_name      name of the lns instance
 *
 * @return status code
 */
static te_errno
l2tp_lns_opt_get_routine(char *value, const char *option_name,
                         const char *lns_name, enum l2tp_value_type type)
{
    te_l2tp_server *l2tp = l2tp_server_find();
    te_l2tp_option *option = NULL;

    option = l2tp_find_option(l2tp, lns_name, option_name);
    if (option == NULL) {
        switch (type) {
            case L2TP_VALUE_TYPE_INT: {
                strcpy(value, "-1");
                return 0;
            }

            case L2TP_VALUE_TYPE_ADDRESS: {
                strcpy(value, "0.0.0.0");
                return 0;
            }

            default: {
                ERROR("Error in secret getting");
                return TE_RC(TE_TA_UNIX, TE_ENOENT);
            }
        }

    }
    strcpy(value, strcmp(option->value, "yes") == 0 ?
                  "1" : (strcmp(option->value, "no") == 0) ?
                        "0" : option->value);

    return 0;
}

static te_errno
l2tp_pppopt_alloc(te_l2tp_option **pppfile_opt)
{
    te_l2tp_option  *tmp = NULL;
    char             ppp_fname[] = "/tmp/te.options.xl2tpdXXXXXX";
    int              rc;

    if (mkstemp((ppp_fname)) == -1)
    {
        rc = errno;
        ERROR("mkstemp() failed: %s", strerror(rc));
        return TE_OS_RC(TE_TA_UNIX, rc);
    }

    tmp = (te_l2tp_option *)calloc(1, sizeof(te_l2tp_option));
    if (pppfile_opt == NULL)
        return TE_RC(TE_TA_UNIX, TE_ENOMEM);
    tmp->name = strdup(PPP_OPTIONS);
    tmp->value = strdup(ppp_fname);
    tmp->type = L2TP_OPTION_TYPE_L2TP;

    (*pppfile_opt) = tmp;
    return 0;
}

/**
 * Method for deleting a section from /agent/l2tp/lns.
 *
 * @param gid           group identifier
 * @param oid           full identifier of the father instance
 * @param l2tp_name     name of the l2tp instance is always empty
 * @param lns_name      name of the lns instance to delete from the list
 *
 * @return status code
 */
static te_errno
l2tp_lns_section_del(unsigned int gid, const char *oid,
                     const char *l2tp_name, const char *lns_name)
{
    te_l2tp_server  *l2tp = l2tp_server_find();
    te_l2tp_section *l2tp_section = l2tp_find_section(l2tp, lns_name);

    UNUSED(gid);
    UNUSED(oid);
    UNUSED(l2tp_name);

    if (l2tp_section == NULL)
    {
        return TE_RC(TE_TA_UNIX,  TE_ENOENT);
    }

    SLIST_REMOVE(&l2tp->section, l2tp_section, te_l2tp_section, list);
    free(l2tp_section->secname);
    free(l2tp_section);
    l2tp->changed = TRUE;
    return 0;
}

/**
 * Method for adding a section to /agent/l2tp/lns.
 *
 * @param gid           group identifier
 * @param oid           full identifier of the father instance
 * @param value         unused value of /agent/l2tp/lns instance
 * @param l2tp_name     name of the l2tp instance is always empty
 * @param lns_name      name of the lns instance to add
 *
 * @return status code
 */
static te_errno
l2tp_lns_section_add(unsigned int gid, const char *oid, const char *value,
                     const char *l2tp_name, const char *lns_name)
{
    te_l2tp_server  *l2tp = l2tp_server_find();
    te_l2tp_section *l2tp_section = l2tp_find_section(l2tp, lns_name);

    UNUSED(gid);
    UNUSED(oid);
    UNUSED(value);
    UNUSED(l2tp_name);

    if (l2tp_section != NULL)
    {
        return TE_RC(TE_TA_UNIX,  TE_EEXIST);
    }

    l2tp_section = (te_l2tp_section *)calloc(1, sizeof(te_l2tp_section));
    memset(l2tp_section->l2tp_opt_auth, '\0', sizeof(l2tp_section->l2tp_opt_auth));
    if (l2tp_section != NULL)
    {
        l2tp_section->secname = strdup(lns_name);
    }
    else
        return TE_RC(TE_TA_UNIX, TE_ENOMEM);
    SLIST_INSERT_HEAD(&l2tp->section, l2tp_section, list);
    l2tp->changed = TRUE;

    return 0;

}

/**
 * Set method routine.
 *
 * @param lns_name      name of the lns instance
 * @param option_name   name of the option
 * @param value         value to set
 * @param opt_type      PPP or L2TP option type
 *
 * @return status code
 */
static te_errno
l2tp_lns_opt_set_routine(const char *lns_name, const char *option_name,
                         const char *value,
                         enum l2tp_option_type opt_type)
{

    te_l2tp_server        *l2tp = l2tp_server_find();
    te_l2tp_section       *l2tp_section = l2tp_find_section(l2tp, lns_name);
    te_l2tp_option        *option = NULL;
    te_l2tp_option        *pppfile_opt = NULL;

    if (opt_type == L2TP_OPTION_TYPE_PPP &&
        l2tp_find_option(l2tp, lns_name, PPP_OPTIONS) == NULL)
    {
        l2tp_pppopt_alloc(&pppfile_opt);
        SLIST_INSERT_HEAD(&l2tp_section->l2tp_option, pppfile_opt, list);
    }

    option = l2tp_find_option(l2tp, lns_name, option_name);

    if (option != NULL)
    {
        free(option->value);
        option->value = strdup(value);
    }
    else
    {
        option = (te_l2tp_option *)calloc(1, sizeof(te_l2tp_option));
        if (option == NULL)
            return TE_RC(TE_TA_UNIX, TE_ENOMEM);
        option->name = strdup(option_name);
        option->value = strdup(value);
        option->type = opt_type;
        SLIST_INSERT_HEAD(&l2tp_section->l2tp_option, option, list);
    }
    return 0;
}

/**
 * Get method for /agent/l2tp/listen.
 *
 * @param gid           group identifier
 * @param oid           full identifier of the father instance
 * @param value         returned server option value
 * @param l2tp_name     name of the l2tp instance is always empty
 *
 * @return status code
 */
static te_errno
l2tp_global_listen_get(unsigned gid, const char *oid, char *value,
                       const char *l2tp_name)
{
    te_l2tp_server *l2tp = l2tp_server_find();

    UNUSED(gid);
    UNUSED(oid);
    UNUSED(l2tp_name);

    if (l2tp_find_section(l2tp, L2TP_GLOBAL) == NULL)
    {
        l2tp_lns_section_add(0, NULL, NULL, NULL, L2TP_GLOBAL);
    }

    return l2tp_lns_opt_get_routine(value, "listen-addr",
                                    L2TP_GLOBAL, L2TP_VALUE_TYPE_ADDRESS);
}

/**
 * Set method for /agent/l2tp/listen
 *
 * @param gid           group identifier
 * @param oid           full identifier of the father instance
 * @param value         value to set
 * @param l2tp_name     name of the l2tp instance is always empty
 *
 * @return status code
 */
static te_errno
l2tp_global_listen_set(unsigned int gid, const char *oid, const char *value,
                       const char *l2tp_name)
{
    UNUSED(gid);
    UNUSED(oid);
    UNUSED(l2tp_name);

    return l2tp_lns_opt_set_routine(L2TP_GLOBAL, "listen-addr", value,
                                    L2TP_OPTION_TYPE_L2TP);
}

/**
 * Get method for /agent/l2tp/port.
 *
 * @param gid           group identifier
 * @param oid           full identifier of the father instance
 * @param value         returned server option value
 * @param l2tp_name     name of the l2tp instance is always empty
 *
 * @return status code
 */
static te_errno
l2tp_global_port_get(unsigned gid, const char *oid, char *value,
                     const char *l2tp_name)
{
    te_l2tp_server *l2tp = l2tp_server_find();

    UNUSED(gid);
    UNUSED(oid);
    UNUSED(l2tp_name);

    if (l2tp_find_section(l2tp, L2TP_GLOBAL) == NULL)
    {
        l2tp_lns_section_add(0, NULL, NULL, NULL, L2TP_GLOBAL);
    }

    return l2tp_lns_opt_get_routine(value, "port",
                                    L2TP_GLOBAL, L2TP_VALUE_TYPE_INT);
}

/**
 * Set method for /agent/l2tp/port.
 *
 * @param gid           group identifier
 * @param oid           full identifier of the father instance
 * @param value         value to set
 * @param l2tp_name     name of the l2tp instance is always empty
 *
 * @return status code
 */
static te_errno
l2tp_global_port_set(unsigned int gid, const char *oid, const char *value,
                     const char *l2tp_name)
{


    UNUSED(gid);
    UNUSED(oid);
    UNUSED(l2tp_name);

    return l2tp_lns_opt_set_routine(L2TP_GLOBAL, "port", value,
                                    L2TP_OPTION_TYPE_L2TP);
}

/**
 * Method for obtaining the list of sections from /agent/l2tp/lns.
 *
 * @param gid           group identifier
 * @param oid           full identifier of the father instance
 * @param list          location of the lns sections list
 * @param l2tp_name     name of the l2tp instance is always empty

 * @return status code
 */
static te_errno
l2tp_lns_section_list(unsigned int gid, const char *oid,
                      char **list, const char *l2tp_name)
{
    te_l2tp_server  *l2tp = l2tp_server_find();
    te_l2tp_section *l2tp_section;
    te_string        str = TE_STRING_INIT;
    uint32_t         list_size;

    UNUSED(gid);
    UNUSED(oid);
    UNUSED(l2tp_name);

    list_size = L2TP_SERVER_LIST_SIZE;
    if ((*list = (char *)calloc(1, list_size)) == NULL)
    {
        return TE_RC(TE_TA_UNIX, TE_ENOMEM);
    }
    SLIST_FOREACH(l2tp_section, &l2tp->section, list)
    {
        if (strcmp(l2tp_section->secname, L2TP_GLOBAL) != 0)
            te_string_append(&str, "%s ",l2tp_section->secname);
    }
    *list = str.ptr;
    return 0;
}

/**
 * Get method for /agent/l2tp/lns/pppopt/mtu
 *
 * @param gid           group identifier
 * @param oid           full identifier of the father instance
 * @param value         value to set
 * @param l2tp_name     name of the l2tp instance is always empty
 * @param lns_name      name of the lns instance
 *
 * @return status code
 */
static te_errno
l2tp_lns_opt_get_mtu(unsigned int gid, const char *oid, char *value,
                     const char *l2tp_name, const char *lns_name)
{

    UNUSED(oid);
    UNUSED(gid);
    UNUSED(l2tp_name);

    return  l2tp_lns_opt_get_routine(value, "mtu",
                                     lns_name, L2TP_VALUE_TYPE_INT);
}

/**
 * Set method for /agent/l2tp/lns/pppopt/mtu
 *
 * @param gid           group identifier
 * @param oid           full identifier of the father instance
 * @param value         value to set
 * @param l2tp_name     name of the l2tp instance is always empty
 * @param lns_name      name of the lns instance
 *
 * @return status code
 */
static te_errno
l2tp_lns_opt_set_mtu(unsigned int gid, const char *oid, const char *value,
                     const char *l2tp_name, const char *lns_name)
{

    UNUSED(gid);
    UNUSED(oid);
    UNUSED(l2tp_name);

    return l2tp_lns_opt_set_routine(lns_name, "mtu", value,
                                    L2TP_OPTION_TYPE_PPP);
}

/**
 * Get method for /agent/l2tp/lns/pppopt/mru
 *
 * @param gid           group identifier
 * @param oid           full identifier of the father instance
 * @param value         value to set
 * @param l2tp_name     name of the l2tp instance is always empty
 * @param lns_name      name of the lns instance
 *
 * @return status code
 */
static te_errno
l2tp_lns_opt_get_mru(unsigned int gid, const char *oid, char *value,
                     const char *l2tp_name, const char *lns_name)
{

    UNUSED(oid);
    UNUSED(gid);
    UNUSED(l2tp_name);

    return  l2tp_lns_opt_get_routine(value, "mru",
                                     lns_name, L2TP_VALUE_TYPE_INT);
}

/**
 * Set method for /agent/l2tp/lns/pppopt/mru
 *
 * @param gid           group identifier
 * @param oid           full identifier of the father instance
 * @param value         value to set
 * @param l2tp_name     name of the l2tp instance is always empty
 * @param lns_name      name of the lns instance
 *
 * @return status code
 */
static te_errno
l2tp_lns_opt_set_mru(unsigned int gid, const char *oid, char *value,
                     const char *l2tp_name, const char *lns_name)
{

    UNUSED(gid);
    UNUSED(oid);
    UNUSED(l2tp_name);

    return l2tp_lns_opt_set_routine(lns_name, "mru", value,
                                    L2TP_OPTION_TYPE_PPP);
}

/**
 * Get method for /agent/l2tp/lns/pppopt/lcp-echo-interval
 *
 * @param gid           group identifier
 * @param oid           full identifier of the father instance
 * @param value         value to set
 * @param l2tp_name     name of the l2tp instance is always empty
 * @param lns_name      name of the lns instance
 *
 * @return status code
 */
static te_errno
l2tp_lns_opt_get_lei(unsigned int gid, const char *oid, char *value,
                     const char *l2tp_name, const char *lns_name)
{

    UNUSED(oid);
    UNUSED(gid);
    UNUSED(l2tp_name);

    return l2tp_lns_opt_get_routine(value, "lcp-echo-interval",
                                    lns_name, L2TP_VALUE_TYPE_INT);
}

/**
 * Set method for /agent/l2tp/lns/pppopt/lcp-echo-interval
 *
 * @param gid           group identifier
 * @param oid           full identifier of the father instance
 * @param value         value to set
 * @param l2tp_name     name of the l2tp instance is always empty
 * @param lns_name      name of the lns instance
 *
 * @return status code
 */
static te_errno
l2tp_lns_opt_set_lei(unsigned int gid, const char *oid, char *value,
                     const char *l2tp_name, const char *lns_name)
{

    UNUSED(gid);
    UNUSED(oid);
    UNUSED(l2tp_name);

    return l2tp_lns_opt_set_routine(lns_name, "lcp-echo-interval", value,
                                    L2TP_OPTION_TYPE_PPP);
}

/**
 * Get method for /agent/l2tp/lns/pppopt/lcp-echo-failure
 *
 * @param gid           group identifier
 * @param oid           full identifier of the father instance
 * @param value         value to set
 * @param l2tp_name     name of the l2tp instance is always empty
 * @param lns_name      name of the lns instance
 *
 * @return status code
 */
static te_errno
l2tp_lns_opt_get_lef(unsigned int gid, const char *oid, char *value,
                     const char *l2tp_name, const char *lns_name)
{

    UNUSED(oid);
    UNUSED(gid);
    UNUSED(l2tp_name);

    return l2tp_lns_opt_get_routine(value, "lcp-echo-failure",
                                    lns_name, L2TP_VALUE_TYPE_INT);
}

/**
 * Set method for /agent/l2tp/lns/pppopt/lcp-echo-failure
 *
 * @param gid           group identifier
 * @param oid           full identifier of the father instance
 * @param value         value to set
 * @param l2tp_name     name of the l2tp instance is always empty
 * @param lns_name      name of the lns instance
 *
 * @return status code
 */
static te_errno
l2tp_lns_opt_set_lef(unsigned int gid, const char *oid, char *value,
                     const char *l2tp_name, const char *lns_name)
{

    UNUSED(gid);
    UNUSED(oid);
    UNUSED(l2tp_name);

    return l2tp_lns_opt_set_routine(lns_name, "lcp-echo-failure", value,
                                    L2TP_OPTION_TYPE_PPP);
}

/**
 * Get method for /agent/l2tp/lns/use_challenge
 *
 * @param gid           group identifier
 * @param oid           full identifier of the father instance
 * @param value         value to set
 * @param l2tp_name     name of the l2tp instance is always empty
 * @param lns_name      name of the lns instance
 *
 * @return status code
 */
static te_errno
l2tp_lns_opt_get_challenge(unsigned int gid, const char *oid, char *value,
                           const char *l2tp_name, const char *lns_name)
{

    UNUSED(oid);
    UNUSED(gid);
    UNUSED(l2tp_name);

    return l2tp_lns_opt_get_routine(value, "challenge",
                                    lns_name, L2TP_VALUE_TYPE_INT);
}

/**
 * Set method for /agent/l2tp/lns/use_challenge
 *
 * @param gid           group identifier
 * @param oid           full identifier of the father instance
 * @param value         value to set
 * @param l2tp_name     name of the l2tp instance is always empty
 * @param lns_name      name of the lns instance
 *
 * @return status code
 */
static te_errno
l2tp_lns_opt_set_challenge(unsigned int gid, const char *oid, char *value,
                           const char *l2tp_name, const char *lns_name)
{

    UNUSED(gid);
    UNUSED(oid);
    UNUSED(l2tp_name);

    return l2tp_lns_opt_set_routine(lns_name, "challenge", value,
                                    L2TP_OPTION_TYPE_L2TP);
}

/**
 * Get method for /agent/l2tp/lns/ppp_debug
 *
 * @param gid           group identifier
 * @param oid           full identifier of the father instance
 * @param value         value to set
 * @param l2tp_name     name of the l2tp instance is always empty
 * @param lns_name      name of the lns instance
 *
 * @return status code
 */
static te_errno
l2tp_lns_opt_get_debug(unsigned int gid, const char *oid, char *value,
                       const char *l2tp_name, const char *lns_name)
{

    UNUSED(oid);
    UNUSED(gid);
    UNUSED(l2tp_name);

    return l2tp_lns_opt_get_routine(value, "ppp debug",
                                    lns_name, L2TP_VALUE_TYPE_INT);
}

/**
 * Set method for /agent/l2tp/lns/ppp_debug
 *
 * @param gid           group identifier
 * @param oid           full identifier of the father instance
 * @param value         value to set
 * @param l2tp_name     name of the l2tp instance is always empty
 * @param lns_name      name of the lns instance
 *
 * @return status code
 */
static te_errno
l2tp_lns_opt_set_debug(unsigned int gid, const char *oid, char *value,
                       const char *l2tp_name, const char *lns_name)
{

    UNUSED(gid);
    UNUSED(oid);
    UNUSED(l2tp_name);

    return l2tp_lns_opt_set_routine(lns_name, "ppp debug", value,
                                    L2TP_OPTION_TYPE_L2TP);
}

/**
 * Get method for /agent/l2tp/lns/unix_auth
 *
 * @param gid           group identifier
 * @param oid           full identifier of the father instance
 * @param value         value to set
 * @param l2tp_name     name of the l2tp instance is always empty
 * @param lns_name      name of the lns instance
 *
 * @return status code
 */
static te_errno
l2tp_lns_opt_get_uauth(unsigned int gid, const char *oid, char *value,
                       const char *l2tp_name, const char *lns_name)
{

    UNUSED(oid);
    UNUSED(gid);
    UNUSED(l2tp_name);

    return l2tp_lns_opt_get_routine(value, "unix authentication",
                                    lns_name, L2TP_VALUE_TYPE_INT);
}

/**
 * Set method for /agent/l2tp/lns/unix_auth
 *
 * @param gid           group identifier
 * @param oid           full identifier of the father instance
 * @param value         value to set
 * @param l2tp_name     name of the l2tp instance is always empty
 * @param lns_name      name of the lns instance
 *
 * @return status code
 */
static te_errno
l2tp_lns_opt_set_uauth(unsigned int gid, const char *oid, char *value,
                       const char *l2tp_name, const char *lns_name)
{

    UNUSED(gid);
    UNUSED(oid);
    UNUSED(l2tp_name);

    return l2tp_lns_opt_set_routine(lns_name, "unix authentication", value,
                                    L2TP_OPTION_TYPE_L2TP);
}

static te_errno
l2tp_lns_bit_get(unsigned int gid, const char *oid, char *value,
                 const char *l2tp_name, const char *lns_name,
                 const char *bit_name)
{
    char             buf_bit[L2TP_MAX_OPTNAME_LENGTH];

    UNUSED(oid);
    UNUSED(gid);
    UNUSED(l2tp_name);

    TE_SPRINTF(buf_bit, "%s bit", bit_name);

    return l2tp_lns_opt_get_routine(value, buf_bit,
                                    lns_name, L2TP_VALUE_TYPE_INT);
}

/**
 * Add method for /agent/l2tp/lns/bit
 *
 * @param gid           group identifier
 * @param oid           full identifier of the father instance
 * @param value         value to set
 * @param l2tp_name     name of the l2tp instance is always empty
 * @param lns_name      name of the lns instance
 * @param bit_name      bit instance name
 *
 * @return status code
 */
static te_errno
l2tp_lns_bit_add(unsigned int gid, const char *oid, const char *value,
                 const char *l2tp_name, const char *lns_name,
                 const char *bit_name)
{
    te_l2tp_server  *l2tp = l2tp_server_find();
    te_l2tp_section *l2tp_section = l2tp_find_section(l2tp, lns_name);
    te_l2tp_option  *l2tp_option  = NULL;
    char             buf_bit[L2TP_MAX_OPTNAME_LENGTH];

    UNUSED(gid);
    UNUSED(oid);
    UNUSED(value);
    UNUSED(l2tp_name);

    TE_SPRINTF(buf_bit, "%s bit", bit_name);
    l2tp_option = l2tp_find_option(l2tp, lns_name, bit_name);

    if (l2tp_option != NULL)
    {
        return TE_RC(TE_TA_UNIX,  TE_EEXIST);
    }

    l2tp_option = (te_l2tp_option *)calloc(1, sizeof(te_l2tp_option));
    if (l2tp_option != NULL)
    {

        l2tp_option->name = strdup(buf_bit);
        l2tp_option->value = strdup(strcmp(value, "1") == 0 ? "yes" : "no");
        l2tp_option->type = L2TP_OPTION_TYPE_L2TP;
    }
    else
        return TE_RC(TE_TA_UNIX, TE_ENOMEM);
    SLIST_INSERT_HEAD(&l2tp_section->l2tp_option, l2tp_option, list);
    l2tp->changed = TRUE;

    return 0;
}

/**
 * Delete method for /agent/l2tp/lns/bit
 *
 * @param gid           group identifier
 * @param oid           full identifier of the father instance
 * @param l2tp_name     name of the l2tp instance is always empty
 * @param lns_name      name of the lns instance
 * @param bit_name      bit instance name
 *
 * @return status code
 */
static te_errno
l2tp_lns_bit_del(unsigned int gid, const char *oid,
                 const char *l2tp_name, const char *lns_name,
                 const char *bit_name)
{
    te_l2tp_server  *l2tp = l2tp_server_find();
    te_l2tp_section *l2tp_section = l2tp_find_section(l2tp, lns_name);
    char buf_opt[L2TP_MAX_OPTNAME_LENGTH];
    te_l2tp_option  *l2tp_option;

    TE_SPRINTF(buf_opt, "%s bit", bit_name);
    l2tp_option = l2tp_find_option(l2tp, lns_name, buf_opt);

    UNUSED(gid);
    UNUSED(oid);
    UNUSED(l2tp_name);

    if (l2tp_section == NULL)
    {
        return TE_RC(TE_TA_UNIX,  TE_ENOENT);
    }

    SLIST_REMOVE(&l2tp_section->l2tp_option, l2tp_option,
                 te_l2tp_option, list);

    free(l2tp_option->name);
    free(l2tp_option->value);
    free(l2tp_option);
    l2tp->changed = TRUE;
    return 0;
}

/**
 * Method for obtaining the list of sections from /agent/l2tp/lns/bit.
 *
 * @param gid           group identifier
 * @param oid           full identifier of the father instance
 * @param list          location of the lns sections list
 * @param l2tp_name     name of the l2tp instance is always empty

 * @return status code
 */
static te_errno
l2tp_lns_bit_list(unsigned int gid, const char *oid,
                  char **list, const char *l2tp_name,
                  const char *lns_name)
{
    te_l2tp_server  *l2tp = l2tp_server_find();
    te_l2tp_section *l2tp_section = l2tp_find_section(l2tp, lns_name);
    te_l2tp_option  *l2tp_option;
    te_string        str = TE_STRING_INIT;
    uint32_t         list_size;

    UNUSED(gid);
    UNUSED(oid);
    UNUSED(l2tp_name);

    list_size = L2TP_SECRETS_LENGTH;
    if ((*list = (char *)calloc(1, list_size)) == NULL)
    {
        return TE_RC(TE_TA_UNIX, TE_ENOMEM);
    }

    SLIST_FOREACH(l2tp_option, &l2tp_section->l2tp_option, list)
    {
        if (strcmp(l2tp_option->name, "hidden bit") == 0)
        {
            te_string_append(&str, "hidden ");
        }
        else if (strcmp(l2tp_option->name, "length bit") == 0)
        {
            te_string_append(&str, "length ");
        }
    }

    *list = str.ptr;
    return 0;
}

static te_errno
l2tp_lns_policy_get_routine(te_l2tp_section *section,
                            enum l2tp_secret_prot type,
                            enum l2tp_auth_policy policy,
                            char *value)
{
    te_l2tp_opt_auth      *opt_auth = &section->l2tp_opt_auth[type];

    if (opt_auth->policy != policy)
    {
        strcpy(value, "-1");
    }
    else
    {
        strcpy(value, opt_auth->is_enabled ? "1" : "0");
    }

    return 0;
}

static te_errno
l2tp_lns_policy_set_routine(te_l2tp_section *section,
                            enum l2tp_secret_prot type,
                            enum l2tp_auth_policy policy,
                            te_bool value)
{
    te_l2tp_opt_auth      *opt_auth = &section->l2tp_opt_auth[type];

    opt_auth->policy = policy;
    opt_auth->is_enabled = value;

    return 0;
}

/**
 * Get method for /agent/l2tp/lns/auth/require
 *
 * @param gid           group identifier
 * @param oid           full identifier of the father instance
 * @param value         returned value
 * @param l2tp_name     name of the l2tp instance is always empty
 * @param lns_name      name of the lns instance
 * @param auth_type     name of the auth instance (chap|pap)
 *
 * @return status code
 */
static te_errno
l2tp_lns_require_get(unsigned int gid, const char *oid, char *value,
                     const char *l2tp_name, const char *lns_name,
                     const char *auth_type)
{
    te_l2tp_server        *l2tp = l2tp_server_find();
    te_l2tp_section       *section = l2tp_find_section(l2tp, lns_name);
    enum l2tp_secret_prot  type = strcmp(auth_type, "chap") == 0 ?
                                  L2TP_SECRET_PROT_CHAP : L2TP_SECRET_PROT_PAP;
    UNUSED(gid);
    UNUSED(oid);
    UNUSED(l2tp_name);

    return l2tp_lns_policy_get_routine(section, type,
                                       L2TP_AUTH_POLICY_REQUIRE, value);
}

/**
 * Set method for /agent/l2tp/lns/auth/refuse|require
 *
 * @param gid           group identifier
 * @param oid           full identifier of the father instance
 * @param value         value to set
 * @param l2tp_name     name of the l2tp instance is always empty
 * @param lns_name      name of the lns instance
 * @param auth_type     name of the auth instance (chap|pap|authentication)
 *
 * @return status code
 */
static te_errno
l2tp_lns_require_set(unsigned int gid, const char *oid, char *value,
                     const char *l2tp_name, const char *lns_name,
                     const char *auth_type)
{

    te_l2tp_server        *l2tp = l2tp_server_find();
    te_l2tp_section       *section = l2tp_find_section(l2tp, lns_name);
    enum l2tp_secret_prot  type = strcmp(auth_type, "chap") == 0 ?
                                  L2TP_SECRET_PROT_CHAP : L2TP_SECRET_PROT_PAP;

    UNUSED(oid);
    UNUSED(gid);
    UNUSED(l2tp_name);

    return l2tp_lns_policy_set_routine(section, type, L2TP_AUTH_POLICY_REQUIRE,
                                       strcmp(value, "1") == 0 ?
                                       TRUE : FALSE);
}

/**
 * Get method for /agent/l2tp/lns/auth/refuse
 *
 * @param gid           group identifier
 * @param oid           full identifier of the father instance
 * @param value         returned value
 * @param l2tp_name     name of the l2tp instance is always empty
 * @param lns_name      name of the lns instance
 * @param auth_type     name of the auth instance (chap|pap|authentication)
 *
 * @return status code
 */
static te_errno
l2tp_lns_refuse_get(unsigned int gid, const char *oid, char *value,
                    const char *l2tp_name, const char *lns_name,
                    const char *auth_type)
{
    te_l2tp_server        *l2tp = l2tp_server_find();
    te_l2tp_section       *section = l2tp_find_section(l2tp, lns_name);
    enum l2tp_secret_prot  type = strcmp(auth_type, "chap") == 0 ?
                                  L2TP_SECRET_PROT_CHAP : L2TP_SECRET_PROT_PAP;
    UNUSED(gid);
    UNUSED(oid);
    UNUSED(l2tp_name);

    return l2tp_lns_policy_get_routine(section, type, L2TP_AUTH_POLICY_REFUSE, value);

}

/**
 * Set method for /agent/l2tp/lns/auth/refuse
 *
 * @param gid           group identifier
 * @param oid           full identifier of the father instance
 * @param value         value to set
 * @param l2tp_name     name of the l2tp instance is always empty
 * @param lns_name      name of the lns instance
 * @param auth_type     name of the auth instance (chap|pap|authentication)
 *
 * @return status code
 */
static te_errno
l2tp_lns_refuse_set(unsigned int gid, const char *oid, char *value,
                    const char *l2tp_name, const char *lns_name,
                    const char *auth_type)
{
    te_l2tp_server        *l2tp = l2tp_server_find();
    te_l2tp_section       *section = l2tp_find_section(l2tp, lns_name);
    enum l2tp_secret_prot  type = strcmp(auth_type, "chap") == 0 ?
                                  L2TP_SECRET_PROT_CHAP : L2TP_SECRET_PROT_PAP;

    UNUSED(oid);
    UNUSED(gid);
    UNUSED(l2tp_name);

    return l2tp_lns_policy_set_routine(section, type,
                                       L2TP_AUTH_POLICY_REFUSE,
                                       strcmp(value, "1") == 0 ?
                                       TRUE : FALSE);
}

/**
 * Get method for /agent/l2tp/lns/local_ip
 *
 * @param gid           group identifier
 * @param oid           full identifier of the father instance
 * @param value         returned local_ip
 * @param l2tp_name     name of the l2tp instance is always empty
 * @param lns_name      name of the lns instance
 *
 * @return status code
 */
static te_errno
l2tp_lns_local_ip_get(unsigned int gid, const char *oid, char *value,
                      const char *l2tp_name, const char *lns_name)
{

    UNUSED(gid);
    UNUSED(oid);
    UNUSED(l2tp_name);

    return l2tp_lns_opt_get_routine(value, "local ip",
                                    lns_name, L2TP_VALUE_TYPE_ADDRESS);
}

/**
 * Set method for /agent/l2tp/lns/local_ip
 *
 * @param gid           group identifier
 * @param oid           full identifier of the father instance
 * @param value         local_ip to set
 * @param l2tp_name     name of the l2tp instance is always empty
 * @param lns_name      name of the lns instance
 *
 * @return status code
 */
static te_errno
l2tp_lns_local_ip_set(unsigned int gid, const char *oid, char *value,
                      const char *l2tp_name, const char *lns_name)
{
    UNUSED(gid);
    UNUSED(oid);
    UNUSED(l2tp_name);

    return l2tp_lns_opt_set_routine(lns_name, "local ip", value,
                                    L2TP_OPTION_TYPE_L2TP);
}

/**
 * Add method routine for ranges
 *
 * @param lns_name             name of the lns instance
 * @param option_name          name of the option
 * @param range                ip range or lac range
 *                             like "192.168.37.38-192.168.37.40"
 *
 * @return status code
 */
static te_errno
l2tp_lns_range_add_routine(const char *lns_name, const char *option_name,
                           const char *range)
{
    char                 buf_ip[L2TP_IP_ADDR_LEN] = "/0";
    char                *hyphen;

    te_l2tp_option      *l2tp_option;
    te_l2tp_ipv4_range  *l2tp_range;
    te_l2tp_server      *l2tp = l2tp_server_find();
    te_l2tp_section     *l2tp_section = l2tp_find_section(l2tp, lns_name);
    int                  rc;

    if ((l2tp_option = l2tp_find_option(l2tp, lns_name, option_name)) == NULL)
    {
        l2tp_option = (te_l2tp_option *)calloc(1, sizeof(te_l2tp_option));
        if (l2tp_option == NULL)
            return TE_RC(TE_TA_UNIX, TE_ENOMEM);
        l2tp_option->name = strdup(option_name);
        l2tp_option->value = NULL;
        l2tp_option->type = L2TP_OPTION_TYPE_L2TP;
        SLIST_INSERT_HEAD(&l2tp_section->l2tp_option, l2tp_option, list);
    }

    if ((l2tp_range = l2tp_find_range(l2tp, lns_name, option_name, range)) != NULL)
    {
        return TE_RC(TE_TA_UNIX,  TE_EEXIST);
    }
    l2tp_range = (te_l2tp_ipv4_range *)calloc(1, sizeof(te_l2tp_ipv4_range));
    if (l2tp_range == NULL)
        return TE_RC(TE_TA_UNIX, TE_ENOMEM);
    hyphen = strchr(range, '-');
    if (hyphen != NULL)
    {
        memcpy(buf_ip, range, strlen(range) - strlen(hyphen));
        if (inet_aton(buf_ip, NULL) == 0)
        {
            free(l2tp_range);
            rc = errno;
            ERROR("First ip address in range is incorrect: %s", strerror(rc));
            return TE_OS_RC(TE_TA_UNIX, rc);
        }

        l2tp_range->start = strdup(buf_ip);
        memset(buf_ip, '\0', sizeof(buf_ip));
        memcpy(buf_ip, hyphen + 1, sizeof(buf_ip));
        if (inet_aton(buf_ip, NULL) == 0)
        {
            free(l2tp_range);
            free(l2tp_range->start);
            rc = errno;
            ERROR("Second ip address in range is incorrect: %s", strerror(rc));
            return TE_OS_RC(TE_TA_UNIX, rc);
        }
        l2tp_range->end = strdup(buf_ip);
    }
    else
    {
        memcpy(buf_ip, range, strlen(range) - strlen(hyphen));
        if (inet_aton(buf_ip, NULL) == 0)
        {
            free(l2tp_range);
            rc = errno;
            ERROR("First ip address in range is incorrect: %s", strerror(rc));
            return TE_OS_RC(TE_TA_UNIX, rc);
        }
        l2tp_range->start = strdup(buf_ip);
        l2tp_range->end = NULL;
    }

    SLIST_INSERT_HEAD(&l2tp_option->l2tp_range, l2tp_range, list);
    l2tp->changed = TRUE;

    return 0;
}

/**
 * Method for addding a range to /agent/l2tp/lns/ip_range
 *
 * @param gid                  group identifier
 * @param oid                  full identifier of the father instance
 * @param l2pt_name            name of the l2tp instance is always empty
 * @param lns_name             name of the lns instance
 * @param range                name of the ../ip_range instances
 *                             to add
 *                             like "192.168.37.38-192.168.37.40"
 *
 * @return status code
 */
static te_errno
l2tp_lns_ip_range_add(unsigned int gid, const char *oid, const char *value,
                      const char *l2tp_name, const char *lns_name,
                      const char *range)
{
    char    optname[L2TP_MAX_OPTNAME_LENGTH];

    UNUSED(gid);
    UNUSED(oid);
    UNUSED(l2tp_name);

    TE_SPRINTF(optname, "%sip range", strcmp(value, "deny") == 0 ? "no " : "");

    return l2tp_lns_range_add_routine(lns_name, optname, range);
}

/**
 * Method for addding a range to /agent/l2tp/lns/lac_range
 *
 * @param gid                  group identifier
 * @param oid                  full identifier of the father instance
 * @param l2pt_name            name of the l2tp instance is always empty
 * @param lns_name             name of the lns instance
 * @param range                name of the ../lac_range instances
 *                             to add
 *                             like "192.168.37.38-192.168.37.40"
 *
 * @return status code
 */
static te_errno
l2tp_lns_lac_range_add(unsigned int gid, const char *oid, const char *value,
                       const char *l2tp_name, const char *lns_name,
                       const char *range)
{
    char    optname[L2TP_MAX_OPTNAME_LENGTH];

    UNUSED(gid);
    UNUSED(oid);
    UNUSED(l2tp_name);

    TE_SPRINTF(optname, "%slac", strcmp(value, "deny") == 0 ? "no " : "");

    return l2tp_lns_range_add_routine(lns_name, optname, range);
}

/**
 * Get range routine
 *
 * @param value         returning value
 * @param option_name   ip range or lac range
 * @param lns_name      name of the lns instance
 * @param range         range like "192.168.37.38-192.168.37.40"
 *
 * @return status code
 */
static te_errno
l2tp_lns_range_get_routine(char *value, const char *option_name,
                           const char *lns_name, const char *range)
{
    te_l2tp_server     *l2tp = l2tp_server_find();
    te_l2tp_ipv4_range *l2tp_range = l2tp_find_range(l2tp, lns_name,
                                                     option_name, range);

    char buf_opt[L2TP_MAX_OPTNAME_LENGTH];

    if (l2tp_range != NULL)
    {
        strcpy(value, "allow");
        return 0;
    }
    TE_SPRINTF(buf_opt, "no %s", option_name);
    l2tp_range = l2tp_find_range(l2tp, lns_name, buf_opt, range);
    if (l2tp_range != NULL)
    {
        strcpy(value, "deny");
        return 0;
    }
    strcpy(value, "");

    return 0;
}

/**
 * Get method for /agent/l2tp/lns/ip_range
 *
 * @param gid           group identifier
 * @param oid           full identifier of the father instance
 * @param value         value to set
 * @param l2tp_name     name of the l2tp instance is always empty
 * @param lns_name      name of the lns instance
 * @param range         range like "192.168.37.38-192.168.37.40"
 *
 * @return status code
 */
static te_errno
l2tp_lns_ip_range_get(unsigned int gid, const char *oid, char *value,
                      const char *l2tp_name, const char *lns_name,
                      const char *range)
{
    UNUSED(oid);
    UNUSED(gid);
    UNUSED(l2tp_name);

    return l2tp_lns_range_get_routine(value, "ip range", lns_name, range);
}

/**
 * Get method for /agent/l2tp/lns/lac_range
 *
 * @param gid           group identifier
 * @param oid           full identifier of the father instance
 * @param value         value to set
 * @param l2tp_name     name of the l2tp instance is always empty
 * @param lns_name      name of the lns instance
 * @param range         range like "192.168.37.38-192.168.37.40"
 *
 * @return status code
 */
static te_errno
l2tp_lns_lac_range_get(unsigned int gid, const char *oid, char *value,
                       const char *l2tp_name, const char *lns_name,
                       const char *range)
{

    UNUSED(oid);
    UNUSED(gid);
    UNUSED(l2tp_name);

    return l2tp_lns_range_get_routine(value, "lac range", lns_name, range);
}

/**
 * List method routing
 *
 * @param lns_name      name of the lns instance
 * @param option_name   lac range or ip range

 * @return status code
 */
static char *
l2tp_lns_range_list_routine(const char *lns_name, const char *option_name)
{
    te_l2tp_server       *l2tp = l2tp_server_find();
    te_l2tp_section      *l2tp_section = l2tp_find_section(l2tp, lns_name);
    te_l2tp_option       *l2tp_option = l2tp_find_option(l2tp, lns_name, option_name);
    te_l2tp_ipv4_range   *l2tp_range;

    te_string        str = TE_STRING_INIT;

    if (l2tp_section != NULL && l2tp_option != NULL)
    {
        if (strcmp(l2tp_section->secname, L2TP_GLOBAL) != 0)
        {
            SLIST_FOREACH(l2tp_range, &l2tp_option->l2tp_range, list)
            {
                te_string_append(&str, "%s-%s ", l2tp_range->start,
                                 l2tp_range->end);
            }
        }
    }

    return str.ptr;
}

/**
 * Method for obtaining the list of /agent/l2tp/lns/lac_range.
 *
 * @param gid           group identifier
 * @param oid           full identifier of the father instance
 * @param list          location of the lns sections list
 * @param l2tp_name     name of the l2tp instance is always empty
 * @param lns_name      name of the lns instance

 * @return status code
 */
static te_errno
l2tp_lns_lac_range_list(unsigned int gid, const char *oid,
                        char **list, const char *l2tp_name,
                        const char *lns_name)
{
    uint32_t  list_size;
    UNUSED(gid);
    UNUSED(oid);
    UNUSED(l2tp_name);

    list_size = L2TP_SERVER_LIST_SIZE;
    if ((*list = (char *)calloc(1, list_size)) == NULL)
    {
        return TE_RC(TE_TA_UNIX, TE_ENOMEM);
    }

    *list = l2tp_lns_range_list_routine(lns_name, "lac range");
    return 0;
}

/**
 * Method for obtaining the list of /agent/l2tp/lns/ip_range.
 *
 * @param gid           group identifier
 * @param oid           full identifier of the father instance
 * @param list          location of the lns sections list
 * @param l2tp_name     name of the l2tp instance is always empty
 * @param lns_name      name of the lns instance

 * @return status code
 */
static te_errno
l2tp_lns_ip_range_list(unsigned int gid, const char *oid,
                       char **list, const char *l2tp_name,
                       const char *lns_name)
{
    uint32_t  list_size;
    UNUSED(gid);
    UNUSED(oid);
    UNUSED(l2tp_name);

    list_size = L2TP_SERVER_LIST_SIZE;
    if ((*list = (char *)calloc(1, list_size)) == NULL)
    {
        return TE_RC(TE_TA_UNIX, TE_ENOMEM);
    }

    *list = l2tp_lns_range_list_routine(lns_name, "ip range");
    return 0;
}

/**
 * Delete method routine
 *
 * @param lns_name             name of the lns instance
 * @paran option_name          option name
 * @param range                range to delete
 *                             like "192.168.37.38-192.168.37.40"
 *
 * @return status code
 */
static te_errno
l2tp_lns_range_del_routine(const char *lns_name, const char *option_name,
                           const char *range)
{
    char                 opt_buf[L2TP_MAX_OPTNAME_LENGTH];
    te_l2tp_ipv4_range  *l2tp_range;
    te_l2tp_option      *l2tp_option;
    te_l2tp_server      *l2tp = l2tp_server_find();


    if ((l2tp_option = l2tp_find_option(l2tp, lns_name, option_name)) == NULL)
    {
        TE_SPRINTF(opt_buf, "no %s", option_name);

        if ((l2tp_option = l2tp_find_option(l2tp, lns_name, opt_buf)) == NULL)
        {
            return TE_RC(TE_TA_UNIX,  TE_ENOENT);
        }
    }
    else
    {
        TE_SPRINTF(opt_buf, "%s", option_name);
    }

    if ((l2tp_range = l2tp_find_range(l2tp, lns_name, opt_buf, range)) == NULL)
    {
        return TE_RC(TE_TA_UNIX,  TE_ENOENT);
    }

    SLIST_REMOVE(&l2tp_option->l2tp_range, l2tp_range,
                 te_l2tp_ipv4_range, list);

    free(l2tp_range->start);
    free(l2tp_range->end);

    l2tp->changed = TRUE;
    return 0;
}

/**
 * Method for deleting an option from /agent/l2tp/lns/ip_range
 *
 * @param gid                  group identifier
 * @param oid                  full identifier of the father instance
 * @param l2pt_name            name of the l2tp instance is always empty
 * @param lns_name             name of the lns instance
 * @param range                name of the ../ip_range instances
 *                             to delete
 *                             like "192.168.37.38-192.168.37.40"
 *
 * @return status code
 */
static te_errno
l2tp_lns_ip_range_del(unsigned int gid, const char *oid,
                      const char *l2tp_name, const char *lns_name,
                      const char *range)
{
    UNUSED(gid);
    UNUSED(oid);
    UNUSED(l2tp_name);

    return l2tp_lns_range_del_routine(lns_name, "ip range", range);
}

/**
 * Method for deleting an option from /agent/l2tp/lns/lac_range
 *
 * @param gid                  group identifier
 * @param oid                  full identifier of the father instance
 * @param l2pt_name            name of the l2tp instance is always empty
 * @param lns_name             name of the lns instance
 * @param range                name of the ../lac_range instances
 *                             to delete
 *                             like "192.168.37.38-192.168.37.40"
 *
 * @return status code
 */
static te_errno
l2tp_lns_lac_range_del(unsigned int gid, const char *oid,
                       const char *l2tp_name, const char *lns_name,
                       const char *range)
{
    UNUSED(gid);
    UNUSED(oid);
    UNUSED(l2tp_name);

    return l2tp_lns_range_del_routine(lns_name, "lac range", range);
}

/**
 * Method for adding an option to /agent/l2tp/lns/pppopt/option
 *
 * @param gid                  group identifier
 * @param oid                  full identifier of the father instance
 * @param value                unused value
 * @param l2tp_name            name of the l2tp instance is always empty
 * @param lns_name             name of the lns instance
 * @param pppopt               name of the pppopt instance is always empty
 * @param option               name of the option instance to add
 *
 * @return status code
 */
static te_errno
l2tp_lns_pppopt_add(unsigned int gid, const char *oid, const char *value,
                    const char *l2tp_name, const char *lns_name,
                    const char *pppopt, const char *option)
{
    te_l2tp_option  *l2tp_option;
    te_l2tp_server  *l2tp = l2tp_server_find();
    te_l2tp_section *l2tp_section = l2tp_find_section(l2tp, lns_name);

    UNUSED(gid);
    UNUSED(oid);
    UNUSED(value);
    UNUSED(l2tp_name);
    UNUSED(pppopt);

    if (l2tp_section == NULL)
        return TE_RC(TE_TA_UNIX,  TE_ENOENT);

    if ((l2tp_option = l2tp_find_option(l2tp, lns_name, PPP_OPTIONS)) == NULL)
    {
        l2tp_pppopt_alloc(&l2tp_option);
        SLIST_INSERT_HEAD(&l2tp_section->l2tp_option, l2tp_option, list);
    }

    else if ((l2tp_option = l2tp_find_option(l2tp, lns_name, option)) != NULL)
    {
        return TE_RC(TE_TA_UNIX,  TE_EEXIST);
    }
    l2tp_option = (te_l2tp_option *)calloc(1, sizeof(te_l2tp_option));
    if (l2tp_option == NULL)
        return TE_RC(TE_TA_UNIX, TE_ENOMEM);

    l2tp_option->name = strdup(option);
    l2tp_option->value = NULL;
    l2tp_option->type = L2TP_OPTION_TYPE_PPP;
    SLIST_INSERT_HEAD(&l2tp_section->l2tp_option, l2tp_option, list);
    l2tp->changed = TRUE;
    return 0;
}

/**
 * Method for deleting an option from /agent/l2tp/lns/pppopt/option
 *
 * @param gid                  group identifier
 * @param oid                  full identifier of the father instance
 * @param value                unused value
 * @param l2tp_name            name of the l2tp instance is always empty
 * @param lns_name             name of the lns instance
 * @param pppopt               name of the pppopt instance is always empty
 * @param option               name of the option instance to delete
 *
 * @return status code
 */
static te_errno
l2tp_lns_pppopt_del(unsigned int gid, const char *oid,
                    const char *l2tp_name, const char *lns_name,
                    const char *pppopt, const char *option)
{
    te_l2tp_server  *l2tp = l2tp_server_find();
    te_l2tp_section *l2tp_section = l2tp_find_section(l2tp, lns_name);
    te_l2tp_option  *l2tp_option  = l2tp_find_option(l2tp, lns_name, option);

    UNUSED(gid);
    UNUSED(oid);
    UNUSED(pppopt);
    UNUSED(l2tp_name);

    if (l2tp_section == NULL || l2tp_option == NULL)
        return TE_RC(TE_TA_UNIX,  TE_ENOENT);

    SLIST_REMOVE(&l2tp_section->l2tp_option, l2tp_option,
                 te_l2tp_option, list);

    free(l2tp_option->name);
    free(l2tp_option->value);
    free(l2tp_option);
    l2tp->changed = TRUE;
    return 0;
}

/**
 * Method for obtaining the list of ppp options from
 * /agent/l2tp/lns/pppopt/option
 *
 * @param gid           group identifier
 * @param oid           full identifier of the father instance
 * @param list          location of the option list
 * @param l2tp_name     name of the l2tp instance is always empty
 * @param lns_name      name of the lns

 * @return status code
 */
static te_errno
l2tp_lns_pppopt_list(unsigned int gid, const char *oid,
                     char **list, const char *l2tp_name,
                     const char *lns_name)
{
    te_l2tp_server  *l2tp = l2tp_server_find();
    te_l2tp_section *l2tp_section = l2tp_find_section(l2tp, lns_name);
    te_l2tp_option  *l2tp_option;
    te_string        str = TE_STRING_INIT;
    uint32_t         list_size;

    UNUSED(gid);
    UNUSED(oid);
    UNUSED(l2tp_name);

    list_size = L2TP_SERVER_LIST_SIZE;
    if ((*list = (char *)calloc(1, list_size)) == NULL)
    {
        return TE_RC(TE_TA_UNIX, TE_ENOMEM);
    }
    SLIST_FOREACH(l2tp_option, &l2tp_section->l2tp_option, list)
    {
        if (l2tp_option->type == L2TP_OPTION_TYPE_PPP &&
            l2tp_option->value == NULL)
        {
            te_string_append(&str, "%s ", l2tp_option->name);
        }
    }
    *list = str.ptr;
    return 0;
}

/**
 * Method for adding a client to /agent/l2tp/lns/auth/client
 *
 * @param gid           group identifier
 * @param oid           full identifier of the father instance
 * @param value         unused value
 * @param l2tp_name     name of the /client: instance
 * @param lns_name      name of the lns instance
 * @param auth_type     name of the lns auth instance
 * @param client_name   client name of the lns instance
 *
 * @return status code
 */
static te_errno
l2tp_lns_client_add(unsigned int gid, const char *oid, const char *value,
                    const char *l2tp_name, const char *lns_name,
                    const char *auth_type, const char *client_name)
{
    te_l2tp_secret       *client = NULL;
    te_l2tp_server       *l2tp = l2tp_server_find();
    te_l2tp_section      *section = l2tp_find_section(l2tp, lns_name);

    enum l2tp_secret_prot type = strcmp(auth_type, "chap") == 0 ?
                                 L2TP_SECRET_PROT_CHAP : L2TP_SECRET_PROT_PAP;

    te_l2tp_opt_auth     *opt_auth = &section->l2tp_opt_auth[type];

    UNUSED(oid);
    UNUSED(gid);
    UNUSED(value);
    UNUSED(l2tp_name);

    if ((client = l2tp_find_client(l2tp, lns_name, client_name, type)) != NULL)
    {
        return TE_RC(TE_TA_UNIX,  TE_EEXIST);
    }

    client = (te_l2tp_secret *)calloc(1, sizeof(te_l2tp_option));
    if (client == NULL)
        return TE_RC(TE_TA_UNIX, TE_ENOMEM);
    client->name = strdup(client_name);
    client->secret = strdup("*");
    client->sipv4 = strdup("*");
    client->server = strdup("*");

    SLIST_INSERT_HEAD(&opt_auth->secret, client, list);
    l2tp->changed = TRUE;
    return 0;
}

/**
 * Method for deleting a client from /agent/l2tp/lns/auth/client
 *
 * @param gid           group identifier
 * @param oid           full identifier of the father instance
 * @param l2tp_name     name of the /client: instance
 * @param lns_name      name of the lns instance
 * @param auth_type     name of the lns auth instance
 * @param client_name   client name of the lns instance
 * @param secret_part   client name of the lns instance
 *
 * @return status code
 */
static te_errno
l2tp_lns_client_del(unsigned int gid, const char *oid,
                    const char *l2tp_name, const char *lns_name,
                    const char *auth_type, const char *client_name)
{

    te_l2tp_secret        *client = NULL;
    te_l2tp_server        *l2tp = l2tp_server_find();
    te_l2tp_section       *section = l2tp_find_section(l2tp, lns_name);
    enum l2tp_secret_prot  type = strcmp(auth_type, "chap") == 0 ?
                                  L2TP_SECRET_PROT_CHAP : L2TP_SECRET_PROT_PAP;
    te_l2tp_opt_auth      *opt_auth = &section->l2tp_opt_auth[type];

    UNUSED(gid);
    UNUSED(oid);
    UNUSED(l2tp_name);

    if ((client = l2tp_find_client(l2tp, lns_name, client_name, type)) == NULL)
    {
        return TE_RC(TE_TA_UNIX,  TE_ENOENT);
    }

    SLIST_REMOVE(&opt_auth->secret, client, te_l2tp_secret, list);

    l2tp->changed = TRUE;
    free(client->name);
    free(client->server);
    free(client->sipv4);
    free(client->secret);

    free(client);

    return 0;
}

/**
 * Method for obtaining the list of /agent/l2tp/lns/auth/client.
 *
 * @param gid           group identifier
 * @param oid           full identifier of the father instance
 * @param list          location of the lns sections list
 * @param l2tp_name     name of the l2tp instance is always empty
 * @param lns_name      name of the lns instance

 * @return status code
 */
static te_errno
l2tp_lns_client_list(unsigned int gid, const char *oid,
                     char **list, const char *l2tp_name, const char *lns_name,
                     const char *auth_type)
{
    te_l2tp_server  *l2tp = l2tp_server_find();
    te_l2tp_section *l2tp_section = l2tp_find_section(l2tp, lns_name);
    te_l2tp_secret  *l2tp_client;
    enum l2tp_secret_prot  type = strcmp(auth_type, "chap") == 0 ?
                                  L2TP_SECRET_PROT_CHAP : L2TP_SECRET_PROT_PAP;
    te_l2tp_opt_auth      *opt_auth = &l2tp_section->l2tp_opt_auth[type];

    te_string        str = TE_STRING_INIT;
    uint32_t         list_size;

    UNUSED(gid);
    UNUSED(oid);
    UNUSED(l2tp_name);

    list_size = L2TP_SERVER_LIST_SIZE;
    if ((*list = (char *)calloc(1, list_size)) == NULL)
    {
        return TE_RC(TE_TA_UNIX, TE_ENOMEM);
    }
    SLIST_FOREACH(l2tp_client, &opt_auth->secret, list)
    {
        te_string_append(&str, "%s ", l2tp_client->name);
    }
    *list = str.ptr;
    return 0;
}

/**
 * Method for adding an auth instance from /agent/l2tp/lns/auth
 *
 * @param gid                  group identifier
 * @param oid                  full identifier of the father instance
 * @param value                unused value
 * @param l2tp_name            name of the l2tp instance is always empty
 * @param lns_name             name of the lns instance
 * @param auth_type            name of authentication
 *
 * @return status code
 */
static te_errno
l2tp_lns_auth_add(unsigned int gid, const char *oid, const char *value,
                  const char *l2tp_name, const char *lns_name,
                  const char *auth_type)
{
    te_l2tp_server  *l2tp = l2tp_server_find();
    te_l2tp_section *l2tp_section = l2tp_find_section(l2tp, lns_name);
    enum l2tp_secret_prot  type = strcmp(auth_type, "chap") == 0 ?
                                  L2TP_SECRET_PROT_CHAP : L2TP_SECRET_PROT_PAP;
    te_l2tp_opt_auth     *opt_auth = &l2tp_section->l2tp_opt_auth[type];

    UNUSED(gid);
    UNUSED(oid);
    UNUSED(value);
    UNUSED(l2tp_name);
    if (opt_auth->policy != L2TP_AUTH_POLICY_NONE)
    {
        return TE_RC(TE_TA_UNIX,  TE_EEXIST);
    }
    opt_auth->is_enabled = TRUE;

    return 0;
}

/**
 * Method for deleting an auth instance from /agent/l2tp/lns/auth
 *
 * @param gid                  group identifier
 * @param oid                  full identifier of the father instance
 * @param value                unused value
 * @param l2tp_name            name of the l2tp instance is always empty
 * @param lns_name             name of the lns instance
 * @param auth_type            name of authentication
 *
 * @return status code
 */
static te_errno
l2tp_lns_auth_del(unsigned int gid, const char *oid, const char *l2tp_name,
                  const char *lns_name, const char *auth_type)
{
    te_l2tp_server  *l2tp = l2tp_server_find();
    te_l2tp_section *l2tp_section = l2tp_find_section(l2tp, lns_name);
    enum l2tp_secret_prot  type = strcmp(auth_type, "chap") == 0 ?
                                  L2TP_SECRET_PROT_CHAP : L2TP_SECRET_PROT_PAP;
    te_l2tp_opt_auth     *opt_auth = &l2tp_section->l2tp_opt_auth[type];
    UNUSED(oid);
    UNUSED(gid);
    UNUSED(l2tp_name);

    opt_auth->policy = L2TP_AUTH_POLICY_NONE;

    return 0;
}

/**
 * Method for obtaining the list of /agent/l2tp/lns/auth.
 *
 * @param gid           group identifier
 * @param oid           full identifier of the father instance
 * @param list          location of the lns sections list
 * @param l2tp_name     name of the l2tp instance is always empty
 * @param lns_name      name of the lns instance

 * @return status code
 */
static te_errno
l2tp_lns_auth_list(unsigned int gid, const char *oid,
                   char **list, const char *l2tp_name,
                   const char *lns_name)
{
    te_l2tp_server        *l2tp = l2tp_server_find();
    te_l2tp_section       *l2tp_section = l2tp_find_section(l2tp, lns_name);
    te_l2tp_opt_auth      *l2tp_opt_auth;
    enum l2tp_secret_prot  type_iter;
    te_string              str = TE_STRING_INIT;
    uint32_t               list_size;

    UNUSED(gid);
    UNUSED(oid);
    UNUSED(l2tp_name);

    list_size = L2TP_SECRETS_LENGTH;

    if ((*list = (char *)calloc(1, list_size)) == NULL)
    {
        return TE_RC(TE_TA_UNIX, TE_ENOMEM);
    }
    for (type_iter = L2TP_SECRET_PROT_CHAP;
         type_iter < L2TP_SECRET_PROT_NTYPES; type_iter++)
    {
        l2tp_opt_auth = &l2tp_section->l2tp_opt_auth[type_iter];
        if (l2tp_opt_auth->is_enabled)
        {
            te_string_append(&str, "%s ", type_iter == L2TP_SECRET_PROT_CHAP
                                          ? "chap" : "pap");
        }
    }
    *list = str.ptr;
    return 0;
}

/**
 * Get secret routine
 *
 * @param lns_name      name of the lns instance
 * @param auth_type     name of the lns auth instance
 * @param client_name   client name of the lns instance
 * @param secret        returning secret
 * @param secret_type   type which specifies the secret's part
 *
 * @return status code
 */
static te_errno
l2tp_lns_secret_get_routine(const char *lns_name, const char *auth_type,
                            const char *client_name, char *secret,
                            enum l2tp_secret_type secret_type)
{
    te_l2tp_secret       *client;
    te_l2tp_server       *l2tp = l2tp_server_find();

    enum l2tp_secret_prot type = strcmp(auth_type, "chap") == 0 ?
                                 L2TP_SECRET_PROT_CHAP : L2TP_SECRET_PROT_PAP;

    if ((client = l2tp_find_client(l2tp, lns_name, client_name, type)) == NULL)
    {
        return TE_RC(TE_TA_UNIX,  TE_ENOENT);
    }

    switch (secret_type)
    {
        case L2TP_SECRET_TYPE_SEC:
        {
            strcpy(secret, client->secret);
            return 0;
        }

        case L2TP_SECRET_TYPE_IPV4:
        {
            strcpy(secret, strcmp(client->sipv4, "*") == 0 ?
                           "0.0.0.0" : client->sipv4);
            return 0;
        }

        case L2TP_SECRET_TYPE_SERV:
        {
            strcpy(secret, client->server);
            return 0;
        }

        default:
        {
            ERROR("Error in secret getting");
            return TE_RC(TE_TA_UNIX, TE_ENOENT);
        }
    }
}

/**
 * Get method for /agent/l2tp/lns/auth/client/secret
 *
 * @param gid           group identifier
 * @param oid           full identifier of the father instance
 * @param secret        secret to get
 * @param l2tp_name     name of the /client: instance
 * @param lns_name      name of the lns instance
 * @param auth_type     name of the lns auth instance
 * @param client_name   client name of the lns instance
 *
 * @return status code
 */
static te_errno
l2tp_lns_secret_get_sec(unsigned int gid, const char *oid, char *secret,
                        const char *l2tp_name, const char *lns_name,
                        const char *auth_type, const char *client_name)
{

    UNUSED(oid);
    UNUSED(gid);
    UNUSED(l2tp_name);

    return l2tp_lns_secret_get_routine(lns_name, auth_type,
                                       client_name, secret, L2TP_SECRET_TYPE_SEC);
}

/**
 * Get method for /agent/l2tp/lns/auth/client/secret
 *
 * @param gid           group identifier
 * @param oid           full identifier of the father instance
 * @param server        server to get
 * @param l2tp_name     name of the /client: instance
 * @param lns_name      name of the lns instance
 * @param auth_type     name of the lns auth instance
 * @param client_name   client name of the lns instance
 *
 * @return status code
 */
static te_errno
l2tp_lns_secret_get_serv(unsigned int gid, const char *oid, char *serv,
                         const char *l2tp_name, const char *lns_name,
                         const char *auth_type, const char *client_name)
{

    UNUSED(oid);
    UNUSED(gid);
    UNUSED(l2tp_name);

    return l2tp_lns_secret_get_routine(lns_name, auth_type,
                                       client_name, serv, L2TP_SECRET_TYPE_SERV);
}

/**
 * Get method for /agent/l2tp/lns/auth/client/secret
 *
 * @param gid           group identifier
 * @param oid           full identifier of the father instance
 * @param ipv4          ipv4 to get
 * @param l2tp_name     name of the /client: instance
 * @param lns_name      name of the lns instance
 * @param auth_type     name of the lns auth instance
 * @param client_name   client name of the lns instance
 *
 * @return status code
 */
static te_errno
l2tp_lns_secret_get_ipv4(unsigned int gid, const char *oid, char *ipv4,
                         const char *l2tp_name, const char *lns_name,
                         const char *auth_type, const char *client_name)
{

    UNUSED(oid);
    UNUSED(gid);
    UNUSED(l2tp_name);

    return l2tp_lns_secret_get_routine(lns_name, auth_type,
                                       client_name, ipv4, L2TP_SECRET_TYPE_IPV4);
}

/**
 * Set secret routine
 *
 * @param lns_name      name of the lns instance
 * @param auth_type     name of the lns auth instance
 * @param client_name   client name of the lns instance
 * @param secret_part   secret to set
 * @param secret_type   type of the modifying secret
 *
 * @return status code
 */
static te_errno
l2tp_lns_secret_set_routine(const char *lns_name, const char *auth_type,
                            const char *client_name, const char *secret_part,
                            enum l2tp_secret_type secret_type)
{
    te_l2tp_secret       *client;
    te_l2tp_server       *l2tp = l2tp_server_find();

    enum l2tp_secret_prot type = strcmp(auth_type, "chap") == 0 ?
                                 L2TP_SECRET_PROT_CHAP : L2TP_SECRET_PROT_PAP;
    if ((client = l2tp_find_client(l2tp, lns_name, client_name, type)) == NULL)
    {
        return TE_RC(TE_TA_UNIX,  TE_ENOENT);
    }

    switch (secret_type)
    {
        case L2TP_SECRET_TYPE_SEC:
        {
            free(client->secret);
            client->secret = strdup(secret_part);
            return 0;
        }

        case L2TP_SECRET_TYPE_IPV4:
        {
            free(client->sipv4);
            client->sipv4 = strdup(strcmp(secret_part, "0.0.0.0") == 0 ?
                                   "*" : secret_part);
            return 0;
        }


        case L2TP_SECRET_TYPE_SERV:
        {
            free(client->server);
            client->server = strdup(secret_part);
            return 0;
        }

        default:
        {
            ERROR("Error in secret setting");
            return TE_RC(TE_TA_UNIX,  TE_ENOENT);
        }
    }

}

/**
 * Set method for /agent/l2tp/lns/auth/client/secret
 *
 * @param gid           group identifier
 * @param oid           full identifier of the father instance
 * @param secret        secret to set
 * @param l2tp_name     name of the /client: instance
 * @param lns_name      name of the lns instance
 * @param auth_type     name of the lns auth instance
 * @param client_name   client name of the lns instance
 *
 * @return status code
 */
static te_errno
l2tp_lns_secret_set_sec(unsigned int gid, const char *oid, char *secret,
                        const char *l2tp_name, const char *lns_name,
                        const char *auth_type, const char *client_name)
{

    UNUSED(oid);
    UNUSED(gid);
    UNUSED(l2tp_name);

    return l2tp_lns_secret_set_routine(lns_name, auth_type,
                                       client_name, secret,
                                       L2TP_SECRET_TYPE_SEC);
}

/**
 * Set method for /agent/l2tp/lns/auth/client/server
 *
 * @param gid           group identifier
 * @param oid           full identifier of the father instance
 * @param server        server to set
 * @param l2tp_name     name of the /client: instance
 * @param lns_name      name of the lns instance
 * @param auth_type     name of the lns auth instance
 * @param client_name   client name of the lns instance
 *
 * @return status code
 */
static te_errno
l2tp_lns_secret_set_serv(unsigned int gid, const char *oid, const char *server,
                         const char *l2tp_name, const char *lns_name,
                         const char *auth_type, const char *client_name)
{
    UNUSED(oid);
    UNUSED(gid);
    UNUSED(l2tp_name);

    return l2tp_lns_secret_set_routine(lns_name, auth_type,
                                       client_name, server,
                                       L2TP_SECRET_TYPE_SERV);
}

/**
 * Set method for /agent/l2tp/lns/auth/client/ipv4
 *
 * @param gid           group identifier
 * @param oid           full identifier of the father instance
 * @param ipv4          ipv4 to set
 * @param l2tp_name     name of the /client: instance
 * @param lns_name      name of the lns instance
 * @param auth_type     name of the lns auth instance
 * @param client_name   client name of the lns instance
 *
 * @return status code
 */
static te_errno
l2tp_lns_secret_set_ipv4(unsigned int gid, const char *oid, const char *ipv4,
                         const char *l2tp_name, const char *lns_name,
                         const char *auth_type, const char *client_name)
{
    UNUSED(oid);
    UNUSED(gid);
    UNUSED(l2tp_name);

    return l2tp_lns_secret_set_routine(lns_name, auth_type,
                                       client_name, ipv4,
                                       L2TP_SECRET_TYPE_IPV4);
}


/** Check address belong to any ip range
 *
 *  @param l2tp     l2tp server structure
 *  @param cip      address for checking
 *
 *  @return         TRUE if belong
                    otherwise FALSE
 */
static te_bool
te_l2tp_check_accessory(te_l2tp_server *l2tp, char *cip)
{
    te_l2tp_option     *option;
    te_l2tp_ipv4_range *range;
    te_l2tp_section    *section;
    struct in_addr      start;
    struct in_addr      center;
    struct in_addr      end;

    SLIST_FOREACH(section, &l2tp->section, list)
    {
        SLIST_FOREACH(option, &section->l2tp_option, list)
        {
            if (strcmp(option->name, "ip range") == 0)
            {
                SLIST_FOREACH(range, &option->l2tp_range, list)
                {
                    inet_aton(cip, &center);
                    inet_aton(range->start, &start);
                    if (range->end != NULL)
                    {
                        inet_aton(range->end, &end);
                        if (ntohl(start.s_addr) <= ntohl(center.s_addr) &&
                            ntohl(center.s_addr) <= ntohl(end.s_addr))
                        {
                            return TRUE;
                        }
                    }
                    else if (ntohl(start.s_addr) == ntohl(center.s_addr))
                    {
                        return TRUE;
                    }
                }
            }
        }
    }
    return FALSE;
}

/**
 * Check the ip address is equal to any existing local ip
 *
 * @param local_ip   testing ip
 * @param ranges     array of ip ranges
 *
 * @return TRUE if local_ip is equal to
 *         local ip from ranges' array
 *         otherwise it returns FALSE
 */
static te_bool
l2tp_check_lns_ip(char *local_ip, te_l2tp_server *l2tp)
{
    te_l2tp_section  *section;
    te_l2tp_option   *opt;

    SLIST_FOREACH(section, &l2tp->section, list)
    {
        SLIST_FOREACH(opt, &section->l2tp_option, list)
        {
            if (strcmp(opt->name, "local ip") == 0 &&
                strcmp(opt->value, local_ip) == 0)
                return TRUE;
        }
    }
    return FALSE;
}

/**
 * Add connected clients to the te_l2tp_server.
 *
 * @return status code
 */
static te_errno
te_l2tp_clients_add(te_l2tp_server *l2tp)
{
    te_l2tp_connected *client;
    struct ifaddrs    *iter;
    struct ifaddrs    *perm;
    char               cip[INET6_ADDRSTRLEN];
    int                rc;

    if (getifaddrs(&perm))
    {
        rc = errno;
        ERROR("getifaddrs: %s", strerror(rc));
        return TE_OS_RC(TE_TA_UNIX, rc);
    }
    else
    {
        for (iter = perm; iter != NULL; iter = iter->ifa_next)
        {
            if (iter->ifa_addr && iter->ifa_addr->sa_family == AF_INET
                && l2tp_check_lns_ip(inet_ntoa(
                    ((struct sockaddr_in *) iter->ifa_addr)->sin_addr), l2tp))
            {
                inet_ntop(AF_INET, &((struct sockaddr_in *)
                                  iter->ifa_ifu.ifu_dstaddr)->sin_addr,
                          cip, INET_ADDRSTRLEN);
                client = (te_l2tp_connected *)
                        calloc(1, sizeof(te_l2tp_connected));
                if (client == NULL)
                    return TE_RC(TE_TA_UNIX, TE_ENOMEM);
                else if (te_l2tp_check_accessory(l2tp, cip))
                {
                    client->cname = strdup(cip);
                    SLIST_INSERT_HEAD(&l2tp->client, client, list);
                }
            }
        }
    }
    freeifaddrs(perm);
    l2tp->changed = TRUE;
    return 0;
}

/**
 * Method for obtaining the list of connected clients from
 * /agent/l2tp/lns/connected.
 *
 * @param gid           group identifier
 * @param oid           full identifier of the father instance
 * @param list          location of the connected clients
 * @param l2tp_name     name of the l2tp instance is always empty

 * @return status code
 */
static te_errno
l2tp_lns_connected_list(unsigned int gid, const char *oid,
                        char **list, const char *l2tp_name)
{
    te_l2tp_connected *l2tp_connected;
    uint32_t           list_size;
    te_string          str = TE_STRING_INIT;
    te_l2tp_server    *l2tp = l2tp_server_find();

    UNUSED(gid);
    UNUSED(oid);
    UNUSED(l2tp_name);

    if (te_l2tp_clients_add(l2tp) == 0)
    {
        list_size = L2TP_SERVER_LIST_SIZE;
        if ((*list = (char *)calloc(1, list_size)) == NULL)
        {
            return TE_RC(TE_TA_UNIX, TE_ENOMEM);
        }
        SLIST_FOREACH(l2tp_connected, &l2tp->client, list)
        {
            te_string_append(&str, "%s ", l2tp_connected->cname);
        }
        *list = str.ptr;
        return 0;
    }
    return TE_RC(TE_TA_UNIX, TE_ENOENT);
}


static rcf_pch_cfg_object node_l2tp;

static rcf_pch_cfg_object node_l2tp_lns_pppoption =
        { "option", 0, NULL, NULL, NULL, NULL,
          (rcf_ch_cfg_add)l2tp_lns_pppopt_add,
          (rcf_ch_cfg_del)l2tp_lns_pppopt_del,
          (rcf_ch_cfg_list)l2tp_lns_pppopt_list, NULL, &node_l2tp };

static rcf_pch_cfg_object node_l2tp_lns_einterval =
        { "lcp-echo-interval", 0, NULL, &node_l2tp_lns_pppoption,
          (rcf_ch_cfg_get)l2tp_lns_opt_get_lei,
          (rcf_ch_cfg_set)l2tp_lns_opt_set_lei,
          NULL, NULL, NULL, NULL, &node_l2tp };

static rcf_pch_cfg_object node_l2tp_lns_efailure =
        { "lcp-echo-failure", 0, NULL, &node_l2tp_lns_einterval,
          (rcf_ch_cfg_get)l2tp_lns_opt_get_lef,
          (rcf_ch_cfg_set)l2tp_lns_opt_set_lef,
          NULL, NULL, NULL, NULL, &node_l2tp };

static rcf_pch_cfg_object node_l2tp_lns_mru =
        { "mru", 0, NULL, &node_l2tp_lns_efailure,
          (rcf_ch_cfg_get)l2tp_lns_opt_get_mru,
          (rcf_ch_cfg_set)l2tp_lns_opt_set_mru,
          NULL, NULL, NULL, NULL, &node_l2tp };

static rcf_pch_cfg_object node_l2tp_lns_mtu =
        { "mtu", 0, NULL, &node_l2tp_lns_mru,
          (rcf_ch_cfg_get)l2tp_lns_opt_get_mtu,
          (rcf_ch_cfg_set)l2tp_lns_opt_set_mtu,
          NULL, NULL, NULL, NULL, &node_l2tp};

static rcf_pch_cfg_object node_l2tp_lns_ppp_debug =
        { "ppp_debug", 0, NULL, NULL,
          (rcf_ch_cfg_get)l2tp_lns_opt_get_debug,
          (rcf_ch_cfg_set)l2tp_lns_opt_set_debug,
          NULL, NULL, NULL, NULL, &node_l2tp };

static rcf_pch_cfg_object node_l2tp_lns_ppp =
        { "pppopt", 0, &node_l2tp_lns_mtu, &node_l2tp_lns_ppp_debug,
          NULL, NULL, NULL, NULL, NULL, NULL, &node_l2tp };

static rcf_pch_cfg_object node_l2tp_lns_unix_auth =
        { "unix_auth", 0, NULL, &node_l2tp_lns_ppp,
          (rcf_ch_cfg_get)l2tp_lns_opt_get_uauth,
          (rcf_ch_cfg_set)l2tp_lns_opt_set_uauth,
          NULL, NULL, NULL, NULL, &node_l2tp };

static rcf_pch_cfg_object node_l2tp_lns_challenge =
        { "use_challenge", 0, NULL, &node_l2tp_lns_unix_auth,
          (rcf_ch_cfg_get)l2tp_lns_opt_get_challenge,
          (rcf_ch_cfg_set)l2tp_lns_opt_set_challenge,
          NULL, NULL, NULL, NULL, &node_l2tp };

static rcf_pch_cfg_object node_l2tp_lns_sserver =
        { "server",  0, NULL, NULL,
          (rcf_ch_cfg_get)l2tp_lns_secret_get_serv,
          (rcf_ch_cfg_set)l2tp_lns_secret_set_serv,
          NULL, NULL, NULL, NULL, &node_l2tp };

static rcf_pch_cfg_object node_l2tp_lns_sipv4 =
        { "ipv4",  0, NULL, &node_l2tp_lns_sserver,
          (rcf_ch_cfg_get)l2tp_lns_secret_get_ipv4,
          (rcf_ch_cfg_set)l2tp_lns_secret_set_ipv4,
          NULL, NULL, NULL, NULL, &node_l2tp };

static rcf_pch_cfg_object node_l2tp_lns_ssecret =
        { "secret",  0, NULL, &node_l2tp_lns_sipv4,
          (rcf_ch_cfg_get)l2tp_lns_secret_get_sec,
          (rcf_ch_cfg_set)l2tp_lns_secret_set_sec,
          NULL, NULL, NULL, NULL, &node_l2tp };

static rcf_pch_cfg_object node_l2tp_lns_sclient =
        { "client", 0, &node_l2tp_lns_ssecret, NULL,
          NULL, NULL,
          (rcf_ch_cfg_add)l2tp_lns_client_add,
          (rcf_ch_cfg_del)l2tp_lns_client_del,
          (rcf_ch_cfg_list)l2tp_lns_client_list, NULL, &node_l2tp };

static rcf_pch_cfg_object node_l2tp_lns_refuse =
        { "refuse", 0, NULL, &node_l2tp_lns_sclient,
          (rcf_ch_cfg_get)l2tp_lns_refuse_get,
          (rcf_ch_cfg_set)l2tp_lns_refuse_set,
          NULL, NULL, NULL, NULL, &node_l2tp };

static rcf_pch_cfg_object node_l2tp_lns_require =
        { "require", 0, NULL, &node_l2tp_lns_refuse,
          (rcf_ch_cfg_get)l2tp_lns_require_get,
          (rcf_ch_cfg_set)l2tp_lns_require_set,
          NULL, NULL, NULL, NULL, &node_l2tp };

static rcf_pch_cfg_object node_l2tp_lns_auth =
        { "auth", 0, &node_l2tp_lns_require, &node_l2tp_lns_challenge,
          NULL, NULL,
          (rcf_ch_cfg_add)l2tp_lns_auth_add,
          (rcf_ch_cfg_del)l2tp_lns_auth_del,
          (rcf_ch_cfg_list)l2tp_lns_auth_list, NULL, &node_l2tp };

static rcf_pch_cfg_object node_l2tp_lns_bit =
        { "bit", 0, NULL, &node_l2tp_lns_auth,
          (rcf_ch_cfg_get)l2tp_lns_bit_get, NULL,
          (rcf_ch_cfg_add)l2tp_lns_bit_add,
          (rcf_ch_cfg_del)l2tp_lns_bit_del,
          (rcf_ch_cfg_list)l2tp_lns_bit_list, NULL, &node_l2tp };

static rcf_pch_cfg_object node_l2tp_lns_local_ip =
        { "local_ip", 0, NULL, &node_l2tp_lns_bit,
          (rcf_ch_cfg_get)l2tp_lns_local_ip_get,
          (rcf_ch_cfg_set)l2tp_lns_local_ip_set,
          NULL, NULL, NULL, NULL, &node_l2tp };

static rcf_pch_cfg_object node_l2tp_connected =
        { "connected", 0, NULL, &node_l2tp_lns_local_ip,
          NULL, NULL, NULL, NULL,
          (rcf_ch_cfg_list)l2tp_lns_connected_list,
          NULL, &node_l2tp };

static rcf_pch_cfg_object node_l2tp_lns_lac_range =
        { "lac_range", 0, NULL, &node_l2tp_connected,
          (rcf_ch_cfg_get)l2tp_lns_lac_range_get, NULL,
          (rcf_ch_cfg_add)l2tp_lns_lac_range_add,
          (rcf_ch_cfg_del)l2tp_lns_lac_range_del,
          (rcf_ch_cfg_list)l2tp_lns_lac_range_list, NULL, &node_l2tp };

static rcf_pch_cfg_object node_l2tp_lns_ip_range =
        { "ip_range", 0, NULL, &node_l2tp_lns_lac_range,
          (rcf_ch_cfg_get)l2tp_lns_ip_range_get, NULL,
          (rcf_ch_cfg_add)l2tp_lns_ip_range_add,
          (rcf_ch_cfg_del)l2tp_lns_ip_range_del,
          (rcf_ch_cfg_list)l2tp_lns_ip_range_list, NULL, &node_l2tp };

static rcf_pch_cfg_object node_l2tp_listen =
        { "listen", 0, NULL, NULL,
          (rcf_ch_cfg_get)l2tp_global_listen_get,
          (rcf_ch_cfg_set)l2tp_global_listen_set,
          NULL, NULL, NULL, NULL, &node_l2tp };

static rcf_pch_cfg_object node_l2tp_port =
        { "port", 0, NULL, &node_l2tp_listen,
          (rcf_ch_cfg_get)l2tp_global_port_get,
          (rcf_ch_cfg_set)l2tp_global_port_set,
          NULL, NULL, NULL, NULL, &node_l2tp };

static rcf_pch_cfg_object node_l2tp_lns =
        { "lns", 0, &node_l2tp_lns_ip_range, &node_l2tp_port,
          NULL, NULL,
          (rcf_ch_cfg_add)l2tp_lns_section_add,
          (rcf_ch_cfg_del)l2tp_lns_section_del,
          (rcf_ch_cfg_list)l2tp_lns_section_list, NULL, &node_l2tp };


static rcf_pch_cfg_object node_l2tp =
        { "l2tp", 0, &node_l2tp_lns, NULL,
          (rcf_ch_cfg_get)l2tp_server_get,
          (rcf_ch_cfg_set)l2tp_server_set,
          NULL, NULL, NULL,
          (rcf_ch_cfg_commit)l2tp_server_commit, NULL };

/**
 * Grab method for l2tp server resource
 *
 * @param name  dummy name of l2tp server
 *
 * @return status code
 */
te_errno
l2tp_grab(const char *name)
{
    te_l2tp_server *l2tp = l2tp_server_find();
    te_errno        retval = 0;

    UNUSED(name);

    ENTRY("grab l2tp server");

    if (access(L2TP_SERVER_EXEC, X_OK) != 0)
    {
        ERROR("No L2TP server executable was not found");
        return TE_RC(TE_TA_UNIX, TE_ENOENT);
    }

    if ((retval = rcf_pch_add_node("/agent", &node_l2tp)) != 0)
    {
        return retval;
    }

    if ((retval = l2tp_server_stop(l2tp)) != 0)
    {
        ERROR("Failed to stop L2TP server");
        rcf_pch_del_node(&node_l2tp);
        return retval;
    }

    l2tp->started = FALSE;

    return 0;
}

/**
 * Remove temporary ppp config files
 *
 * @return status code
 */
static te_errno
l2tp_remove_pppfile()
{
    DIR            *dir;
    struct dirent  *tmpdir;
    char            pppfilename[L2TP_MAX_OPTNAME_LENGTH];
    char           *template = "te.options.xl2tpd";
    int             rc;

    if ((dir = opendir("/tmp/")) != NULL)
    {
        while ((tmpdir = readdir(dir)) != 0)
        {
            if (strstr(tmpdir->d_name, template) != NULL)
            {
                TE_SPRINTF(pppfilename, "/tmp/%s", tmpdir->d_name);
                if (remove(pppfilename) != 0)
                {
                    ERROR("remove %s failed", tmpdir->d_name);
                    return TE_RC(TE_TA_UNIX, errno);
                }
            }
        }
    }
    else
    {
        rc = errno;
        ERROR("open /tmp/ dir failed: %s", strerror(rc));
        return TE_OS_RC(TE_TA_UNIX, rc);
    }
    return 0;
}

/**
 * Release method for l2tp server resource
 *
 * @param name  dummy name of l2tp server
 *
 * @return status code
 */
te_errno
l2tp_release(const char *name)
{
    te_l2tp_server *l2tp = l2tp_server_find();
    te_errno        retval;
    char            l2tp_conf[sizeof(L2TP_SERVER_CONF_BASIS)
                              + L2TP_MAX_PID_VALUE_LENGTH];

    UNUSED(name);

    ENTRY("release l2tp server");

    if ((retval = rcf_pch_del_node(&node_l2tp)) != 0)
        return retval;

    if ((retval = l2tp_server_stop(l2tp)) != 0)
    {
        ERROR("Failed to stop l2tp server");
        return retval;
    }

    if ((retval = l2tp_remove_pppfile() != 0))
    {
        ERROR("Failed to remove ppp options files");
        return retval;
    }
    TE_SPRINTF(l2tp_conf, L2TP_SERVER_CONF_BASIS ".%i", getpid());
    
    /*FIXME: there is no exist check of the files below*/
    if (!access(l2tp_conf, X_OK) && remove(l2tp_conf) != 0)
    {
        ERROR("remove %s failed", l2tp_conf);
        return TE_RC(TE_TA_UNIX, TE_ESHCMD);
    }
    
    if (l2tp_secrets_recover(L2TP_CHAP_SECRETS) != 0)
    {
        ERROR("recover L2TP_CHAP_SECRETS failed");
        return TE_RC(TE_TA_UNIX, TE_ESHCMD);
    }
    if (l2tp_secrets_recover(L2TP_PAP_SECRETS)!= 0)
    {
        ERROR("recover L2TP_PAP_SECRETS failed");
        return TE_RC(TE_TA_UNIX, TE_ESHCMD);
    }
    return 0;
}
