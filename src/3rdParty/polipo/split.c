#include "polipo.h"

static AtomPtr splitTunnelingFile = NULL;
static AtomPtr splitTunnelingDnsServer = NULL;
NetworkList* localNetworks = NULL;
int splitTunneling = 0;

static int parseSplitFile(AtomPtr filename);

void 
preinitSplitTunneling(void)
{
    CONFIG_VARIABLE(splitTunnelingDnsServer, CONFIG_ATOM_LOWER,
            "The name server to use with split tunneling over SOCKS.");
    CONFIG_VARIABLE(splitTunneling, CONFIG_BOOLEAN,
            "Use split tunneling");
    CONFIG_VARIABLE(splitTunnelingFile, CONFIG_ATOM, 
            "Local networks file for split tunneling");

    return;
}

void 
initSplitTunneling(void)
{
    int rc;
    if(splitTunneling && splitTunnelingFile)
    {
        rc = parseSplitFile(splitTunnelingFile);
        if(rc < 0)
            exit(1);
        int i;
        for(i=0; i< localNetworks->used; i++)
        {   

            NetworkPtr t = localNetworks->networks[i];
            struct in_addr test_network =  t->network;
            char* ip = inet_ntoa(test_network);
            do_log(L_WARN, "%s/%d\n", ip, t->maskbits);

        }   
        struct in_addr local;
        char * test_ip = "127.0.0.2";
        inet_aton(test_ip, &local);
        int n = isLocalAddress(local);
        do_log(L_WARN, "%s is %s the range\n", test_ip, n?"in":"not in");
    }

    return;
}

int 
do_split_tunneling(int (*handler)(int, SplitTunnelingRequestPtr), void *data)
{

    SplitTunnelingRequestPtr request = malloc(sizeof(SplitTunnelingRequestRec));

    request->data = data;
    request->handler = handler;
    request->local = 1;

    //handler(1, request);
    //return 1;
    //connect to socks parent proxy and
    //do DNS over TCP
    int dns_port = 80;
    do_socks_connect(atomString(splitTunnelingDnsServer), dns_port,
            splitSocksConnectDnsHandler,
            request);
    return 0;
}

int
splitSocksConnectDnsHandler(int status, SocksRequestPtr request)
{
    /*
       ./polipo proxyPort=8888 socksParentProxy=127.0.0.1:12345 diskCacheRoot="" disableLocalInterface=true logLevel=1 splitTunneling=true splitTunnelingDnsServer=8.8.8.8


    */
    /*
    HTTPConnectionPtr connection = request->data;

    assert(connection->fd < 0);
    if(request->fd >= 0) { 
        connection->fd = request->fd;
        connection->server->addrindex = 0; 
    }    
    return httpServerConnectionHandlerCommon(status, connection);
    */
    do_stream(IO_WRITE, request->fd, 0, "1\n\n", 3,
            splitSocksWriteDnsHandler, request);

    return 1;
}

int 
splitSocksWriteDnsHandler(int status,
        FdEventHandlerPtr event,
        StreamRequestPtr srequest)
{
    SocksRequestPtr request = srequest->data;

    if(status < 0)
        goto error;

    if(!streamRequestDone(srequest)) {
        if(status) {
            status = -ESOCKS_PROTOCOL;
            goto error;
        }
        return 0;
    }

    request->buf = malloc(10); 

    do_stream(IO_READ | IO_NOTNOW, request->fd, 0, request->buf, 8,
            splitSocksReadDnsHandler,
            request);
    return 1;

error:
    CLOSE(request->fd);
    request->fd = -1;
    request->handler(status, request);
    //destroySocksRequest(request);
    return 1;
}

int
splitSocksReadDnsHandler(int status,
                 FdEventHandlerPtr event,
                 StreamRequestPtr srequest)
{
    SocksRequestPtr request = srequest->data;

    CLOSE(request->fd);
    request->handler(status, request);
    //destroySocksRequest(request);
    return 1;
}

static int
parseSplitFile(AtomPtr filename)
{
    char buf[512], *cn, * cm;
    int rc;
    struct in_addr inamask, inanetwork;
    int  mask, i, maskbits;
    FILE *f;

    if(!filename || filename->length == 0)
        return 0;
    f = fopen(filename->string, "r");
    if(f == NULL) {
        do_log(L_ERROR, "Couldn't open split tunneling file %s: %d.\n",
               filename->string, errno);
        return -1;
    }

    while(fgets(buf, 512, f)) {
        mask = 0;
        cn = cm = NULL;

        cn = strtok(buf, " \t");
        cm = strtok(NULL, " \t"); 

        if(!cn || !cm)
            continue;

        rc = inet_aton(cn, &inanetwork);
        if(!rc) //read next line
            continue;

        rc = inet_aton(cm, &inamask);
        if(!rc) //read next line
            continue;

        /* Calculate the number of network bits */
        mask = ntohl(inamask.s_addr);
        for ( maskbits=32 ; (mask & (1L<<(32-maskbits))) == 0 ; maskbits-- )
            ;
        /* Re-create the netmask and compare to the origianl
         * to make sure it is a valid netmask.
         */
        mask = 0;
        for ( i=0 ; i<maskbits ; i++ )
            mask |= 1<<(31-i);
        if ( mask != ntohl(inamask.s_addr) )
            continue;

        if(localNetworks == NULL) {
            localNetworks = makeNetworkList(0);
            if(localNetworks == NULL) {
                do_log(L_ERROR, "Couldn't allocate NetworkList.\n");
                exit(1);
            }
        }
        NetworkRec nw;
        nw.network = inanetwork;
        nw.netmask = inamask;
        nw.maskbits = maskbits;
        networkListCons(&nw, localNetworks);
    }
    fclose(f);

    if(NULL == localNetworks)
        return 1;

    qsort(localNetworks->networks, localNetworks->used, sizeof(NetworkPtr), cmpNetworks);
    //remove duplicate networks from 
    //the array
    removeDups(&localNetworks);
    return 1;
}

int cmpNetworks(const void *a, const void *b)
{
    unsigned long numa, numb; 

    NetworkPtr ia = *(NetworkPtr *) a;
    NetworkPtr ib = *(NetworkPtr *) b;

    numa = ntohl(ia->network.s_addr);
    numb = ntohl(ib->network.s_addr);
    
    if(numa == numb) 
        return 0;
    if(numa > numb) 
        return 1;
    return -1;
}

NetworkList* 
makeNetworkList(int size)
{
    NetworkList *list;
    if(size <= 0)
        size = 4;

    list = malloc(sizeof(NetworkList));
    if(list == NULL)
        return NULL;

    list->networks = malloc(size * sizeof(NetworkPtr));
    if(list->networks == NULL) {
        free(list);
        return NULL;
    }

    list->used = 0;
    list->size = size;
    return list;
}


void 
networkListCons(NetworkPtr network, NetworkList *list)
{
    if(list->networks == NULL) {
        assert(list->used == 0);
    }
    if(list->size <= list->used) {
        NetworkPtr *new_networks;
        int n = (2 * list->used);
        new_networks = realloc(list->networks, n * sizeof(NetworkPtr));
        if(new_networks == NULL) {
            do_log(L_ERROR, "Couldn't realloc NetworkList\n");
            return;
        }
        list->networks = new_networks;
        list->size = n;
    }

    NetworkPtr nw;
    nw = malloc(sizeof(NetworkRec));
    memcpy(nw, network, sizeof(NetworkRec));

    list->networks[list->used] = nw;
    list->used++;
}

void 
removeDups(NetworkList** listPtr)
{
    NetworkList* list = *listPtr;
    if(list->networks == NULL)
    {
        assert(list->used == 0);
        return;
    }
    if(list->used == 1)
        return;
    int k = 0;
    int i;
    for (i = 1; i < list->used; i++) 
    {
        //if (memcmp(list->networks[k]->data, list->networks[i]->data, 4)) 
        if (memcmp(&(list->networks[k]->network), &(list->networks[i]->network), sizeof(struct in_addr)))
        {              
            k++;
            if(k != i)
            {
                free(list->networks[k]);
                list->networks[k] = list->networks[i];
            }
        }
    }

    // The new array size..
    list->used = k+1;
}

int
isLocalAddress(struct in_addr addr)
{
    NetworkPtr *pNetwork;
    pNetwork = bsearch(&addr, localNetworks->networks, localNetworks->used, sizeof(NetworkPtr), addressInNetwork); 
    return pNetwork != NULL;
}

int
addressInNetwork( const void * va, const void * vb)
{
    struct in_addr addr = *(struct in_addr *)va;
    NetworkPtr b = *(NetworkPtr *) vb;

    struct in_addr network = b->network;
    struct in_addr netmask = b->netmask;

    if(ntohl(addr.s_addr) <  ntohl(network.s_addr)) 
        return -1;
    if((network.s_addr & netmask.s_addr) == (addr.s_addr & netmask.s_addr)) 
        return 0;
    return 1;
}
