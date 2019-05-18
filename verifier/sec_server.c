#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <inttypes.h>

#define PORT 12345
#define SERVADDR "fd00::201:1:1:1"
#define MAX_CHILD 15

#define TOPOLOGY 30
#define ATTESTATION 40
#define SECOND_ROUND 50
#define SECOND_TIME_START 60
#define SECOND_TIME_STOP  70
#define SECOND_TIME_DONE  45

typedef struct {
   uint8_t req_type;
   uint8_t size;
   uint8_t node_addr[150];  
}__attribute__((packed))req_pkt;

struct sec_rnd_resp {
    uint8_t max_index;
    uint64_t checksum;
//    uint64_t child_checksums[MAX_CHILD];
}__attribute__((packed));

struct request_packet {
    uint8_t pkt_type;
	uint8_t run_count;
}__attribute__((packed));

int sock;
socklen_t clilen;
struct sockaddr_in6 server_addr, client_addr;
char buffer[1024];
char addrbuf[INET6_ADDRSTRLEN];
int sec_client_count = 0, run_no = 0;
int node_index = 0, node_no = 0, sent_id = 0;
char serv_addr[100];
static FILE *fp;

int start_sent = 0;
req_pkt local_rpkt;
/*---------------------------------------------------------------------------*/
void current_timestamp() {
    struct timeval te; 
    gettimeofday(&te, NULL); // get current time
    long long milliseconds = te.tv_sec*1000LL + te.tv_usec/1000; // calculate milliseconds
    printf("milliseconds: %lld\n", milliseconds);
    //return milliseconds;
}
/*---------------------------------------------------------------------------*/
int get_id_from_addr(char *a)
{
	char *token;
	int id;

	/*fd00::204:4:....*/
	strtok(addrbuf, ": ");
	strtok(NULL, ": ");
	token = strtok(NULL, ": ");

	id = (int)strtol(token, NULL, 16);
	printf("id:%d\n", id);

	return id;
}
/*---------------------------------------------------------------------------*/
void get_addr_from_id(uint8_t id, char *a)
{
  char temp[20];

  if(id <= 15)
  	strcpy(a, "fd00::20");
  else
	strcpy(a, "fd00::2");

  sprintf(temp, "%x:%x:%x:%x", id, id, id, id);
  strcat(a, temp);
}
/*---------------------------------------------------------------------------*/
void second_udp_send(struct request_packet *spkt, char* addr)
{
  memset(&server_addr, 0, sizeof(server_addr));

  server_addr.sin6_family = AF_INET6;
  inet_pton(AF_INET6, addr, &server_addr.sin6_addr);
  server_addr.sin6_port = htons(PORT);

  printf("Sending packet to %s, type: %d\n", addr, spkt->pkt_type);

  /* send a datagram */
  if (sendto(sock, spkt, sizeof(struct request_packet), 0,
             (struct sockaddr *)&server_addr,
	     sizeof(server_addr)) < 0) {
      perror("sendto failed");
      exit(4);
  }

}
/*---------------------------------------------------------------------------*/
void send_req_single_node(){
   char *addr;
   struct request_packet spkt;

   memset(&spkt, 0, sizeof(spkt));
   spkt.run_count = run_no;
   spkt.pkt_type = SECOND_ROUND;
 
   addr = (char *)malloc(1024);
   get_addr_from_id(local_rpkt.node_addr[node_index], addr);

   sent_id = local_rpkt.node_addr[node_index]; 
   int node_id_check = local_rpkt.node_addr[node_index]; 
   if(node_id_check == 0){ 
		printf("Node ID is 0\n");
		return;
	}

   printf("node_index: %d, node_id:%d\n", node_index, local_rpkt.node_addr[node_index]);
   second_udp_send(&spkt, addr);
   node_index++;
}
/*---------------------------------------------------------------------------*/
void send_unicast_second_round_req(req_pkt *rpkt)
{
   int max_size;

   if(start_sent == 0) {
	   struct request_packet spkt;

	   /*Send Simulation start */
	   memset(&spkt, 0, sizeof(spkt));

	   spkt.pkt_type = SECOND_TIME_START;
	   spkt.run_count = run_no;
   	   second_udp_send(&spkt, serv_addr);

	  start_sent = 1;
   }

   printf("**************************\n");
   printf("spkt.run_count: %d\n", run_no);

   memcpy(&local_rpkt, rpkt, sizeof(req_pkt));

   node_index = 0;
   send_req_single_node();
/*   for(node_index = 0; node_index < max_size; node_index++){
     printf("id: %d, ", local_rpkt.node_addr[node_index]);
	
     addr = (char *)malloc(1024);
     get_addr_from_id(local_rpkt.node_addr[node_index], addr);
     second_udp_send(&spkt, addr);
	 sleep(1);
   }
*/
}
/*---------------------------------------------------------------------------*/
int read_line_send_req()
{
  char *token;
  char line[1000];
  req_pkt rpkt;

  memset(&rpkt, 0, sizeof(req_pkt));
  rpkt.req_type = SECOND_ROUND;

  while(fgets(line, sizeof(line), fp) != NULL)
  {
	if(strstr(line, "Run_no") != NULL){
		token = strtok(line, ": ");
		run_no = atoi(strtok(NULL, ": "));
		continue;
	}else if(strstr(line, "faulty_nodes") != NULL){
		token = strtok(line, ": ");
   		printf("**************************\n");
		printf("Faulty Nodes: %d\n", atoi(strtok(NULL, ": ")));
   		printf("**************************\n");
		//Ignore this line
		continue;	
	}else if(strstr(line, "****") != NULL){
		//Ignore this line
		continue;	
	}else {
		token = strtok(line, ", ");
		sec_client_count = 0;
		while( token != NULL ) {
    	  rpkt.node_addr[sec_client_count] = atoi(token);
	      token = strtok(NULL, ", ");
		  sec_client_count++;
	    }
	    rpkt.size = sec_client_count;
		printf("rpkt.size: %d\n", rpkt.size);
      	send_unicast_second_round_req(&rpkt);

		/*We read just one line at a time */
		return 1;
    }
  }
	return 0;
} 
/*---------------------------------------------------------------------------*/
int main(void)
{
  req_pkt rpkt;
  uint8_t request = SECOND_ROUND;
  struct sockaddr_in6 server_addr_bind;
  struct sec_rnd_resp response;

  /* create a DGRAM (UDP) socket in the INET6 (IPv6) protocol */
  sock = socket(PF_INET6, SOCK_DGRAM, 0);
  if (sock < 0) {
    perror("creating socket");
    exit(1);
  }
 
  server_addr_bind.sin6_family = AF_INET6;
  server_addr_bind.sin6_addr = in6addr_any;
  server_addr_bind.sin6_port = htons(PORT);
 
  /* Bind address and socket together */
  int ret = bind(sock, (struct sockaddr*)&server_addr_bind, sizeof(server_addr_bind));
  if(ret == -1){
	perror("binding socket");
    exit(1);
  }

   strcpy(serv_addr, "fd00::201:1:1:1");

  fp = fopen("temp.txt", "r");	
  if(fp == NULL) {
    memset(&rpkt, 0, sizeof(req_pkt));
	printf("File open error\n");
  	rpkt.req_type = request;
    rpkt.size = 1;
    rpkt.node_addr[0] = 6;

    printf("Sent packet: %d\n", request);
  	send_unicast_second_round_req(&rpkt);
  } else {
	current_timestamp();
	if(!read_line_send_req()) { //Read line with node IDs
		printf("DONE\n");
		fclose(fp);
		exit(0);
	}
  }

  int recv_len, recv_id;
  int recv_count = 0;
  char recv_addr[100];
  struct request_packet spkt1;
  while(1) {
//	  printf("waiting for a reply...\n");
	  clilen = sizeof(client_addr);
	  recv_len = recvfrom(sock, &response, sizeof(struct sec_rnd_resp), 0, (struct sockaddr *)&client_addr, &clilen);
	  if(recv_len < 0) {
	      perror("recvfrom failed");
    	  exit(4);
  	  }

	  recv_count++;
  	  printf("got reply from %s, max_index: %u, checksum: %" PRIx64 "\n",
	  	inet_ntop(AF_INET6, &client_addr.sin6_addr, addrbuf,
           INET6_ADDRSTRLEN), response.max_index, response.checksum);

	  if(sec_client_count == recv_count){
   		spkt1.pkt_type = SECOND_TIME_STOP;
   		spkt1.run_count = run_no;
	    second_udp_send(&spkt1, serv_addr);

		recv_count = 0;
//		printf("Received resp from all nodes\n\n");
		if(!read_line_send_req()) {
			current_timestamp();
			printf("DONE\n");
			fclose(fp);

   			spkt1.pkt_type = SECOND_TIME_DONE;
		    second_udp_send(&spkt1, serv_addr);

			exit(0);
		}
	//	sleep(0.5);
	  //} else
	  } else if(sent_id == get_id_from_addr(addrbuf))
	  	send_req_single_node();
   }

  /* close socket */
  close(sock);

  return 0;
}
