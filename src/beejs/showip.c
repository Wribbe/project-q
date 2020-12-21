#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>

int
main(int argc, char * argv[]) {

  if (argc != 2) {
    fprintf(stderr, "usage: %s hostname\n", argv[0]);
    return 1;
  }

  struct addrinfo hints = {
    .ai_family = AF_UNSPEC,
    .ai_socktype = SOCK_STREAM,
  };
  struct addrinfo * res = NULL;

  char ipstr[INET6_ADDRSTRLEN];
  int status = getaddrinfo(argv[1], NULL, &hints, &res);
  if (status != 0) {
    fprintf(stderr, "ERROR: getaddrinfo: %s.\n", gai_strerror(status));
    return 2;
  }

  printf("IP addresses for %s:\n\n", argv[1]);

  for (struct addrinfo * p = res; p != NULL; p = p->ai_next) {

    void * addr = NULL;
    const char * ipver = "";

    // Get the pointer the address itself, different fields in IPv4 and IPv6.
    if (p->ai_family == AF_INET) { // IPv4.
      struct sockaddr_in * ipv4 = (struct sockaddr_in *)p->ai_addr;
      addr = &(ipv4->sin_addr);
      ipver = "IPv4";
    } else { // IPv6.
      struct sockaddr_in6 * ipv6 = (struct sockaddr_in6 *)p->ai_addr;
      addr = &(ipv6->sin6_addr);
      ipver = "IPv6";
    }

    // Convert the IP to a string and print it:
    inet_ntop(p->ai_family, addr, ipstr, sizeof ipstr);
    printf("  %s: %s\n", ipver, ipstr);

  }

  freeaddrinfo(res); // Free the linked list.
  return 0;

}
