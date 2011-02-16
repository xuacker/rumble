/*$T database.c GC 1.140 02/16/11 21:10:53 */

/*$6
 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 */

#include "rumble.h"
#include "private.h"
#include "database.h"
#include <stdarg.h>
masterHandle    *rumble_database_master_handle = 0;

/*
 =======================================================================================================================
    Database constructors and wrappers
 =======================================================================================================================
 */
void rumble_database_load(masterHandle *master) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    char    *dbpath = (char *) calloc(1, strlen(rumble_config_str(master, "datafolder")) + 32);
    char    *mailpath = (char *) calloc(1, strlen(rumble_config_str(master, "datafolder")) + 32);
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    if (!dbpath || !mailpath) merror();
    sprintf(dbpath, "%s/rumble.sqlite", rumble_config_str(master, "datafolder"));
    sprintf(mailpath, "%s/mail.sqlite", rumble_config_str(master, "datafolder"));
    printf("%-48s", "Loading database...");

    /* Domains and accounts */
    if (sqlite3_open(dbpath, (sqlite3 **) &master->_core.db)) {
        fprintf(stderr, "Can't open database <%s>: %s\n", dbpath, sqlite3_errmsg((sqlite3 *) master->_core.db));
        exit(EXIT_FAILURE);
    }

    /* Letters */
    if (sqlite3_open(mailpath, (sqlite3 **) &master->_core.mail)) {
        fprintf(stderr, "Can't open database <%s>: %s\n", mailpath, sqlite3_errmsg((sqlite3 *) master->_core.mail));
        exit(EXIT_FAILURE);
    }

    free(dbpath);
    free(mailpath);
    printf("[OK]\n");
}

/*
 =======================================================================================================================
    Wrapper for the SQL prepare statement
 =======================================================================================================================
 */
void *rumble_database_prepare(void *db, const char *statement, ...) {

    /*~~~~~~~~~~~~~~~~~~~~~~*/
    char        *sql,
                b;
    const char  *p,
                *op;
    char        injects[32];
    void        *returnObject;
    va_list     vl;
    int         rc;
    ssize_t     len = 0,
                strl = 0,
                at = 0;
    /*~~~~~~~~~~~~~~~~~~~~~~*/

    memset(injects, 0, 32);
    sql = (char *) calloc(1, 2048);
    if (!sql) merror();
    op = statement;
    for (p = strchr(statement, '%'); p != NULL; p = strchr(op, '%')) {
        strl = strlen(op) - strlen(p);
        strncpy((char *) (sql + len), op, strl);
        len += strl;
        sscanf((const char *) p, "%%%c", &b);
        if (b == '%') {
            strncpy((char *) (sql + len), "%", 1);
            len += 1;
        } else {
            strncpy((char *) (sql + len), "?", 1);
            len += 1;
            injects[at++] = b;
        }

        op = (char *) p + 2;
    }

    strl = strlen(op);
    strncpy((char *) (sql + len), op, strl);
#ifdef RUMBLE_USING_SQLITE3
    rc = sqlite3_prepare_v2((sqlite3 *) db, sql, -1, (sqlite3_stmt **) &returnObject, NULL);
    free(sql);
    if (rc != SQLITE_OK) return (0);
#endif
    va_start(vl, statement);
    for (at = 0; injects[at] != 0; at++) {
        switch (injects[at])
        {
        case 's':
#ifdef RUMBLE_USING_SQLITE3
            rc = sqlite3_bind_text((sqlite3_stmt *) returnObject, at + 1, va_arg(vl, const char *), -1, SQLITE_TRANSIENT);
#endif
            break;

        case 'u':
#ifdef RUMBLE_USING_SQLITE3
            rc = sqlite3_bind_int((sqlite3_stmt *) returnObject, at + 1, va_arg(vl, unsigned int));
#endif
            break;

        case 'i':
#ifdef RUMBLE_USING_SQLITE3
            rc = sqlite3_bind_int((sqlite3_stmt *) returnObject, at + 1, va_arg(vl, signed int));
#endif
            break;

        case 'l':
#ifdef RUMBLE_USING_SQLITE3
            rc = sqlite3_bind_int64((sqlite3_stmt *) returnObject, at + 1, va_arg(vl, signed int));
#endif
            break;

        case 'f':
#ifdef RUMBLE_USING_SQLITE3
            rc = sqlite3_bind_double((sqlite3_stmt *) returnObject, at + 1, va_arg(vl, double));
#endif
            break;

        default:
            break;
        }

#ifdef RUMBLE_USING_SQLITE3
        if (rc != SQLITE_OK) {
            va_end(vl);
            return (0);
        }
#endif
    }

    va_end(vl);
    return (returnObject);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void rumble_free_account(rumble_mailbox *user) {
    if (!user) return;
    if (user->arg) free(user->arg);
    if (user->user) free(user->user);
    if (user->hash) free(user->hash);
    if (user->domain) {
        if (user->domain->name) free(user->domain->name);
        if (user->domain->path) free(user->domain->path);
        free(user->domain);
    }

    user->arg = 0;
    user->domain = 0;
    user->user = 0;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
uint32_t rumble_account_exists(sessionHandle *session, const char *user, const char *domain) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    int             rc;
    void            *state;
    masterHandle    *master = (masterHandle *) session->_master;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    state = rumble_database_prepare(master->_core.db,
                                    "SELECT 1 FROM accounts WHERE domain = %s AND %s GLOB user ORDER BY LENGTH(user) DESC LIMIT 1", domain,
                                    user);
    rc = rumble_database_run(state);
    rumble_database_cleanup(state);
    return (rc == RUMBLE_DB_RESULT) ? 1 : 0;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
rumble_mailbox *rumble_account_data(sessionHandle *session, const char *user, const char *domain) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    int             rc;
    void            *state;
    char            *tmp;
    rumble_mailbox  *acc;
    masterHandle    *master = (masterHandle *) session->_master;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    state = rumble_database_prepare(master->_core.db,
                                    "SELECT id, domain, user, password, type, arg FROM accounts WHERE domain = %s AND %s GLOB user ORDER BY LENGTH(user) DESC LIMIT 1",
                                domain, user);
    rc = rumble_database_run(state);
    acc = NULL;
    if (rc == RUMBLE_DB_RESULT) {

        /*~~*/
        int l;
        /*~~*/

        acc = (rumble_mailbox *) malloc(sizeof(rumble_mailbox));
        if (!acc) merror();

        /* Account UID */
        acc->uid = sqlite3_column_int((sqlite3_stmt *) state, 0);

        /* Account Domain struct */
        l = sqlite3_column_bytes((sqlite3_stmt *) state, 1);
        acc->domain = rumble_domain_copy((const char *) sqlite3_column_text((sqlite3_stmt *) state, 1));

        /* Account Username */
        l = sqlite3_column_bytes((sqlite3_stmt *) state, 2);
        acc->user = (char *) calloc(1, l + 1);
        memcpy((char *) acc->user, sqlite3_column_text((sqlite3_stmt *) state, 2), l);

        /* Password (hashed) */
        l = sqlite3_column_bytes((sqlite3_stmt *) state, 3);
        acc->hash = (char *) calloc(1, l + 1);
        memcpy((char *) acc->hash, sqlite3_column_text((sqlite3_stmt *) state, 3), l);

        /* Account type */
        l = sqlite3_column_bytes((sqlite3_stmt *) state, 4);
        tmp = (char *) calloc(1, l + 1);
        if (!tmp) merror();
        memcpy((char *) tmp, sqlite3_column_text((sqlite3_stmt *) state, 4), l);
        rumble_string_lower(tmp);
        acc->type = RUMBLE_MTYPE_MBOX;
        if (!strcmp(tmp, "alias")) acc->type = RUMBLE_MTYPE_ALIAS;
        else if (!strcmp(tmp, "mod"))
            acc->type = RUMBLE_MTYPE_MOD;
        else if (!strcmp(tmp, "feed"))
            acc->type = RUMBLE_MTYPE_FEED;
        else if (!strcmp(tmp, "relay"))
            acc->type = RUMBLE_MTYPE_FEED;
        free(tmp);

        /* Account args */
        l = sqlite3_column_bytes((sqlite3_stmt *) state, 5);
        acc->arg = (char *) calloc(1, l + 1);
        memcpy((char *) acc->arg, sqlite3_column_text((sqlite3_stmt *) state, 5), l);
    }

    rumble_database_cleanup(state);
    return (acc);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
rumble_mailbox *rumble_account_data_auth(sessionHandle *session, const char *user, const char *domain, const char *pass) {

    /*~~~~~~~~~~~~~~~~~~*/
    rumble_mailbox  *acc;
    char            *hash;
    /*~~~~~~~~~~~~~~~~~~*/

    acc = rumble_account_data(session, user, domain);
    if (acc) {
        hash = rumble_sha256((const unsigned char *) pass);
        if (!strcmp(hash, acc->hash)) return (acc);
        rumble_free_account(acc);
        acc = 0;
    }

    return (acc);
}

/*
 =======================================================================================================================
    rumble_domain_exists: Checks if a domain exists in the database. Returns 1 if true, 0 if false.
 =======================================================================================================================
 */
uint32_t rumble_domain_exists(const char *domain) {

    /*~~~~~~~~~~~~~~~~~*/
    uint32_t        rc;
    rumble_domain   *dmn;
    /*~~~~~~~~~~~~~~~~~*/

    rc = 0;
    rumble_rw_start_read(rumble_database_master_handle->domains.rrw);
    for
    (
        dmn = (rumble_domain *) cvector_first(rumble_database_master_handle->domains.list);
        dmn != NULL;
        dmn = (rumble_domain *) cvector_next(rumble_database_master_handle->domains.list)
    ) {
        if (!strcmp(dmn->name, domain)) {
            rc = 1;
            break;
        }
    }

    rumble_rw_stop_read(rumble_database_master_handle->domains.rrw);
    return (rc);
}

/*
 =======================================================================================================================
    rumble_domain_copy: Returns a copy of the domain info
 =======================================================================================================================
 */
rumble_domain *rumble_domain_copy(const char *domain) {

    /*~~~~~~~~~~~~~~~~~*/
    rumble_domain   *dmn,
                    *rc;
    /*~~~~~~~~~~~~~~~~~*/

    rc = (rumble_domain *) malloc(sizeof(rumble_domain));
    rc->id = 0;
    rc->path = 0;
    rc->name = 0;
    rumble_rw_start_read(rumble_database_master_handle->domains.rrw);
    for
    (
        dmn = (rumble_domain *) cvector_first(rumble_database_master_handle->domains.list);
        dmn != NULL;
        dmn = (rumble_domain *) cvector_next(rumble_database_master_handle->domains.list)
    ) {
        if (!strcmp(dmn->name, domain)) {
            rc->name = (char *) calloc(1, strlen(dmn->name) + 1);
            rc->path = (char *) calloc(1, strlen(dmn->path) + 1);
            strcpy(rc->name, dmn->name);
            strcpy(rc->path, dmn->path);
            rc->id = dmn->id;
            break;
        }
    }

    rumble_rw_stop_read(rumble_database_master_handle->domains.rrw);
    return (rc);
}

/*
 =======================================================================================================================
    Internal database functions (not for use by modules) ;
    rumble_database_update_domains: Updates the list of domains from the db
 =======================================================================================================================
 */
void rumble_database_update_domains(void) {

    /*~~~~~~~~~~~~~~~~~~~~*/
    int             rc,
                    l;
    void            *state;
    rumble_domain   *domain;
    /*~~~~~~~~~~~~~~~~~~~~*/

    /* Clean up the old list */
    rumble_rw_start_write(rumble_database_master_handle->domains.rrw);
    for
    (
        domain = (rumble_domain *) cvector_first(rumble_database_master_handle->domains.list);
        domain != NULL;
        domain = (rumble_domain *) cvector_next(rumble_database_master_handle->domains.list)
    ) {
        free(domain->name);
        free(domain->path);
        free(domain);
    }

    cvector_flush(rumble_database_master_handle->domains.list);
    state = rumble_database_prepare(rumble_database_master_handle->_core.db, "SELECT id, domain, storagepath FROM domains WHERE 1");
    while ((rc = rumble_database_run(state)) == RUMBLE_DB_RESULT) {
        domain = (rumble_domain *) malloc(sizeof(rumble_domain));

        /* Domain ID */
        domain->id = sqlite3_column_int((sqlite3_stmt *) state, 0);

        /* Domain name */
        l = sqlite3_column_bytes((sqlite3_stmt *) state, 1);
        domain->name = (char *) calloc(1, l + 1);
        memcpy(domain->name, sqlite3_column_text((sqlite3_stmt *) state, 1), l);

        /* Optional domain specific storage path */
        l = sqlite3_column_bytes((sqlite3_stmt *) state, 2);
        domain->path = (char *) calloc(1, l + 1);
        memcpy(domain->path, sqlite3_column_text((sqlite3_stmt *) state, 2), l);
        cvector_add(rumble_database_master_handle->domains.list, domain);
    }

    rumble_rw_stop_write(rumble_database_master_handle->domains.rrw);
    rumble_database_cleanup(state);
}
