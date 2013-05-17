/*
 * Copyright (c) 2013, Psiphon Inc.
 * All rights reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */


#include "putty.h"
#include "SSH.H"


#define CONNECTED_OUTPUT_MESSAGE    "PSIPHON:CONNECTED"
#define PORTFWD_STOP_INPUT_MESSAGE  "PSIPHON:PORTFWDSTOP"


/* copied from ssh.c */
struct ssh_portfwd {
    enum { DESTROY, KEEP, CREATE } status;
    int type;
    unsigned sport, dport;
    char *saddr, *daddr;
    char *sserv, *dserv;
    struct ssh_rportfwd *remote;
    int addressfamily;
    void *local;
};
#define free_portfwd(pf) ( \
    ((pf) ? (sfree((pf)->saddr), sfree((pf)->daddr), \
         sfree((pf)->sserv), sfree((pf)->dserv)) : (void)0 ), sfree(pf) )


static tree234** g_pp_portfwds = NULL;


int psiphon_stdin_gotdata(struct handle *h, void *data, int len)
{
    char* buf = NULL;
    BOOL portfwd_stop = FALSE;

    if (len < 0)
    {
        cleanup_exit(0);
        return 0;
    }
    else if (len == 0)
    {
        return 0;
    }

    buf = snewn(len+1, char);
    strncpy_s(buf, len+1, (char*)data, len);
    buf[len] = '\0';

    portfwd_stop = (strstr(buf, PORTFWD_STOP_INPUT_MESSAGE) != NULL);

    sfree(buf);

    if (!portfwd_stop)
    {
        return 0;
    }

    /* Terminate the port forwarding */
    /* Taken from ssh.c ~2950 */
    if (g_pp_portfwds && *g_pp_portfwds) {
        struct ssh_portfwd *pf;
        while (NULL != (pf = (struct ssh_portfwd*)index234(*g_pp_portfwds, 0))) {
            /* Dispose of any listening socket. */
            if (pf->local)
            pfd_terminate(pf->local);
            del234(*g_pp_portfwds, pf); /* moving next one to index 0 */
            free_portfwd(pf);
        }
        freetree234(*g_pp_portfwds);
        *g_pp_portfwds = NULL;
    }

    return 0;
}


void do_psiphon_setup(tree234** pp_portfwds)
{
    g_pp_portfwds = pp_portfwds;
}


const char* get_psiphon_connected_message()
{
    return CONNECTED_OUTPUT_MESSAGE;
}
