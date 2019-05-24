#include "net/ip/uip.h"
#include "net/ipv6/uip-ds6.h"
#include "net/rpl/rpl-private.h"
#if RPL_WITH_NON_STORING
#include "net/rpl/rpl-ns.h"
#endif /* RPL_WITH_NON_STORING */
#include "net/netstack.h"
#include "dev/slip.h"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#define DEBUG DEBUG_NONE
#include "dev/button-sensor.h"
#include "sys/ctimer.h"
#include "sys/clock.h"
#include "orchestra.h"
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
static uip_ipaddr_t prefix;
static uint8_t prefix_set;

PROCESS(border_router_process, "Border router process");

#if WEBSERVER==0
/* No webserver */
AUTOSTART_PROCESSES(&border_router_process);
#elif WEBSERVER>1
/* Use an external webserver application */
#include "webserver-nogui.h"
AUTOSTART_PROCESSES(&border_router_process,&webserver_nogui_process);
#else
/* Use simple webserver with only one page for minimum footprint.
 * Multiple connections can result in interleaved tcp segments since
 * a single static buffer is used for all segments.
 */
#include "httpd-simple.h"
/* The internal webserver can provide additional information if
 * enough program flash is available.
 */
#define WEBSERVER_CONF_LOADTIME 0
#define WEBSERVER_CONF_FILESTATS 0
#define WEBSERVER_CONF_NEIGHBOR_STATUS 0
/* Adding links requires a larger RAM buffer. To avoid static allocation
 * the stack can be used for formatting; however tcp retransmissions
 * and multiple connections can result in garbled segments.
 * TODO:use PSOCk_GENERATOR_SEND and tcp state storage to fix this.
 */
#define WEBSERVER_CONF_ROUTE_LINKS 0
#if WEBSERVER_CONF_ROUTE_LINKS
#define BUF_USES_STACK 1
#endif

PROCESS(webserver_nogui_process, "Web server");
PROCESS_THREAD(webserver_nogui_process, ev, data)
{
	PROCESS_BEGIN();

	httpd_init();

	while(1) {
		PROCESS_WAIT_EVENT_UNTIL(ev == tcpip_event);
		httpd_appcall(data);
	}

	PROCESS_END();
}
AUTOSTART_PROCESSES(&border_router_process,&webserver_nogui_process);

static const char *TOP = "<html><head><title>ContikiRPL</title></head><body>\n";
static const char *BOTTOM = "</body></html>\n";
#if BUF_USES_STACK
static char *bufptr, *bufend;
#define ADD(...) do {                                                   \
	bufptr += snprintf(bufptr, bufend - bufptr, __VA_ARGS__);      \
} while(0)
#else
static char buf[256];
static int blen;
#define ADD(...) do {                                                   \
	blen += snprintf(&buf[blen], sizeof(buf) - blen, __VA_ARGS__);      \
} while(0)
#endif

/*---------------------------------------------------------------------------*/
	static void
ipaddr_add(const uip_ipaddr_t *addr)
{
	uint16_t a;
	int i, f;
	for(i = 0, f = 0; i < sizeof(uip_ipaddr_t); i += 2) {
		a = (addr->u8[i] << 8) + addr->u8[i + 1];
		if(a == 0 && f >= 0) {
			if(f++ == 0) ADD("::");
		} else {
			if(f > 0) {
				f = -1;
			} else if(i > 0) {
				ADD(":");
			}
			ADD("%x", a);
		}
	}
}
/*---------------------------------------------------------------------------*/
	static
PT_THREAD(generate_routes(struct httpd_state *s))
{
	static uip_ds6_route_t *r;
#if RPL_WITH_NON_STORING
	static rpl_ns_node_t *link;
#endif /* RPL_WITH_NON_STORING */
	static uip_ds6_nbr_t *nbr;
#if BUF_USES_STACK
	char buf[256];
#endif
#if WEBSERVER_CONF_LOADTIME
	static clock_time_t numticks;
	numticks = clock_time();
#endif

	PSOCK_BEGIN(&s->sout);

	SEND_STRING(&s->sout, TOP);
#if BUF_USES_STACK
	bufptr = buf;bufend=bufptr+sizeof(buf);
#else
	blen = 0;
#endif
	ADD("Neighbors<pre>");

	for(nbr = nbr_table_head(ds6_neighbors);
			nbr != NULL;
			nbr = nbr_table_next(ds6_neighbors, nbr)) {

#if WEBSERVER_CONF_NEIGHBOR_STATUS
#if BUF_USES_STACK
		{char* j=bufptr+25;
			ipaddr_add(&nbr->ipaddr);
			while (bufptr < j) ADD(" ");
			switch (nbr->state) {
				case NBR_INCOMPLETE: ADD(" INCOMPLETE");break;
				case NBR_REACHABLE: ADD(" REACHABLE");break;
				case NBR_STALE: ADD(" STALE");break;
				case NBR_DELAY: ADD(" DELAY");break;
				case NBR_PROBE: ADD(" NBR_PROBE");break;
			}
		}
#else
		{uint8_t j=blen+25;
			ipaddr_add(&nbr->ipaddr);
			while (blen < j) ADD(" ");
			switch (nbr->state) {
				case NBR_INCOMPLETE: ADD(" INCOMPLETE");break;
				case NBR_REACHABLE: ADD(" REACHABLE");break;
				case NBR_STALE: ADD(" STALE");break;
				case NBR_DELAY: ADD(" DELAY");break;
				case NBR_PROBE: ADD(" NBR_PROBE");break;
			}
		}
#endif
#else
		ipaddr_add(&nbr->ipaddr);
#endif

		ADD("\n");
#if BUF_USES_STACK
		if(bufptr > bufend - 45) {
			SEND_STRING(&s->sout, buf);
			bufptr = buf; bufend = bufptr + sizeof(buf);
		}
#else
		if(blen > sizeof(buf) - 45) {
			SEND_STRING(&s->sout, buf);
			blen = 0;
		}
#endif
	}
	ADD("</pre>Routes<pre>\n");
	SEND_STRING(&s->sout, buf);
#if BUF_USES_STACK
	bufptr = buf; bufend = bufptr + sizeof(buf);
#else
	blen = 0;
#endif

	for(r = uip_ds6_route_head(); r != NULL; r = uip_ds6_route_next(r)) {

#if BUF_USES_STACK
#if WEBSERVER_CONF_ROUTE_LINKS
		ADD("<a href=http://[");
		ipaddr_add(&r->ipaddr);
		ADD("]/status.shtml>");
		ipaddr_add(&r->ipaddr);
		ADD("</a>");
#else
		ipaddr_add(&r->ipaddr);
#endif
#else
#if WEBSERVER_CONF_ROUTE_LINKS
		ADD("<a href=http://[");
		ipaddr_add(&r->ipaddr);
		ADD("]/status.shtml>");
		SEND_STRING(&s->sout, buf); //TODO: why tunslip6 needs an output here, wpcapslip does not
		blen = 0;
		ipaddr_add(&r->ipaddr);
		ADD("</a>");
#else
		ipaddr_add(&r->ipaddr);
#endif
#endif
		ADD("/%u (via ", r->length);
		ipaddr_add(uip_ds6_route_nexthop(r));
		if(1 || (r->state.lifetime < 600)) {
			ADD(") %us\n", (unsigned int)r->state.lifetime); // iotlab printf does not have %lu
			//ADD(") %lus\n", (unsigned long)r->state.lifetime);
		} else {
			ADD(")\n");
		}
		SEND_STRING(&s->sout, buf);
#if BUF_USES_STACK
		bufptr = buf; bufend = bufptr + sizeof(buf);
#else
		blen = 0;
#endif
	}
	ADD("</pre>");

#if RPL_WITH_NON_STORING
	ADD("Links<pre>\n");
	SEND_STRING(&s->sout, buf);
#if BUF_USES_STACK
	bufptr = buf; bufend = bufptr + sizeof(buf);
#else
	blen = 0;
#endif
	for(link = rpl_ns_node_head(); link != NULL; link = rpl_ns_node_next(link)) {
		if(link->parent != NULL) {
			uip_ipaddr_t child_ipaddr;
			uip_ipaddr_t parent_ipaddr;

			rpl_ns_get_node_global_addr(&child_ipaddr, link);
			rpl_ns_get_node_global_addr(&parent_ipaddr, link->parent);

#if BUF_USES_STACK
#if WEBSERVER_CONF_ROUTE_LINKS
			ADD("<a href=http://[");
			ipaddr_add(&child_ipaddr);
			ADD("]/status.shtml>");
			ipaddr_add(&child_ipaddr);
			ADD("</a>");
#else
			ipaddr_add(&child_ipaddr);
#endif
#else
#if WEBSERVER_CONF_ROUTE_LINKS
			ADD("<a href=http://[");
			ipaddr_add(&child_ipaddr);
			ADD("]/status.shtml>");
			SEND_STRING(&s->sout, buf); //TODO: why tunslip6 needs an output here, wpcapslip does not
			blen = 0;
			ipaddr_add(&child_ipaddr);
			ADD("</a>");
#else
			ipaddr_add(&child_ipaddr);
#endif
#endif

			ADD(" (parent: ");
			ipaddr_add(&parent_ipaddr);
			if(1 || (link->lifetime < 600)) {
				ADD(") %us\n", (unsigned int)link->lifetime); // iotlab printf does not have %lu
				//ADD(") %lus\n", (unsigned long)r->state.lifetime);
			} else {
				ADD(")\n");
			}
			SEND_STRING(&s->sout, buf);
#if BUF_USES_STACK
			bufptr = buf; bufend = bufptr + sizeof(buf);
#else
			blen = 0;
#endif
		}
	}
	ADD("</pre>");
#endif /* RPL_WITH_NON_STORING */

#if WEBSERVER_CONF_FILESTATS
	static uint16_t numtimes;
	ADD("<br><i>This page sent %u times</i>",++numtimes);
#endif

#if WEBSERVER_CONF_LOADTIME
	numticks = clock_time() - numticks + 1;
	ADD(" <i>(%u.%02u sec)</i>",numticks/CLOCK_SECOND,(100*(numticks%CLOCK_SECOND))/CLOCK_SECOND));
#endif

	SEND_STRING(&s->sout, buf);
	SEND_STRING(&s->sout, BOTTOM);

	PSOCK_END(&s->sout);
}
/*---------------------------------------------------------------------------*/
	httpd_simple_script_t
httpd_simple_get_script(const char *name)
{

	return generate_routes;
}

#endif /* WEBSERVER */


/*---------------------------------------------------------------------------*/
	static void
multicast_send(uint8_t token)
{
	uint32_t id = 1234;
	struct request_packet mpkt;
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
	if(resp->pkt_type == TOPOLOGY) {
		LOG_DBG_PY("agg_path: ");
		int i;

		for(i = 0; i <  resp->pkt.topology.pathindex; i++) {
			LOG_DBG_PY("%x,", resp->pkt.topology.path[i]);
		}
		LOG_DBG_PY("\n");

		LOG_DBG_PY("Received checksum: %x, from %x", (unsigned int)resp->pkt.topology.checksum, get_id_from_addr(sender_addr));
		LOG_DBG_PY("\n");
	} else if(resp->pkt_type == ATTESTATION){
		cnt++;
		LOG_DBG_PY("Received checksum: %x, from\n", (unsigned int)resp->pkt.att.checksum);
		printf("Received %d\n", cnt);
		LOG_DBG_6ADDR(sender_addr);
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
	req_pkt *rpkt = (req_pkt *)(uip_appdata);
	LOG_DBG_PY("Received request %d from verifier\n", rpkt->req_type);
	printf("Received req\n");

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
	 * 	 * IPHC will use stateless multicast compression for this destination
	 * 	 	 * (M=1, DAC=0), with 32 inline bits (1E 89 AB CD)
	 * 	 	 	 */
	uip_ip6addr(&ipaddr, 0xFF1E,0,0,0,0,0,0x89,0xABCA);
	mcast_conn = udp_new(&ipaddr, UIP_HTONS(MCAST_SINK_UDP_PORT), NULL);
}
/*---------------------------------------------------------------------------*/

	static void
print_local_addresses(void)
{
	int i;
	uint8_t state;

	PRINTA("Server IPv6 addresses:\n");
	for(i = 0; i < UIP_DS6_ADDR_NB; i++) {
		state = uip_ds6_if.addr_list[i].state;
		if(uip_ds6_if.addr_list[i].isused &&
				(state == ADDR_TENTATIVE || state == ADDR_PREFERRED)) {
			PRINTA(" ");
			uip_debug_ipaddr_print(&uip_ds6_if.addr_list[i].ipaddr);
			PRINTA("\n");
		}
	}
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
PROCESS_THREAD(border_router_process, ev, data)
{
	static struct etimer et;

	PROCESS_BEGIN();

	/* While waiting for the prefix to be sent through the SLIP connection, the future
	 * border router can join an existing DAG as a parent or child, or acquire a default
	 * router that will later take precedence over the SLIP fallback interface.
	 * Prevent that by turning the radio off until we are initialized as a DAG root.
	 */
	prefix_set = 0;
	NETSTACK_MAC.off(0);

	PROCESS_PAUSE();

	SENSORS_ACTIVATE(button_sensor);

	PRINTF("RPL-Border router started\n");
#if 0
	/* The border router runs with a 100% duty cycle in order to ensure high
	   packet reception rates.
	   Note if the MAC RDC is not turned off now, aggressive power management of the
	   cpu will interfere with establishing the SLIP connection */
	NETSTACK_MAC.off(1);
#endif

	/* Request prefix until it has been received */
	while(!prefix_set) {
		etimer_set(&et, CLOCK_SECOND);
		request_prefix();
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
	}

	/* Now turn the radio on, but disable radio duty cycling.
	 * Since we are the DAG root, reception delays would constrain mesh throughbut.
	 */
	NETSTACK_MAC.on();
	/* NETSTACK_MAC.off(1); for ContikiMAC */

	orchestra_init();
#if DEBUG || 1
	print_local_addresses();
#endif
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
		if (ev == sensors_event && data == &button_sensor) {
			PRINTF("Initiating global repair\n");
			rpl_repair_root(RPL_DEFAULT_INSTANCE);
		}
		if(ev == tcpip_event) {
			tcpip_handler();
		}
	}

	PROCESS_END();
}
/*---------------------------------------------------------------------------*/
