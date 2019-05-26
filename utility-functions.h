#include "contiki.h"
#include "rpl.h"
#include "rpl-private.h"
#include "contiki-lib.h"
#include "contiki-net.h"
//#include "net/routing/routing.h"
#include "net/rpl/rpl.h"
#include "net/netstack.h"
//#include "net/ipv6/simple-udp.h"
#include "net/ip/simple-udp.h"
#include "net/ipv6/multicast/uip-mcast6.h"
#include <string.h>
//#include "net/ipv6/uip-debug.h"
#include "net/ip/uip-debug.h"
#include "sys/energest.h"

#ifdef CONF_MAX_PATH_NODES
#define MAX_PATH_NODES CONF_MAX_PATH_NODES
#else
#define MAX_PATH_NODES 10 
#endif

#ifdef NUM_CONF_NODES
#define NUM_NODES NUM_CONF_NODES
#else
#define NUM_NODES 20
#endif

#ifdef CONF_MAX_CHILD
#define MAX_CHILD CONF_MAX_CHILD
#else
#define MAX_CHILD 10
#endif

// Delay in secs
#ifdef CONF_ATTESTATION_DELAY
#define ATTESTATION_DELAY CONF_ATTESTATION_DELAY
#else
#define ATTESTATION_DELAY 7
#endif

#ifdef CONF_STOP_PARENT_SWITCH
#define STOP_PARENT_SWITCH (CONF_STOP_PARENT_SWITCH * 60)
#else
#define STOP_PARENT_SWITCH (60 * 10) //3 min
#endif

#ifdef CONF_FRAME_LENGTH
#define FRAME_LENGTH CONF_FRAME_LENGTH
#else
#define FRAME_LENGTH 100
#endif

/* REQUEST/PKT TYPES */
#define TOPOLOGY 30
#define ATTESTATION  40
#define SECOND_ROUND 50
#define SECOND_TIME_START 60
#define SECOND_TIME_STOP  70
#define SECOND_TIME_DONE  45

/*If set, in phase 1 the nodes will broadcast the aggregated resp to parents
if 0, then the resp is sent as unicast packets to each parent node */
#define BROADCAST_TO_PARENTS 1 
/* In topology phase unicast, in att phase broadcast */
#define ATTESTATION_TIME_TEST 1

#define BINARY_TREE 0

struct request_packet {
    uint8_t pkt_type;
    uint32_t seed;
}__attribute__((packed));

struct att_packet {
    uint64_t checksum;
}__attribute__((packed));

struct topology_info {
    uint8_t pathindex;
    uint16_t path[MAX_PATH_NODES];
    rpl_rank_t max_leaf_rank;
    uint64_t checksum;
}__attribute__((packed));

struct sec_rnd_resp {
    uint8_t max_index;
    uint64_t checksum;
//	uint64_t child_checksums[MAX_CHILD];
}__attribute__((packed));

struct response_pkt {
	uint8_t pkt_type;
//	rpl_rank_t sender_rank;
	union {
		struct att_packet att;
		struct topology_info topology;
	} pkt;	
}__attribute__((packed)); 

uint16_t get_my_id();
uint16_t get_id_from_addr(uip_ipaddr_t *uid);
int attest_memory(uint32_t nonce, uint64_t *checksum);
void send_to_parents(struct response_pkt *packet, uint8_t is_client);
//void create_broadcast_conn();
//void broadcast_to_parents(struct response_pkt *packet);
rpl_rank_t get_rank();
uint8_t is_leaf();
void print_energy();
//rpl_parent_t *rpl_get_second_best_parent();
