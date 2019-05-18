#include "list_utility.h"
#include "../utility-functions.h"

#define MAX_CHILD_NODES MAX_CHILD

#include "sys/log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_INFO


static int num_nodes;
LIST(child_list);
MEMB(nodememb, child_node_t, MAX_CHILD_NODES);

/*---------------------------------------------------------------------------*/
void fill_child_entries()
{
  uip_ipaddr_t *addr;
  rpl_parent_t *p;
  child_node_t *child_node;
  rpl_instance_t *instance;

  instance = rpl_get_default_instance();

  for(p = nbr_table_head(rpl_parents); p != NULL; p = nbr_table_next(rpl_parents, p)) {
     addr = rpl_get_parent_ipaddr(p);
	 if(p->rank > instance->current_dag->rank) {
         child_node = memb_alloc(&nodememb);
		 child_node->child_id = addr->u8[LINKADDR_SIZE - 1];
		 //child_node->child_id = addr->u8[15];
		 child_node->received_pkt = 0;

		 list_add(child_list, child_node);
		 num_nodes++;
     }
  }
}
/*---------------------------------------------------------------------------*/
void init_child_list(void)
{
  num_nodes = 0;
  memb_init(&nodememb);
  list_init(child_list);
	
  fill_child_entries();
}
/*---------------------------------------------------------------------------*/
uint8_t is_child_set(uint8_t child_id)
{
   child_node_t *l;

   for(l = list_head(child_list); l != NULL; l = list_item_next(l)) {
	 if(l->child_id == child_id && l->received_pkt){
		return 1;
	 }
   }
   return 0;
}
/*---------------------------------------------------------------------------*/
/* Reset table entries */
void rest_child_list()
{
   child_node_t *l;
   for(l = list_head(child_list); l != NULL; l = list_item_next(l)) {
     l->received_pkt = 0;
   }
}
/*---------------------------------------------------------------------------*/
/* Check if all the entries in the child table is set */
uint8_t is_all_child_set()
{
   child_node_t *l;
   for(l = list_head(child_list); l != NULL; l = list_item_next(l)) {
     if(l->received_pkt == 0){
        return 0;
     }
   }
   return 1; 
}
/*---------------------------------------------------------------------------*/
uint8_t is_child(uint8_t child_id)
{
   child_node_t *l;
   for(l = list_head(child_list); l != NULL; l = list_item_next(l)) {
	 if(l->child_id == child_id){
		return 1;
	 }
   }
   return 0;
}
/*---------------------------------------------------------------------------*/
uint8_t child_set_pkt_received(uint8_t child_id)
{
   child_node_t *l;
   for(l = list_head(child_list); l != NULL; l = list_item_next(l)) {
	 if(l->child_id == child_id){
		l->received_pkt = 1;
		return 1;
	 }
   }
   return 0;
}
/*---------------------------------------------------------------------------*/
uint8_t get_child_count()
{
	return num_nodes;
}
/*---------------------------------------------------------------------------*/
void
print_child_list(void)
{
   child_node_t *l;
   printf("Child Info:\n");
   for(l = list_head(child_list); l != NULL; l = list_item_next(l)) {
	 printf("child: %d, received_pkt: %d\n", l->child_id, l->received_pkt);
   }
}
