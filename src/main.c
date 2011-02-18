/* File: main.c Author: Administrator Created on January 2, 2011, 8:22 AM */
#include "rumble.h"
#include "database.h"
#include "comm.h"
#include "private.h"
#define RUMBLE_INITIAL_THREADS  20
extern masterHandle *rumble_database_master_handle;

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void rumble_master_init(masterHandle *master) {
    master->smtp.cue_hooks = dvector_init();
    master->smtp.init_hooks = dvector_init();
    master->smtp.threads = dvector_init();
    master->smtp.handles = dvector_init();
    master->smtp.init = rumble_smtp_init;
    pthread_mutex_init(&master->smtp.mutex, 0);
    master->pop3.cue_hooks = dvector_init();
    master->pop3.init_hooks = dvector_init();
    master->pop3.threads = dvector_init();
    master->pop3.handles = dvector_init();
    master->pop3.init = rumble_pop3_init;
    pthread_mutex_init(&master->pop3.mutex, 0);
    master->imap.cue_hooks = dvector_init();
    master->imap.init_hooks = dvector_init();
    master->imap.threads = dvector_init();
    master->imap.handles = dvector_init();
    master->imap.init = rumble_imap_init;
    pthread_mutex_init(&master->imap.mutex, 0);
    master->_core.modules = dvector_init();
    master->_core.workers = dvector_init();
    master->_core.feed_hooks = dvector_init();
    master->_core.parser_hooks = dvector_init();
    master->_core.batv = dvector_init();
    rumble_database_master_handle = master;
    master->domains.list = dvector_init();
    master->domains.rrw = rumble_rw_init();
    master->mailboxes.rrw = rumble_rw_init();
    master->mailboxes.list = dvector_init();
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
int main(int argc, char **argv) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    int             x;
    pthread_t       *t;
    pthread_attr_t  attr;
    masterHandle    *master;
    dvector         *args = dvector_init();
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    master = (masterHandle *) malloc(sizeof(masterHandle));
    if (!master) merror();
    for (x = 0; x < argc; x++) {
        rumble_scan_flags(args, argv[x]);
    }

    if (rhdict(args, "--TEST")) {
        rumble_test();
        exit(EXIT_SUCCESS);
    }

    printf("Starting Rumble Mail Server (v/%u.%02u.%04u)\r\n", RUMBLE_MAJOR, RUMBLE_MINOR, RUMBLE_REV);
    srand(time(0));
    rumble_config_load(master, args);
    rumble_master_init(master);
    rumble_database_load(master);
    rumble_modules_load(master);
    rumble_database_update_domains();
    rumble_crypt_init(master);
    pthread_attr_init(&attr);
    printf("%-48s", "Launching core service...");
    t = (pthread_t *) malloc(sizeof(pthread_t));
    dvector_add(master->_core.workers, t);
    pthread_create(t, NULL, rumble_worker_init, master);
    printf("[OK]\n");
    if (rumble_config_int(master, "enablesmtp")) {

        /*~~*/
        int n;
        /*~~*/

        printf("%-48s", "Launching SMTP service...");
        master->smtp.socket = comm_init(master, rumble_config_str(master, "smtpport"));
        for (n = 0; n < RUMBLE_INITIAL_THREADS; n++) {
            t = (pthread_t *) malloc(sizeof(pthread_t));
            dvector_add(master->smtp.threads, t);
            pthread_create(t, &attr, master->smtp.init, master);
        }

        printf("[OK]\n");
    }

    if (rumble_config_int(master, "enablepop3")) {

        /*~~*/
        int n;
        /*~~*/

        printf("%-48s", "Launching POP3 service...");
        master->pop3.socket = comm_init(master, rumble_config_str(master, "pop3port"));
        for (n = 0; n < RUMBLE_INITIAL_THREADS; n++) {
            t = (pthread_t *) malloc(sizeof(pthread_t));
            dvector_add(master->pop3.threads, t);
            pthread_create(t, &attr, master->pop3.init, master);
        }

        printf("[OK]\n");
    }

    if (rumble_config_int(master, "enableimap4")) {

        /*~~*/
        int n;
        /*~~*/

        printf("%-48s", "Launching IMAP4 service...");
        master->imap.socket = comm_init(master, rumble_config_str(master, "imap4port"));
        for (n = 0; n < RUMBLE_INITIAL_THREADS; n++) {
            t = (pthread_t *) malloc(sizeof(pthread_t));
            dvector_add(master->imap.threads, t);
            pthread_create(t, &attr, master->imap.init, master);
        }

        printf("[OK]\n");
    }

    sleep(999999);
    return (EXIT_SUCCESS);
}
