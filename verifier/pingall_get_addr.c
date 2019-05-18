#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void get_addr_from_id(int id, char *a)
{
  char temp[20];

  if(id <= 15)
    strcpy(a, "fd00::20");
  else
    strcpy(a, "fd00::2");

  sprintf(temp, "%x:%x:%x:%x", id, id, id, id);
  strcat(a, temp);
}

void main()
{
  int i;
  char addr[100], cmd[100];

  for (i = 1; i < 150; i++) {
	get_addr_from_id(i, addr);
    printf("Pinging %s\n", addr);
    sprintf(cmd, "ping6 %s -c 1", addr);
    system(cmd);
  }

}
