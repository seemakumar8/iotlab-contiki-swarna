#include "random.h"
#include "list_utility.h"
#include "../utility-functions.h"
#include "orchestra.h"
#include "sys/log.h"
#define LOG_MODULE "App"
#ifdef LOG_CONF_LEVEL
#define LOG_LEVEL LOG_CONF_LEVEL
#else
#define LOG_LEVEL LOG_LEVEL_INFO
#endif

#if BROADCAST_TO_PARENTS == 1
#define CHECKSUM_PORT 4010
#else
#define PARENT_UDP_CHECKSUM_PORT 4005
#define CLIENT_UDP_CHECKSUM_PORT 4005
#endif

#if ATTESTATION_TIME_TEST == 1
#define PARENT_UDP_CHECKSUM_PORT 4005
#define CLIENT_UDP_CHECKSUM_PORT 4005
#endif

#define UNICAST_SECREQ_PORT      12345
#define MAX_PAYLOAD_LEN 120
#define MCAST_SINK_UDP_PORT 5001 /* Host byte order */
#define UIP_IP_BUF   ((struct uip_ip_hdr *)&uip_buf[UIP_LLH_LEN])


uip_ipaddr_t router_addr;
#define set_router_addr() uip_ip6addr(&router_addr, UIP_DS6_DEFAULT_PREFIX, 0, 0, 0, 0, 0, 0, 1);
static struct simple_udp_connection secreq_conn;
static struct uip_udp_conn *sink_conn;

#if BROADCAST_TO_PARENTS == 1
static struct simple_udp_connection broadcast_conn;
static struct ctimer p_timer;
#else
static struct uip_udp_conn *checksum_conn;
#endif

#if ATTESTATION_TIME_TEST == 1
static struct uip_udp_conn *checksum_conn;
#endif
struct response_pkt ctimer_resp;

static uint64_t checksum_children[MAX_CHILD], sec_checksum_children[MAX_CHILD];
uint8_t max_index = 0, first_round_completed = 1;

uint64_t my_checksum;
uint16_t paths[MAX_PATH_NODES];
static int cur_loc = 0, paths_loc = 0, first_child = 1;
static struct ctimer periodic_timer, rpl_timer;
static rpl_rank_t max_leaf_rank;

#if !NETSTACK_CONF_WITH_IPV6 || !UIP_CONF_ROUTER || !UIP_IPV6_MULTICAST || !UIP_CONF_IPV6_RPL
#error "This example can not work with the current contiki configuration"
#error "Check the values of: NETSTACK_CONF_WITH_IPV6, UIP_CONF_ROUTER, UIP_CONF_IPV6_RPL"
#endif

void process_topology_req(struct response_pkt *resp, uint16_t child_id);
void process_attestation_req(struct response_pkt *resp, uint16_t child_id);
/*---------------------------------------------------------------------------*/
PROCESS(udp_relay_process, "UDP relay");
AUTOSTART_PROCESSES(&udp_relay_process);
/*---------------------------------------------------------------------------*/
void reset_variable()
{
   /* Copy to second round variables */
   first_round_completed = 1;
   max_index = cur_loc;
   memcpy(&sec_checksum_children, &checksum_children, sizeof(uint64_t) * MAX_CHILD);

   cur_loc = 0;
   paths_loc = 0;
   first_child = 1;
//   memset(&checksum_children, 0, sizeof(uint64_t) * MAX_CHILD);
   memset(&paths, 0, sizeof(uint16_t) * MAX_PATH_NODES);
   rest_child_list();
}
/*---------------------------------------------------------------------------*/
uint64_t get_combined_checksum()
{
  uint8_t c_index = 0;
  uint64_t combined_checksum = 0;

  for(c_index = 0; c_index < cur_loc; c_index++){
     combined_checksum = (combined_checksum^checksum_children[c_index]);
//	 LOG_INFO("combined = %" PRIx64 ", c_child = %" PRIx64 ", i = %d, cur_loc = %d,\n", combined_checksum, checksum_children[c_index], c_index, cur_loc);
  }

  if(attest_memory((uint32_t)combined_checksum, &my_checksum)){
     combined_checksum = combined_checksum ^ my_checksum;
//	 LOG_DBG("comb_c; %" PRIx64 ", check_my: %" PRIx64 "\n", combined_checksum, my_checksum);
  }
  else {
     LOG_DBG("Failed to calculate checksum\n");
	 return 0;
  }
  return combined_checksum;
}
/*---------------------------------------------------------------------------*/
uint8_t get_attestation_timeout()
{
  uint8_t timeout;
  /*
    max_path: Rank of the farthest leaf node reachable through this node
    RPL_MIN_HOPRANKINC: Set to 256 for of0.
    3: step_inc for each hop 
    timeout: number of hops to the farthest leaf node
  */

  timeout = ((max_leaf_rank / RPL_MIN_HOPRANKINC) - (get_rank() / RPL_MIN_HOPRANKINC)) / 3;
//  printf("timeout: %d\n", timeout);

  return timeout;
}
/*---------------------------------------------------------------------------*/
uint8_t get_topology_timeout()
{
  uint8_t timeout;

  /*
    Assuming a path topology calculate the timeout.
    [num_nodes (max_depth)] - [this nodes hop from root node]
  */

  timeout = NUM_NODES - ((get_rank() / RPL_MIN_HOPRANKINC - 1) / 3);
  //LOG_DBG("NUM_NODES: %d, rank: %d, RPL_MIN_HOPRANKINC: %d\n", NUM_NODES, get_rank(), RPL_MIN_HOPRANKINC);

  return timeout;
}
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
  struct response_pkt *resp;
  resp = (struct response_pkt *)(data);
  uip_ipaddr_t sender;

  memcpy(&sender, sender_addr, sizeof(uip_ipaddr_t));
 /* LOG_INFO("Received broadcast response from: %d, ", LINKADDR_SIZE);
  LOG_INFO_6ADDR(sender_addr);
  LOG_INFO("\n");
*/
  if(resp->pkt_type == TOPOLOGY)
		process_topology_req(resp, get_id_from_addr(&sender));
		//process_topology_req(resp, sender_addr->u8[LINKADDR_SIZE - 1]);
  else if(resp->pkt_type == ATTESTATION)
		process_attestation_req(resp, get_id_from_addr(&sender));
		//process_attestation_req(resp, sender_addr->u8[LINKADDR_SIZE - 1]);
}
/*---------------------------------------------------------------------------*/
void broadcast_to_parents()
//void broadcast_to_parents(void *packet)
{
  uip_ipaddr_t baddr;

  LOG_INFO("Broadcasting to parents:\n");
  uip_create_linklocal_allnodes_mcast(&baddr);
  simple_udp_sendto(&broadcast_conn, &ctimer_resp, sizeof(struct response_pkt), &baddr);
}
/*---------------------------------------------------------------------------*/
void create_broadcast_conn()
{
   /* Broadcast attestation request */
   simple_udp_register(&broadcast_conn, CHECKSUM_PORT,
                      NULL, CHECKSUM_PORT,
                      checksum_receiver);
}
#endif
/*---------------------------------------------------------------------------*/
void send_attestation_resp()
{
  uint64_t combined_checksum = 0;

  combined_checksum = get_combined_checksum();
  if(combined_checksum != 0){
     struct response_pkt resp;

	 resp.pkt_type = (uint8_t)ATTESTATION;
	 //resp.sender_rank = get_rank();
	 resp.pkt.att.checksum = combined_checksum;
#if BROADCAST_TO_PARENTS == 1
	 printf("Broadcasting from send_attestation_resp\n");
	 ctimer_resp = resp;
//	 broadcast_to_parents();
	 if(BINARY_TREE) {
	 	if(get_my_id() % 2 == 0)
			broadcast_to_parents();
		else 
	 		ctimer_set(&p_timer, (90 * (CLOCK_SECOND/1000)), broadcast_to_parents, NULL);
	  } else
	 	ctimer_set(&p_timer, (random_rand() % (get_my_id() + FRAME_LENGTH * 10 * (CLOCK_SECOND/1000))), broadcast_to_parents, NULL);
#else
     send_to_parents(&resp, 0);
	 /* packetbuf_hdr = 10, frame_hdr = 14, 2 bytes, app = 9 bytes */
     LOG_DBG_PY("comm_overhead: 35\n"); 
#endif

	 reset_variable();
  }
}
/*---------------------------------------------------------------------------*/
void send_topology_resp()
{
  uint64_t combined_checksum = 0;

  combined_checksum = get_combined_checksum();
  struct response_pkt resp;

  resp.pkt_type = (uint8_t)TOPOLOGY;
//  resp.sender_rank = get_rank();
  resp.pkt.topology.checksum = combined_checksum;
  resp.pkt.topology.max_leaf_rank = max_leaf_rank;

  // memcpy(&resp.pkt.topology.path, &paths, MAX_PATH_NODES * sizeof(uint8_t));

  int i;
  for(i = 0; i < paths_loc; i++) {
	resp.pkt.topology.path[i] = paths[i];
	//LOG_DBG("%d:", resp.pkt.topology.path[i]);
  }
  LOG_DBG("path, combined checksum: %x\n", (unsigned int)combined_checksum);

  if(i >= MAX_PATH_NODES-1) {
	LOG_DBG("path full: %d\n", i); 
  } else {
	/* Add my id and send the packet */
   	resp.pkt.topology.path[paths_loc] = get_my_id();
   	resp.pkt.topology.pathindex = paths_loc + 1;
  }

#if ATTESTATION_TIME_TEST == 1
     send_to_parents(&resp, 0);
#else
     send_to_parents(&resp, 0);
	 /* packetbuf_hdr = 10, frame_hdr = 14, 2 bytes, app = 1 bytes */
#endif
     	LOG_DBG_PY("comm_overhead: 35\n"); 
	 reset_variable();
}
/*---------------------------------------------------------------------------*/
void process_attestation_req(struct response_pkt *resp, uint16_t child_id)
{
  checksum_children[cur_loc++] = resp->pkt.att.checksum;

  if(!is_child(child_id)) {
	//LOG_DBG("Topology resp packet from non-child node: %d\n", child_id);
	return;
  }

 /* if(is_child_set(child_id)){
	LOG_DBG("Packet already received from this child: %d\n", child_id);
	return;
  }*/

#if BROADCAST_TO_PARENTS == 1
  if(first_child) {
	// frame_length_sec * no_of_hops_away millsec
    ctimer_set(&periodic_timer, get_attestation_timeout() * 10 * FRAME_LENGTH * (CLOCK_SECOND/1000), send_attestation_resp, NULL);
    first_child = 0;
  }
#else
  child_set_pkt_received(child_id);

  if(is_all_child_set()){
	send_attestation_resp();
  }
#endif 
}
/*---------------------------------------------------------------------------*/
//void send_req_to_child
/*---------------------------------------------------------------------------*/
int is_present_paths(int recv_id)
{
	int i;
	for (i = 0 ; i < paths_loc ; i++)
	{
    	if (paths[i] == recv_id) 
		 {
           return 1;
    	 }
	}
   return 0;
}
/*---------------------------------------------------------------------------*/
void process_topology_req(struct response_pkt *resp, uint16_t child_id)
{

  if(!is_child(child_id)) {
	LOG_DBG("Topology resp packet from non-child node: %d\n", child_id);
	return;
  }

  ctimer_reset(&periodic_timer);

  child_set_pkt_received(child_id);

  checksum_children[cur_loc] = resp->pkt.topology.checksum;
  cur_loc = cur_loc + 1; //received cound of child nodes

  LOG_DBG_PY("node_id:%x, child_id:%x, c_checksum:%x\n", get_my_id(), child_id,
  //LOG_DBG_PY("I node_id:%x received from child_id:%x, c_checksum:%x\n", get_my_id(), resp->pkt.topology.path[resp->pkt.topology.pathindex-1],
						 (unsigned int)resp->pkt.topology.checksum);

  /* Copy paths from received pkt to local variable */
  int i, j;
  for(i = paths_loc, j = 0; j < resp->pkt.topology.pathindex; j++) {
	 if(is_present_paths(resp->pkt.topology.path[j])) {
		continue;
     }

     paths[i] = resp->pkt.topology.path[j];
     i++;
	 if(i >= MAX_PATH_NODES-2) {
		/*If path is full we send multiple packets*/
		LOG_DBG("path full and fixing:i %d, paths_loc :%d\n", i, paths_loc);
  		paths_loc = i;
		send_topology_resp();
		reset_variable();
		i = 0;
	 }
  }
  paths_loc = i;

  if(max_leaf_rank < resp->pkt.topology.max_leaf_rank)
	max_leaf_rank = resp->pkt.topology.max_leaf_rank;

  //printf("child_count: %d, total children: %d\n", cur_loc, get_child_count());
  /*If packet received from all children, then forward request 
	Do not hold the response forever. Timeout after a certain value */
  if(cur_loc == get_child_count()){
//  if(is_all_child_set()){
	send_topology_resp();
  } else {
    ctimer_set(&periodic_timer, get_topology_timeout() * CLOCK_SECOND, send_topology_resp, NULL);
  }

  /*
  if(first_child) {
    ctimer_set(&periodic_timer, get_topology_timeout() * CLOCK_SECOND, send_topology_resp, NULL);
    first_child = 0;
  } */
}
/*---------------------------------------------------------------------------*/
static void
tcpip_handler(void)
{
  struct response_pkt *resp;
  static int send_pkt_count = 0;

  if(uip_newdata()) {
    resp = (struct response_pkt *)(uip_appdata);

	if(resp->pkt_type == TOPOLOGY){
		send_pkt_count++;
		//process_topology_req(resp, UIP_IP_BUF->srcipaddr.u8[LINKADDR_SIZE - 1]);
		process_topology_req(resp, get_id_from_addr(&UIP_IP_BUF->srcipaddr));
	}
	else if(resp->pkt_type == ATTESTATION){
		send_pkt_count++;
		process_attestation_req(resp, get_id_from_addr(&UIP_IP_BUF->srcipaddr));
	}

  }
}
/*---------------------------------------------------------------------------*/
uint8_t create_response(struct request_packet *mpkt, struct response_pkt *resp)
{
	uint64_t checksum;

    if(attest_memory(mpkt->seed, &checksum)){
	    if(mpkt->pkt_type == TOPOLOGY){
			resp->pkt_type = (uint8_t)TOPOLOGY;
			resp->pkt.topology.path[0] = get_my_id();
			resp->pkt.topology.pathindex = 1;
			resp->pkt.topology.checksum = checksum;
			resp->pkt.topology.max_leaf_rank = get_rank();
        }else if(mpkt->pkt_type == ATTESTATION){
			resp->pkt_type = (uint8_t)ATTESTATION;
			resp->pkt.att.checksum = checksum;
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

//  memcpy(&resp.child_checksums, &sec_checksum_children, sizeof(uint64_t) * MAX_CHILD);
  resp.max_index = max_index;
  resp.checksum = my_checksum;
 
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
    printf("received second round\n");

	if(secreq->pkt_type == SECOND_ROUND)
    {
		if(first_round_completed){
			LOG_DBG("Second round: Received request packet\n");
			unicast_send_to_sink();
		}
	}
}
/*---------------------------------------------------------------------------*/
static void
mcast_tcpip_handler(void)
{
  uint32_t nonce;
  struct request_packet *mpkt;
  struct response_pkt resp;

  memset(&resp, 0, sizeof(resp));

  if(uip_newdata()) {
    mpkt = (struct request_packet *)(uip_appdata);

    nonce = mpkt->seed;
    LOG_DBG("Received multicast: [0x%08lx], pkt_type:%d\n", nonce, mpkt->pkt_type);

	if(create_response(mpkt, &resp))
#if ATTESTATION_TIME_TEST == 1
	{
	    if(mpkt->pkt_type == TOPOLOGY) {
			send_to_parents(&resp, 1);
    			LOG_DBG_PY("comm_overhead: 35\n"); 
		}
		else if(mpkt->pkt_type == ATTESTATION) {
	 		ctimer_resp = resp;
			broadcast_to_parents();
		}
	}
#else
		send_to_parents(&resp, 0);
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

  printf("Joining multicast group");
  if(rv) {
    printf("Joined multicast group");
  }
  return rv;
}
/*---------------------------------------------------------------------------*/
void network_settled()
{
  //stop_parent_switch(); //COmment this line if not requirded to stop parent switch

  rpl_print_neighbor_list();
  init_child_list();
  print_child_list();

  if(is_leaf()){
  	if(join_mcast_group() == NULL) {
    	printf("Failed to join multicast group\n");
    	return;
  	}

  	/* Multicast Connection */
  	sink_conn = udp_new(NULL, UIP_HTONS(0), NULL);
  	udp_bind(sink_conn, UIP_HTONS(MCAST_SINK_UDP_PORT));
  }
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(udp_relay_process, ev, data)
{

  PROCESS_BEGIN();

  orchestra_init();
#if BROADCAST_TO_PARENTS == 1
  create_broadcast_conn();
#else
  /* Children nodes starts sending checksum */
  checksum_conn = udp_new(NULL, UIP_HTONS(CLIENT_UDP_CHECKSUM_PORT), NULL);
  udp_bind(checksum_conn, UIP_HTONS(PARENT_UDP_CHECKSUM_PORT));
#endif
#if ATTESTATION_TIME_TEST == 1
  checksum_conn = udp_new(NULL, UIP_HTONS(CLIENT_UDP_CHECKSUM_PORT), NULL);
  udp_bind(checksum_conn, UIP_HTONS(PARENT_UDP_CHECKSUM_PORT));
#endif

  /* Second Round Request - Downward Traffic*/
  set_router_addr();
  simple_udp_register(&secreq_conn, UNICAST_SECREQ_PORT, NULL,
                      UNICAST_SECREQ_PORT, udp_secreq_callback);

  printf("My ID is: %x\n", get_my_id());
  ctimer_set(&rpl_timer, STOP_PARENT_SWITCH * CLOCK_SECOND, network_settled, NULL);
  while(1){
    PROCESS_YIELD();

    if(ev == tcpip_event) {
  		if(is_leaf())
			mcast_tcpip_handler();
		else
		      tcpip_handler();
    }
  }


  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
