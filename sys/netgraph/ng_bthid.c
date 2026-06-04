#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>

#include <netgraph/ng_message.h>
#include <netgraph/ng_bthid.h>
#include <netgraph/netgraph.h>

#define M_NETGRAPH_BTHID M_NETGRAPH

static ng_constructor_t ng_bthid_constructor;
static ng_shutdown_t 	ng_bthid_shutdown;

struct bthid {
	node_p		node; // Pointer to self
};
typedef struct bthid *bthid_p;

static struct ng_type typestruct = {
	.version = 	NG_ABI_VERSION,
	.name = 	NG_BTHID_NODE_TYPE,
	.constructor = 	ng_bthid_constructor,
	.shutdown = 	ng_bthid_shutdown,
};

NETGRAPH_INIT(bthid, &typestruct);

static int
ng_bthid_constructor(node_p node)
{
	bthid_p privdata;
	privdata = malloc(sizeof(*privdata), M_NETGRAPH, M_WAITOK | M_ZERO);

	NG_NODE_SET_PRIVATE(node, privdata);
	privdata->node = node;
	return(0);
}

static int
ng_bthid_shutdown(node_p node)
{
	const bthid_p privdata = NG_NODE_PRIVATE(node);
	NG_NODE_SET_PRIVATE(node, NULL);
	free(privdata, M_NETGRAPH);
	return 0;
}

