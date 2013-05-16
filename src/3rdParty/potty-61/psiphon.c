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


struct portfwd_stop_ctx {
    tree234** pp_portfwds;
};

static DWORD start_tick = 0;

static void portfwd_stop_timer(void* void_ctx, long now)
{
    struct portfwd_stop_ctx* ctx = (struct portfwd_stop_ctx*)void_ctx;
    
    printf("portfwd_stop_timer: elapsed: %ums\n", GetTickCount() - start_tick);

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


static void setup_portfwd_stop(Config* cfg, tree234** pp_portfwds)
{
    struct portfwd_stop_ctx* ctx;
    int next;

    printf("setup_portfwd_stop: cfg->portfwd_stop: %dms\n", cfg->portfwd_stop);
    start_tick = GetTickCount();

    if (cfg->portfwd_stop <= 0)
    {
        // Disabled
        return;
    }

    // TODO: Don't memleak this. Or maybe it doesn't matter.
    ctx = snew(struct portfwd_stop_ctx);
    ctx->pp_portfwds = pp_portfwds;

    /* We don't care about the return value (the next timer time) */
    next = schedule_timer(
            cfg->portfwd_stop,
            portfwd_stop_timer, 
            ctx);
}


void do_psiphon_setup(Config* cfg, tree234** pp_portfwds)
{
    setup_portfwd_stop(cfg, pp_portfwds);
}
