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


struct preemptive_listen_stop_ctx {
    tree234** pp_portfwds;
};


static void preemptive_listen_stop_timer(void* void_ctx, long now)
{
    struct preemptive_listen_stop_ctx* ctx = (struct preemptive_listen_stop_ctx*)void_ctx;
    
    char ss[100];
    sprintf(ss, "preemptive_listen_stop_timer: now: %d\n", now);
    OutputDebugString(ss);

    /* Taken from ssh.c ~2950 */
    if (ctx->pp_portfwds) {
        struct ssh_portfwd *pf;
        while (NULL != (pf = (struct ssh_portfwd*)index234(*ctx->pp_portfwds, 0))) {
        /* Dispose of any listening socket. */
        if (pf->local)
        pfd_terminate(pf->local);
        del234(*ctx->pp_portfwds, pf); /* moving next one to index 0 */
        free_portfwd(pf);
        }
        freetree234(*ctx->pp_portfwds);
        *ctx->pp_portfwds = NULL;
    }
}


static void setup_preemptive_listen_stop(tree234** pp_portfwds)
{
    int next;
    char ss[100];

    // TODO: Don't memleak this. Or maybe it doesn't matter.
    struct preemptive_listen_stop_ctx* ctx = snew(struct preemptive_listen_stop_ctx);
    ctx->pp_portfwds = pp_portfwds;

    next = schedule_timer(
            30 * TICKSPERSEC,
            preemptive_listen_stop_timer, 
            ctx);

    sprintf(ss, "setup_preemptive_listen_stop: next: %d\n", next);
    OutputDebugString(ss);
}


void do_psiphon_setup(tree234** pp_portfwds)
{
    setup_preemptive_listen_stop(pp_portfwds);
}
