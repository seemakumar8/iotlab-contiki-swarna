#include "random.h"
#include "orchestra.h"
#include "../utility-functions.h"
#include "sys/timer.h"

#include "sys/log.h"
#define LOG_MODULE "App"
#ifdef LOG_CONF_LEVEL
#define LOG_LEVEL LOG_CONF_LEVEL
#else
#define LOG_LEVEL LOG_LEVEL_INFO
#endif

#define MCAST_SINK_UDP_PORT 5001 /* Host byte order */
#define UIP_IP_BUF   ((struct uip_ip_hdr *)&uip_buf[UIP_LLH_LEN])

#define UNICAST_SECREQ_PORT   12345

uip_ipaddr_t router_addr;
#define set_router_addr() uip_ip6addr(&router_addr, UIP_DS6_DEFAULT_PREFIX, 0, 0, 0, 0, 0, 0, 1);
uint64_t checksum;

#if BROADCAST_TO_PARENTS == 1
#define CHECKSUM_PORT 4010
static struct simple_udp_connection broadcast_conn;
struct response_pkt ctimer_resp;
static struct ctimer p_timer;
#endif

static struct simple_udp_connection secreq_conn;
static struct uip_udp_conn *sink_conn;

#if !NETSTACK_CONF_WITH_IPV6 || !UIP_CONF_ROUTER || !UIP_IPV6_MULTICAST || !UIP_CONF_IPV6_RPL
#error "This example can not work with the current contiki configuration"
#error "Check the values of: NETSTACK_CONF_WITH_IPV6, UIP_CONF_ROUTER, UIP_CONF_IPV6_RPL"
#endif

/*---------------------------------------------------------------------------*/
PROCESS(udp_client_process, "UDP client");
AUTOSTART_PROCESSES(&udp_client_process);
/*---------------------------------------------------------------------------*/
/*static void
udp_rx_callback(struct simple_udp_connection *c,
         const uip_ipaddr_t *sender_addr,
         uint16_t sender_port,
         const uip_ipaddr_t *receiver_addr,
         uint16_t receiver_port,
         const uint8_t *data,
         uint16_t datalen)
{
  unsigned count = *(unsigned *)data;
  LOG_INFO("Received response %u (tc:%d) from ", count,
           uipbuf_get_attr(UIPBUF_ATTR_MAX_MAC_TRANSMISSIONS));
  LOG_INFO_6ADDR(sender_addr);
  LOG_INFO_("\n");
}*/
/*---------------------------------------------------------------------------*/
#if BROADCAST_TO_PARENTS == 1
static void
checksum_receiver(struct simple_udp_connection *c,
         const uip_ipaddr_t *sender_addr,
         uint16_t sender_port,
         const uip_ipaddr_t *receiver_addr,
         uint16_t receiver_port,
         const uint8_t *data,
         uint16_t datalen)
{
  LOG_DBG("should not be here\n");
}
/*---------------------------------------------------------------------------*/
void create_broadcast_conn()
{
   /* Broadcast attestation request */
   simple_udp_register(&broadcast_conn, CHECKSUM_PORT,
                      NULL, CHECKSUM_PORT,
                      checksum_receiver);

   LOG_DBG("Listening on broadcast conn port %d\n", CHECKSUM_PORT);
}
/*---------------------------------------------------------------------------*/
//void broadcast_to_parents(void *packet)
void broadcast_to_parents()
{
  uip_ipaddr_t baddr;

  LOG_DBG("Broadcasting size:%d\n", sizeof(struct response_pkt));
  uip_create_linklocal_allnodes_mcast(&baddr);
  simple_udp_sendto(&broadcast_conn, &ctimer_resp, sizeof(struct response_pkt), &baddr);
}
#endif
/*---------------------------------------------------------------------------*/
uint8_t create_response(struct request_packet *mpkt, struct response_pkt *resp)
{

    if(attest_memory(mpkt->seed, &checksum)){
	    if(mpkt->pkt_type == TOPOLOGY){
			resp->pkt_type = (uint8_t)TOPOLOGY;
//			resp->sender_rank = get_rank();
			resp->pkt.topology.path[0] = get_my_id();
			resp->pkt.topology.pathindex = 1;
			resp->pkt.topology.checksum = checksum;
			resp->pkt.topology.max_leaf_rank = get_rank();
     		LOG_DBG_PY("comm_overhead: 27\n"); 
        }else if(mpkt->pkt_type == ATTESTATION){
			resp->pkt_type = (uint8_t)ATTESTATION;
//			resp->sender_rank = get_rank();
			resp->pkt.att.checksum = checksum;
     		LOG_DBG_PY("comm_overhead: 35\n"); 
		}
    } else {
		LOG_DBG("Failed to calculate checksum\n");
		return 0;
	}
	return 1;
}
/*---------------------------------------------------------------------------*/
void unicast_send_to_sink()
{
  struct sec_rnd_resp resp;

  resp.max_index = 0;
  resp.checksum = checksum;

  LOG_DBG("Sec round: Sending response, %d\n", sizeof(resp));
  simple_udp_sendto(&secreq_conn, &resp, sizeof(struct sec_rnd_resp), &router_addr);
}
/*---------------------------------------------------------------------------*/
static void
udp_secreq_callback(struct simple_udp_connection *c,
         const uip_ipaddr_t *sender_addr,
         uint16_t sender_port,
         const uip_ipaddr_t *receiver_addr,
         uint16_t receiver_port,
         const uint8_t *data,
         uint16_t datalen)
{
    struct request_packet *secreq = (struct request_packet *)(data);

    LOG_DBG("Received UDP req, len %d\n", datalen);
	if(secreq->pkt_type == SECOND_ROUND)
    {
		LOG_DBG("Second round: Received request packet\n");
		unicast_send_to_sink();
	}
}
/*---------------------------------------------------------------------------*/
static void
tcpip_handler(void)
{
  uint32_t nonce;
  struct request_packet *mpkt;
  struct response_pkt resp;

  memset(&resp, 0, sizeof(resp));

  if(uip_newdata()) {
    mpkt = (struct request_packet *)(uip_appdata);

    nonce = mpkt->seed;
    LOG_DBG("Received multicast: [0x%08lx]\n", nonce);

    //static struct ctimer broadcast_timer;
	if(create_response(mpkt, &resp))
#if BROADCAST_TO_PARENTS == 1
	 ctimer_resp = resp;
#if ATTESTATION_TIME_TEST == 1
	{
	    if(mpkt->pkt_type == TOPOLOGY)
			send_to_parents(&resp, 1);
		else {
	 		if(BINARY_TREE) {
	 			if(get_my_id() % 2 == 0)
					broadcast_to_parents();
				else 
	 				ctimer_set(&p_timer, (90 * (CLOCK_SECOND/1000)), broadcast_to_parents, NULL);
	  		} else
				broadcast_to_parents();
		}
	}
#else
		broadcast_to_parents();
#endif
#else
		send_to_parents(&resp, 1);
#endif
  }
}
/*---------------------------------------------------------------------------*/
static uip_ds6_maddr_t *
join_mcast_group(void)
{
  uip_ipaddr_t addr;
  uip_ds6_maddr_t *rv;

  /*
   * IPHC will use stateless multicast compression for this destination
   * (M=1, DAC=0), with 32 inline bits (1E 89 AB CD)
   */
  uip_ip6addr(&addr, 0xFF1E,0,0,0,0,0,0x89,0xABCA);
  rv = uip_ds6_maddr_add(&addr);

  if(rv) {
    LOG_DBG("Joined multicast group\n");
  }
  return rv;
}
void network_settled()
{
	//stop_parent_switch();
	rpl_print_neighbor_list();
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(udp_client_process, ev, data)
{
  static struct ctimer rpl_timer;
  PROCESS_BEGIN();

  orchestra_init();
  if(join_mcast_group() == NULL) {
    LOG_DBG("Failed to join multicast group\n");
    PROCESS_EXIT();
  }

  /* Multicast Connection */
  sink_conn = udp_new(NULL, UIP_HTONS(0), NULL);
  udp_bind(sink_conn, UIP_HTONS(MCAST_SINK_UDP_PORT));

  /* Second Round Request - Downward Traffic*/
  set_router_addr();
  simple_udp_register(&secreq_conn, UNICAST_SECREQ_PORT, NULL,
                      UNICAST_SECREQ_PORT, udp_secreq_callback);

#if BROADCAST_TO_PARENTS == 1
  create_broadcast_conn();
#endif

  ctimer_set(&rpl_timer, STOP_PARENT_SWITCH * CLOCK_SECOND, network_settled, NULL);
  while(1) {
    PROCESS_YIELD();
    if(ev == tcpip_event) {
      tcpip_handler();
    }
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
