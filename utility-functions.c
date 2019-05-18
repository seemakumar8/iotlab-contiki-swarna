#include "dev/watchdog.h"
#include "net/packetbuf.h"
#include "attestation_lib.h"
#include "utility-functions.h"
#include "sys/stimer.h"
#include "sys/timer.h"
#include "net/ipv6/uip-ds6.h"

#include <stdio.h>
//#include "sys/cooja_mt.h"
//#include "lib/simEnvChange.h"

#include "sys/log.h"
#define LOG_MODULE "App"
#ifdef LOG_CONF_LEVEL
#define LOG_LEVEL LOG_CONF_LEVEL
#else
#define LOG_LEVEL LOG_LEVEL_INFO
#endif

#define PARENT_UDP_CHECKSUM_PORT 4005
#define MY_UDP_CHECKSUM_PORT 4005
#define UNICAST_SECREQ_PORT   4002


//static struct uip_udp_conn *broadcast_conn;
//static struct timer att_timer;
//static int att_timer_expired = 1;
/*---------------------------------------------------------------------------*/
uint8_t get_my_id() {
  return (uint8_t)(linkaddr_node_addr).u8[LINKADDR_SIZE - 1];
}
/*---------------------------------------------------------------------------*/
/*void toggle_value()
{
	att_timer_expired = 0;
}*/
int attest_memory(uint32_t nonce, uint64_t *checksum)
{
    /*    
    rtimer_clock_t time, time1;

    time = RTIMER_NOW();
    *checksum = attestation(nonce);
    time1 = RTIMER_NOW();
    time = time1 - time;
    timer_set(&att_timer, CLOCK_SECOND);
	while(1){
       if(timer_expired(&att_timer)){
         LOG_DBG("*****expired*******\n");
         break;
       }
	}
*/
   	*checksum = 0x11e108b7^get_my_id();
    LOG_DBG_PY("nonce: %x, my_checksum:%x\n", (unsigned int)nonce, (unsigned int)*checksum);

  return 1;
}
/*---------------------------------------------------------------------------*/
struct uip_udp_conn* get_parent_conn()
{
  static struct uip_udp_conn *parent_conn;
  parent_conn = udp_new(NULL, UIP_HTONS(PARENT_UDP_CHECKSUM_PORT), NULL);
  udp_bind(parent_conn, UIP_HTONS(MY_UDP_CHECKSUM_PORT));

  return parent_conn;
}
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/*	RPL FUNCTIONS */
/*---------------------------------------------------------------------------*/
rpl_rank_t get_rank()
{
  rpl_instance_t *instance;

  instance = rpl_get_default_instance();

  return instance->current_dag->rank;
//  printf("rank:%d, parent: %d, root:%d\n", instance->current_dag->rank, instance->current_dag->preferred_parent->rank, ROOT_RANK(instance));
}
/*---------------------------------------------------------------------------*/
int
rpl_neighbor_is_parent(rpl_parent_t *nbr, uint8_t is_client)
{
  if(is_client)
	return 1;
  else
    return nbr != NULL && nbr->rank < get_rank();
}
/*---------------------------------------------------------------------------*/
void send_to_parents(struct response_pkt *packet, uint8_t is_client)
{
  static struct uip_udp_conn *parent_conn;
  static uint8_t count = 0;
  uip_ipaddr_t *addr;
  rpl_parent_t *p;

  parent_conn = get_parent_conn();
  if(parent_conn == NULL){
    LOG_DBG("Failed to create socket to parent\n");
    return;
  }
  LOG_DBG("Sending to parents: ");
  for(p = nbr_table_head(rpl_parents); p != NULL; p = nbr_table_next(rpl_parents, p)) {
  	 addr = rpl_get_parent_ipaddr(p);
  	 //addr = rpl_parent_get_ipaddr(p);
	 if(rpl_neighbor_is_parent(p, is_client)) {
     	LOG_DBG_6ADDR(addr);
     	LOG_DBG(",");

		count += 1;
	 	uip_udp_packet_sendto(parent_conn, packet, sizeof(struct response_pkt),
                        addr, UIP_HTONS(PARENT_UDP_CHECKSUM_PORT));
	 }
  }

  LOG_DBG("\n");
}
/*---------------------------------------------------------------------------*/
uint8_t is_leaf()
{
 /* rpl_parent_t *p;
  for(p = nbr_table_head(rpl_parents); p != NULL; p = nbr_table_next(rpl_parents, p)) {
     if(p != NULL && p->rank > get_rank()){
		printf("my rank: %d, neigh: %d\n", get_rank(), rpl_rank_via_parent(p));
	 	return 0;	
     }
  }*/

  if(get_child_count()){
	printf("child count: %d\n", get_child_count());
	return 0;
  }

  printf("----------------------------------------------------\n");
  printf("I AM LEAF\n");
  printf("----------------------------------------------------\n");

  return 1;
}
/*---------------------------------------------------------------------------*/
#if 0
rpl_parent_t *rpl_get_second_best_parent() 
{
	int first = rpl_get_parent_link_metric(default_instance->current_dag->preferred_parent); 
	int second = 1000;
	rpl_parent_t *second_parent = NULL;

	rpl_parent_t *p = nbr_table_head(rpl_parents);
	while(p != NULL){
		int rank = rpl_get_parent_link_metric(p);
		
	 	if(rpl_neighbor_is_parent(p, 0)) {
			if(rank > first && rank < second){
				second = rank;
				second_parent = p;
			}
		}
		p = nbr_table_next(rpl_parents, p);
	}

	return second_parent;
}
/*---------------------------------------------------------------------------*/
#endif
