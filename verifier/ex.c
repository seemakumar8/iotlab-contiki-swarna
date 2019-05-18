#include <stdio.h>
#include <inttypes.h>

struct att_packet {
    uint64_t checksum;
};

struct topology_info {
    uint8_t pathindex;
    uint8_t path[50];
    uint8_t max_leaf_rank;
    uint64_t checksum;
};

struct response_pkt {
    uint8_t pkt_type;
    union {
        struct att_packet att;
        struct topology_info topology;
    } pkt;
};

void main()
{
   struct response_pkt resp;
   resp.pkt.att.checksum = 1234;

   printf("resp: %d\n", sizeof(resp));
}
