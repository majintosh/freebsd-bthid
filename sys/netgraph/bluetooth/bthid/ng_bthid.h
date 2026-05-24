/*
 * ng_bthid.h
 * ng_sample.h was used as reference
 */

#ifndef _NETGRAPH_NG_BTHID_H_
#define _NETGRAPH_NG_BTHID_H_

#define NG_BTHID_NODE_TYPE "bthid"

#define NGM_BTHID_COOKIE 1778154629

#define NG_BTHID_CTRL_HOOK "ctrl"
#define NG_BTHID_INTR_HOOK "intr"

// Node functions
enum {
	NGM_BTHID_SET_FLAG = 1,
	NGM_BTHID_GET_STATUS,
	NGM_BTHID_SET_RDESC, // "RDESC" is short for report descriptor
};

struct ngbthidstat {
	// Made these 64 bits rather than 32 to stave off overflow. Not sure if it's necessary though...
	u_int64_t packets_in;
	u_int64_t packets_out;
};

#define NG_BTHID_STATS_TYPE_INFO  {                               \
          { "packets_in",       &ng_parse_uint64_type   },      \
          { "packets_out",      &ng_parse_uint64_type   },      \
          { NULL }                                              \
}

#endif
