extern int splitTunneling;

typedef struct _SplitTunnelingRequest {
    int local;
    int (*handler)(int, struct _SplitTunnelingRequest*);
    void *data;
}  SplitTunnelingRequestRec, *SplitTunnelingRequestPtr;

typedef struct _Network {
    struct in_addr network;
    struct in_addr netmask;
    int maskbits;
} NetworkRec, *NetworkPtr;

typedef struct _NetworkList {
    int size;
    int used;
    NetworkPtr *networks;
} NetworkList;

NetworkList*  makeNetworkList(int size);
void networkListCons(NetworkPtr nw, NetworkList* list);
void preinitSplitTunneling(void);
void initSplitTunneling(void);
int do_split_tunneling(int (*)(int, SplitTunnelingRequestPtr), void*);
int splitSocksConnectDnsHandler(int, SocksRequestPtr);
int splitSocksWriteDnsHandler(int, FdEventHandlerPtr, StreamRequestPtr);
int splitSocksReadDnsHandler(int, FdEventHandlerPtr, StreamRequestPtr);
int cmpNetworks(const void *a, const void *b);
void removeDups(NetworkList** list);
int addressInNetwork( const void * va, const void * vb);
int isLocalAddress(struct in_addr  addr);
void testNetworkList();
