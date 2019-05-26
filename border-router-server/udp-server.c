#include "dev/button-sensor.h"
#include "sys/ctimer.h"
#include "sys/clock.h"

#include "../utility-functions.h"

#include "sys/log.h"
#define LOG_MODULE "App"
#ifdef LOG_CONF_LEVEL
#define LOG_LEVEL LOG_CONF_LEVEL
#else
#define LOG_LEVEL LOG_LEVEL_INFO
#endif

#define VERIFIER_PORT         12345
#define MCAST_SINK_UDP_PORT   5001

#if BROADCAST_TO_PARENTS == 1
#define CHECKSUM_PORT         4010
#else
#define UNICAST_CHECKSUM_PORT 4005
#endif
#if ATTESTATION_TIME_TEST == 1
#define UNICAST_CHECKSUM_PORT 4005
#endif

static struct uip_udp_conn * mcast_conn, *verifier_conn;
#if BROADCAST_TO_PARENTS == 1
static struct simple_udp_connection broadcast_conn_server;
#else
static struct simple_udp_connection checksum_conn;
#endif
#if ATTESTATION_TIME_TEST == 1
static struct simple_udp_connection checksum_conn;
#endif

static uip_ipaddr_t prefix;
static uint8_t prefix_set;
#if !NETSTACK_CONF_WITH_IPV6 || !UIP_CONF_ROUTER || !UIP_IPV6_MULTICAST || !UIP_CONF_IPV6_RPL
#error "This example can not work with the current contiki configuration"
#error "Check the values of: NETSTACK_CONF_WITH_IPV6, UIP_CONF_ROUTER, UIP_CONF_IPV6_RPL"
#endif

typedef struct {
	uint8_t req_type;
	uint8_t run_count;
}__attribute__((packed))req_pkt;


static clock_time_t ct, ct1, time_diff, ct2;
static int cnt;

PROCESS(udp_server_process, "UDP server");
AUTOSTART_PROCESSES(&udp_server_process);
/*---------------------------------------------------------------------------*/
/*void get_addr_from_id(uint8_t id, uip_ipaddr_t *a)
  {
  a->u8[0] = 0xfd;
  a->u8[1] = 0x00;
  a->u8[2] = 0x00;
  a->u8[3] = 0x00;
  a->u8[4] = 0x00;
  a->u8[5] = 0x00;
  a->u8[6] = 0x00;
  a->u8[7] = 0x00;
  a->u8[8] = 0x02;

  a->u8[9] = id;
  a->u8[10] = 0;
  a->u8[11] = id;
  a->u8[12] = 0;
  a->u8[13] = id;
  a->u8[14] = 0;
  a->u8[15] = id;
  }*/
/*---------------------------------------------------------------------------*/
	static void
multicast_send(uint8_t token)
{
	uint32_t id = 1234;
	struct request_packet mpkt;
	//  uint8_t token = TOPOLOGY;

	memset(&mpkt, 0, sizeof(mpkt));
	mpkt.pkt_type = token;
	mpkt.seed = id;

	LOG_DBG("Multicast Send to: ");
	LOG_DBG(" (msg=0x%08lx)", mpkt.seed);
	LOG_DBG(" %d bytes, token; %d, %d, %d\n", sizeof(mpkt), token, sizeof(uint32_t), sizeof(uint8_t));

	uip_udp_packet_send(mcast_conn, &mpkt, sizeof(mpkt));
}
/*---------------------------------------------------------------------------*/
	static void
udp_checksum_callback(struct simple_udp_connection *c,
		const uip_ipaddr_t *sender_addr,
		uint16_t sender_port,
		const uip_ipaddr_t *receiver_addr,
		uint16_t receiver_port,
		const uint8_t *data,
		uint16_t datalen)
{
	struct response_pkt *resp = (struct response_pkt *)(data);
	//LOG_INFO("Received packet: %d\n", resp->pkt_type);

	if(resp->pkt_type == TOPOLOGY) {
		LOG_DBG_PY("agg_path: ");
		int i;

		for(i = 0; i <  resp->pkt.topology.pathindex; i++) {
			LOG_DBG_PY("%x,", resp->pkt.topology.path[i]);
		}
		LOG_DBG_PY("\n");

		LOG_DBG_PY("Received checksum: %x from %x\n", (unsigned int)resp->pkt.topology.checksum, get_id_from_addr(sender_addr));
	} else if(resp->pkt_type == ATTESTATION){
		cnt++;
		LOG_DBG_PY("Received checksum: %x, from %x\n", (unsigned int)resp->pkt.att.checksum, get_id_from_addr(sender_addr));
		LOG_DBG_PY("\n");
	}
}
/*---------------------------------------------------------------------------*/
#if BROADCAST_TO_PARENTS == 1
void create_broadcast_conn_server()
{
	/* Broadcast attestation request */
	simple_udp_register(&broadcast_conn_server, CHECKSUM_PORT,
			NULL, CHECKSUM_PORT,
			udp_checksum_callback);

	LOG_DBG("Listening on broadcast conn port %d\n", CHECKSUM_PORT);
}
#endif
/*---------------------------------------------------------------------------*/
	static void
tcpip_handler()
{
	//  uint8_t *req_type = (uint8_t *)(uip_appdata);
	req_pkt *rpkt = (req_pkt *)(uip_appdata);
	LOG_DBG_PY("Received request %d from verifier\n", rpkt->req_type);
	cnt = 0;
	switch(rpkt->req_type){
		case TOPOLOGY:
		case ATTESTATION: multicast_send(rpkt->req_type);
				  break;
		case SECOND_TIME_START:  
				  ct = clock_time();
				  ct2 = ct;
				  LOG_DBG_PY("\nSECOND_TIME_START: %u\n", (unsigned int)ct2); 
				  break;
		case SECOND_TIME_STOP: 
				  ct1 = clock_time();
				  time_diff = ct1 - ct;

				  LOG_DBG_PY("\nSECOND_TIME_STOP, run_count: %d, diff: %u, first: %u, second: %u\n", 
						  rpkt->run_count, (unsigned int) time_diff, (unsigned int)ct, (unsigned int)ct1);

				  ct = ct1;
				  break;

		case SECOND_TIME_DONE: LOG_INFO("\nSECOND_TIME_DONE: %u\n", (unsigned int)clock_time); 
				       break;

		default: LOG_DBG("Does not support REQ type from verifier\n");
	}
}
/*---------------------------------------------------------------------------*/
	static void
prepare_mcast(void)
{
	uip_ipaddr_t ipaddr;

	/*
	 * IPHC will use stateless multicast compression for this destination
	 * (M=1, DAC=0), with 32 inline bits (1E 89 AB CD)
	 */
	uip_ip6addr(&ipaddr, 0xFF1E,0,0,0,0,0,0x89,0xABCA);
	mcast_conn = udp_new(&ipaddr, UIP_HTONS(MCAST_SINK_UDP_PORT), NULL);
}
/*---------------------------------------------------------------------------*/
	void
request_prefix(void)
{
	/* mess up uip_buf with a dirty request... */
	uip_buf[0] = '?';
	uip_buf[1] = 'P';
	uip_len = 2;
	slip_send();
	uip_clear_buf();
}
/*---------------------------------------------------------------------------*/
	void
set_prefix_64(uip_ipaddr_t *prefix_64)
{
	rpl_dag_t *dag;
	uip_ipaddr_t ipaddr;
	memcpy(&prefix, prefix_64, 16);
	memcpy(&ipaddr, prefix_64, 16);
	prefix_set = 1;
	uip_ds6_set_addr_iid(&ipaddr, &uip_lladdr);
	uip_ds6_addr_add(&ipaddr, 0, ADDR_AUTOCONF);

	dag = rpl_set_root(RPL_DEFAULT_INSTANCE, &ipaddr);
	if(dag != NULL) {
		rpl_set_prefix(dag, &prefix, 64);
		PRINTF("created a new RPL dag\n");
	}
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(udp_server_process, ev, data)
{
	static struct etimer et;
	PROCESS_BEGIN();

	/* Initialize DAG root */
	//  NETSTACK_ROUTING.root_start();

	/* Request prefix until it has been received */
	prefix_set = 0;
	while(!prefix_set) {
		etimer_set(&et, CLOCK_SECOND);
		request_prefix();
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
	}

	/* Now turn the radio on, but disable radio duty cycling.
	 *    * Since we are the DAG root, reception delays would constrain mesh throughbut.
	 *       */
	NETSTACK_MAC.on();

	/* Initialize verifier connection */
	verifier_conn = udp_new(NULL, UIP_HTONS(0), NULL);
	udp_bind(verifier_conn, UIP_HTONS(VERIFIER_PORT));

#if BROADCAST_TO_PARENTS == 1
	create_broadcast_conn_server();
#else
	/* Children nodes starts sending checksum - Upward Traffic*/
	simple_udp_register(&checksum_conn, UNICAST_CHECKSUM_PORT, NULL,
			UNICAST_CHECKSUM_PORT, udp_checksum_callback);
#endif
#if ATTESTATION_TIME_TEST == 1
	simple_udp_register(&checksum_conn, UNICAST_CHECKSUM_PORT, NULL,
			UNICAST_CHECKSUM_PORT, udp_checksum_callback);
#endif

	prepare_mcast();

	while(1) {
		PROCESS_YIELD();
		if(ev == tcpip_event) {
			tcpip_handler();
		}
	}

	PROCESS_END();
}
