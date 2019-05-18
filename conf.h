#define LOG_DBG_6ADDR(...) PRINT6ADDR(__VA_ARGS__)
#define LOG_DBG(...) printf(__VA_ARGS__)
#define LOG_INFO(...) printf(__VA_ARGS__)

#include "../../iotlab/00-configuration/tsch-project-conf.h"
#include "../../iotlab/00-configuration/iotlab-project-conf.h"

/*****************************************************************************************/
/* ATTESTATION */
/*****************************************************************************************/
#define NUM_CONF_NODES 15 //Also change tric in orchestra
#define CONF_FRAME_LENGTH 31 //Should be length of broadcast frame
#define CONF_STOP_PARENT_SWITCH 5

#define ATT_WITH_ORCHESTRA 0
/*****************************************************************************************/
/* TSCH-Orchestra */
/*****************************************************************************************/

/* IEEE802.15.4 frame version */
//#undef FRAME802154_CONF_VERSION
//#define FRAME802154_CONF_VERSION FRAME802154_IEEE802154E_2012

#if ATT_WITH_ORCHESTRA == 1
#define TSCH_SCHEDULE_CONF_WITH_6TISCH_MINIMAL 0
#define TSCH_CONF_WITH_LINK_SELECTOR 1

#define TSCH_CALLBACK_NEW_TIME_SOURCE orchestra_callback_new_time_source
#define TSCH_CALLBACK_PACKET_READY orchestra_callback_packet_ready
#define NETSTACK_CONF_ROUTING_NEIGHBOR_ADDED_CALLBACK orchestra_callback_child_added
#define NETSTACK_CONF_ROUTING_NEIGHBOR_REMOVED_CALLBACK orchestra_callback_child_removed

//Sender-based
#define ORCHESTRA_CONF_UNICAST_SENDER_BASED 1
#define ORCHESTRA_CONF_RULES { &eb_per_time_source, &unicast_per_neighbor_rpl_storing, &default_common }
//#define UIP_CONF_MAX_ROUTES 20

//BINARY
//#define ORCHESTRA_CONF_UNICAST_PERIOD 15
//#define ORCHESTRA_CONF_COMMON_SHARED_PERIOD 9
//#define ORCHESTRA_CONF_EBSF_PERIOD 31

#define ORCHESTRA_CONF_UNICAST_PERIOD 17
#define ORCHESTRA_CONF_COMMON_SHARED_PERIOD 31
//#define ORCHESTRA_CONF_COMMON_SHARED_PERIOD 59


/*
#define TSCH_CONF_MAX_INCOMING_PACKETS 32
#define TSCH_CONF_DEQUEUED_ARRAY_SIZE 64
#define QUEUEBUF_CONF_NUM 32
#define TSCH_CONF_MAC_MAX_FRAME_RETRIES 12
#define TSCH_SCHEDULE_CONF_MAX_LINKS 50 
*/
#else
#define TSCH_SCHEDULE_CONF_WITH_6TISCH_MINIMAL 1
#endif
/*****************************************************************************************/
/* RPL */
/*****************************************************************************************/
/*#define RPL_MRHOF_CONF_SQUARED_ETX 1

//Choose OF-hop count
#define RPL_CONF_SUPPORTED_OFS {&rpl_of0, &rpl_mrhof}
#define RPL_CONF_OF_OCP RPL_OCP_OF0
#define RPL_OF0_CONF_SR RPL_OF0_FIXED_SR
*/

/* Turn of DAO ACK to make code smaller */
#undef RPL_CONF_WITH_DAO_ACK
#define RPL_CONF_WITH_DAO_ACK          0

#undef RPL_CONF_OF
#define RPL_CONF_OF                    rpl_of0

/*****************************************************************************************/
/* MULTICAST */
/*****************************************************************************************/
#include "net/ipv6/multicast/uip-mcast6-engines.h"

/* Change this to switch engines. Engine codes in uip-mcast6-engines.h */
#ifndef UIP_MCAST6_CONF_ENGINE
#define UIP_MCAST6_CONF_ENGINE UIP_MCAST6_ENGINE_ROLL_TM
//#define UIP_MCAST6_CONF_ENGINE UIP_MCAST6_ENGINE_SMRF
#endif

/* For Imin: Use 16 over CSMA, 64 over Contiki MAC */
//#define ROLL_TM_CONF_IMIN_1         64

#define UIP_MCAST6_ROUTE_CONF_ROUTES 1

/*****************************************************************************************/
/* ENERGEST */
/*****************************************************************************************/
/*#define ENERGEST_CONF_ON 0

#define SICSLOWPAN_CONF_FRAG 0
#define UIP_CONF_BUFFER_SIZE 140
#define UIP_CONF_TCP 0 */
#define LOG_CONF_LEVEL LOG_LEVEL_NONE
//#define LOG_CONF_LEVEL_MAC LOG_LEVEL_DBG
//#define LOG_CONF_LEVEL_TCPIP LOG_LEVEL_DBG
#define LOG_DBG_PY printf
//#define LOG_DBG_PY LOG_INFO
