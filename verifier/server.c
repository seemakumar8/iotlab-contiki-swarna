#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

#define PORT 12345
#define SERVADDR "fd00::9074"

#define TOPOLOGY 30
#define ATTESTATION 40
#define SECOND_ROUND 50

typedef struct {
   uint8_t req_type;
   uint8_t seq_no;
}__attribute__((packed))req_pkt;

/*---------------------------------------------------------------------------*/
int main(int argc, char **argv)
{
  int sock;
  socklen_t clilen;
  struct sockaddr_in6 server_addr, client_addr;
  char buffer[1024];
  char addrbuf[INET6_ADDRSTRLEN];
  //uint8_t request = ATTESTATION;
  int request = TOPOLOGY;

  if(argc > 1) {
	request = atoi(argv[1]);
    printf("request: %d, argc: %d\n", request, argc);
  }

  /* create a DGRAM (UDP) socket in the INET6 (IPv6) protocol */
  sock = socket(PF_INET6, SOCK_DGRAM, 0);
   
  if (sock < 0) {
    perror("creating socket");
    exit(1);
  }

  req_pkt rpkt;
  int i;
  memset(&rpkt, 0, sizeof(req_pkt));

//  rpkt.req_type = ATTESTATION;
  rpkt.req_type = request;
  rpkt.seq_no = 1;

  memset(&server_addr, 0, sizeof(server_addr));

  server_addr.sin6_family = AF_INET6;
  inet_pton(AF_INET6, SERVADDR, &server_addr.sin6_addr);
  server_addr.sin6_port = htons(PORT);

  /* send a datagram */
  if (sendto(sock, &rpkt, sizeof(req_pkt), 0,
             (struct sockaddr *)&server_addr,
	     sizeof(server_addr)) < 0) {
      perror("sendto failed");
      exit(4);
  }

  printf("Sent packet: %d, size: %d\n", request, sizeof(req_pkt));

  /* close socket */
  close(sock);

  return 0;
}
