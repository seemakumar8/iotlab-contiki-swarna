#include <stdio.h>
#include "lib/list.h"
#include "lib/memb.h"
#include "rpl.h"
#include "rpl-private.h"

typedef struct child_list_struct {
  struct child_list_struct *next;
  uint8_t child_id;
  uint8_t received_pkt;
}child_node_t;

void init_child_list(void);
uint8_t is_child_set(uint8_t child_id);
void rest_child_list();
uint8_t is_all_child_set();
uint8_t is_child(uint8_t child_id);
uint8_t get_child_count();
uint8_t child_set_pkt_received(uint8_t child_id);
void print_child_list(void);
