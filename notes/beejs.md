# Notes on beej's guide to Network Programming

## Two Types of Internet Sockets

* `SOCK_STREAM` - Stream Socket (TCP)
* `SOCK_DGRAM` - Datagram Socket (UDP)

`SOCK_STREAM` provides a reliable two-way connection that preserves the order
the items are sent. Achieved by _The Transmission Control Protocol_, or _TCP_
for short.

`SOCK_DGRAM`, uses _USER Datagaram Protocol_, does not promise that things will
arrive, or that they will retain their original sequencing if they arrive, but
the data will be correct. For situations when the transfered content actually
matters, like file-transfers, there can be another developer-implemented
protocol on top of UDP. For games or streaming audio, there is no point of
caring for lost packages, they can be ignored.


## Low level Nonsense and Network Theory

`[ Ethernet [ IP [ UDP [ TFTP [ Data ] ] ] ] ]` -- data encapsulation.

Most general version of the `Layered Network Model` or `ISO/OSI`:
* Application
* Presentation
* Session
* Transport
* Network
* Data Link
* Physical

A more Unix-like layered-ish model:
* Application Layer (_telnet_, _ftp_, _etc_.)
* Host-to-Host Transport Layer (_TCP_, _UDP_)
* Internet Layer (_IP and routing_)
* Network Access Layer (_Ethernet_, _wi-fi_, or _whatever_)

Really no need to do anything special with this information, for a STREAM ->
`send()` and for a DGRAM -> `encapsulate w. method + sendto()` and the kernel
does the rest.


## IP Addresses, `structs`, and Data Munging

IPv6-loopback: `::1`  
IPv4-compatibility mode: `::ffff:192.0.2.33`


### Subnets

How to split a ip-address into `network-portion` and `host-portion`. For example
`192.0.2.12` the first three bytes `192.0.2` could represent the network, and
`.12` is the host. Which gives us; `host 12` on network `192.0.2.0`. If your
infrastructure was represented by 1 byte, the remaining 3 bytes could be hosts
(~16 million) -> `Class A` network. On the other end, 3 bytes for network and 1
byte for hosts -> `~256` possible hosts (A few of them are reserved).

__Old-style net-mask:__
In order to figure out the network part of the ip-address, a _netmask_ is used,
by AND'ing it and the ip-address. Typical netmask of `255.255.255.0` in
combination with `192.0.2.12` gives `192.0.2.0` as the network part.

__New-style net-mask:__
Put a slash `/` after the ip-address denoting the number of bits of the
network-mask, e.g. `192.0.2.12/30` indicates that 30-bits should be masked in
order to get the network part of the address.


### Port Numbers

Port numbers (16-bit) help distinguish which service that goes where, so that
multiple applications can communicate on the same connection. Ports under 1024
are usually considered special that require special os-permissions to use.


### Byte Order

`Little-Endian` -> The least-significant "rightmost" byte is sent first.  
`Big-Endian` -> The most-significant "leftmost" byte is sent first.

`Big-Endian == Network Byte Order`, where `Host Byte Order` is the
order used by the host computer, which can be either.

Conversion is used in order to make it possible to ignore how the host
represents the bytes. Provided functions can either convert a `2-byte / 8-bit`
(`short`) or a `4-byte / 16-bit` (`long`) number between the `__h__ost` and the
`__n__etwork` as follows:
```
htons()         host to network short
htonl()         host to network long
ntohs()         network to host short
ntohl()         network to host long
```

Nothing for `8-byte / 64-bit`, and if you want to manage a `float` then you
have to `serialize`.


### structs

Socket descriptors are stored in an `int` and the socket is prepped for for
subsequent use by the new-ish `addrinfo` struct.
```
struct addrinfo {
  int              ai_flags;     // AI_PASSIVE, AI_CANONNAME, etc.
  int              ai_family;    // AF_INET, AF_INET6, AF_UNSPEC.
  int              ai_socktype;  // SOCK_STREAM, SOCK_DGRAM.
  int              ai_protocol;  // use 0 for "any".
  size_t           ai_addrlen;   // size of ai_addr in bytes.
  struct sockaddr *ai_addr;      // struct socaddr_in or _in6.
  char            *ai_canonname; // full canonical hostname.
  struct addrinfo *ai_next;      // linked list, next node.
};
```

This struct is usually populated by hand and then fed into `getaddrinfo()`
which populates the `ai_next` field with additional information. For the
`ai_addr` field, it does not has to be manually filled, but it will have to be
accessed.
```
struct sockaddr {
  unsigned short    sa_family;   // Address family, AF_xxx.
  char              sa_data[14]; // 14 bytes of protocol address.
}
```

For this document the values of `sa_family` will be either `AF_INET` or
`AF_INET6` for IPv4 and IPv6 respectively. Exists a parallell `struct
socaddr_in` in order to avoid having to pack the `sa_data` by hand.

__IMPORTANT__: _a pointer to `struct sockaddr_in` can be cast to a `struct
socaddr` and reversed_.

__IPv4__:
```
struct sockaddr_in {
  short int             sin_family;   // Address family, AF_INET.
  unsigned short int    sin_port;     // Port number.
  struct in_addr        sin_addr;     // Internet address.
  unsigned char         sin_zero[8];  // Padding.
}
```
_Note:_ The `sin_port` should be provided in `Network Byte Order` by converting
it with `htons()`.

The `sin_addr` used to be a union, but is currently defined as:
```
struct sin_addr {
  uint32_t s_addr;
}
```
Don't need to bother with converting the address, handled by `#defines`.

__IPv6__:
```
struct sockaddr_in6 {
  u_int16_t       sin6_family;    // Address family, AF_INET6.
  u_int16_t       sin6_port;      // Port number (NBO).  
  u_int32_t       sin6_flowinfo;  // IPv6 flow information.
  struct in6_addr sin6_addr;      // IPv6 address.
  u_int32_t       sin_scope_id;   // Scope ID.
};

struct in6_addr {
  unsigned char s6_addr[16]; // IPv6 address.
}
```

There is also a `struct sockaddr_storage` that can hold any of the previous
structs:
```
struct sockaddr_storage {
  sa_family_t ss_family;      // Address famly.
  // Platform-specific padding.
};
```
This is useful when the family is not known, this struct can be passed along
instead of `struct sockaddr_in` or `struct sockaddr_in6` and then the
`ss_family` field can be read and the structure cast into the correct variant.


## IP Addresses II

No need for packing here either, `inet_pton(int af, const char * src, void *
dst)`, (_presentation to network_) takes the address family and a
string-representation of an IPv4 or IPv6 address and copies the network address
structure to `dst`.
```
struct sockaddr_in sa; // IPv4.
struct sockaddr_in6 sa6; // IPv6.

// This method returns -1 on error and 0 if address was messed up.
inet_pton(AF_INET, "10.12.110.57", &(sa.sin_addr)); // IPv4.
inet_pton(AF_INET6, "2001:db8:63b3:1::3490", &(sa6.sin6_addr)); // IPv6.
```
__Note:__ `inet_addr()` and `inet_aton()` has been deprecated in favor of the
methods detailed above.

The inverse of `inet_pton` is `inet_ntop` (network to presentation):
```
// IPv4.
char ip4[INET_ADDRSTRLEN]; // Space for IPv4 address.
struct socaddr_in sa; // Pretending this is loaded with something.
inet_ntop(AF_INET, &(sa.sin_addr), p4, INET_ADDRSTRLEN);
printf("The IPv4 address is: %s\n", ip4);

// IPv6.
char ip6[INET6_ADDRSTRLEN];
struct sockaddr_in6 sa6;
inet_ntop(AF_INET6, , &(sa6.sin6_addr), ip6, INET6_ADDRSTRLEN);
print("The address is: %s\n", ip6);
```

These don't work for hostname lookups, only for ip-addresses, for the former
use `getaddrinfo()`.

__Note:__ `inet_nto()` deprecated in favor for above.


## Private (Or Disconnected) Networks

_internal IP -> external IP_ is handled by __NAT__ (_Network Address
Translation_)

IPv6 have private networks to -> starts with `fdXX` (see RFC 4193).


# Jumping from IPv4 to IPv6

1. Use `getaddrinfo(const char * node, const char * service, const stsruct
   addrinfo * hints, struct addrinfo ** res)` to get all the `struct sockaddr`
   information -> will keep things IP-version-agnostic.
1. Use helper function instead of hard-coding.
1. `AF_INET -> AF_INET6`
1. `PF_INET -> PF_INET6`
1. `INADDR_ANY` assign -> `in6addr_any` assign
   ```
   struct sockaddr_in sa;
   struct sockaddr_in6 sa6;
   sa.sin_addr.s_addr = INADDR_ANY; // Use IPv4.
   sa6.sin6_addr = in6addr_any; // Use IPv6.
   ```
   `struct in6_addr` can also be initialized with the value IN6ADDR_ANY_INIT
   `struct in6_addr ia6 = IN6ADDR_ANY_INIT;`
1. `struct sockaddr_in` -> `struct socaddr_in6` _Note:_ no `sin6_zero` field.
1. `struct in_addr` -> `struct in6_addr`
1. `inet_pton` instead of `inet_aton` and `inet_addr`
1. `inet_ntop` instead of `inet_ntop`
1. `getaddrinfo` instead of `gethostbyname`
1. `getnameinfo` instead of `gethostbyaddr`
1. Use IPv6 multicast instead of `INADDR_BROADCAST`, no longer works.


# System Call or Bust

Examples don't include error-checking for brevity, check the standalone
versions for more complete examples.


## getaddrinfo() -- Prepare to launch!
```
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

int
getaddrinfo(
  const char * node,              // "www.example.com" or IP.
  const char * service,           // "http" or port-number.
  const struct addrifno * hints,
  struct addrinfo ** res
);
```

Sample call for preparing to listen on host-ip on port 3490:
```
int status;
struct addrinfo hints;
struct addrinfo * servinfo; // Will contain the results.

memset(&hints, 0, sizeof hints);  // Zero-out struct.
hints.ai_family = AF_UNSPEC;      // IPv6 or IPv4.
hints.ai_socktype = SOCK_STREAM;  // TCP stream socket.
hints.ai_flags = AI_PASSIVE;      // Auto-fill IP.

if ((status = getaddrinfo(NULL, "3490", &hints, &servinfo)) != 0) {
  fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
  exit(1);
}

// servinfo now points to a linked list of 1 or more addrinfo structs.

```

## socket() -- Get the File Descriptor!

Breakdown of the `socket()` systemcall:
```
#include <sys/types.h>
#include <sys/socket.h>

int socket(
  int domain,   // PF_INET | PF_INET6.
  int type,     // SOCK_STREAM | SOCK_DGRAM.
  int protocol  // Set to 0 in order to choose proper protocol for type.
);
```
Can be populated manually, but easier to call `getprotobyname()` together with
either 'tcp' or 'udp' respectively. Should use `AF_INET` with `struct
sockaddr_in` and `PF_INET` with `socket()` but they have the same value.


## Bind() -- What port am I on?

Given a socket, you might want to associate it with a port on the running host
by using `bind()`. If you are a server, this is done in combination with
`listen()` in order to check for incoming connections.
```
#include <sys/types.h>
#include <sys/socket.h>

int bind(
  int sockfd,               // The socket file-descriptor from socket().
  struct sockaddr * my_add, // Info abt. address, port, IP address.
  int addrlen               // Length of bytes of the address.
);
```
Example binds socket to the host port 3490:
```
struct addrinfo hints = {
  .ai_family = AF_UNSPEC,     // Don't care about IPv4/IPv6.
  .ai_socktype = SOCK_STREAM,
  .ai_flags = AI_PASSIVE      // Auto fill IP.
};

struct addrinfo * res = NULL;
getaddrinfo(NULL, "3490", &hints, &res);

// Make the socket.
int sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

// Bind it to the port.
bind(sockfd, res->ai_addr, res->ai_addrlen);
```
`bind()` returns `-1` on error and sets `errno`. Possible ports between
`1024-65535` barring any used ports.

Set socket options in order to allowing reuse of a socket:
```
int yes=1;
int result = setsockopt(
  listener,
  SOL_SOCKET,
  SO_REUSEADDR,
  &yes,
  sizeof yes
);
if (result == -1) {
  perror("setsockopt");
  exit(1);
}
```


## connect() -- Hey, you!

The `connect()` systemcall is used connect a socket to a specific address and
port.
```
#include <sys/types.h>
#include <sys/socket.h>

int connect(
  int sockfd,                  // File-desc, retuned from socket()
  struct sockaddr * serv_addr, // IP-addr + dest port.
  int addrlen                  // Byte-length of address.
);
```
All of the above can be gathered with `getaddrinfo()`.

Example of connecting to `http://www.example.com:3490`:
```
struct addrinfo hints = {
  .ai_family = AF_UNSPEC,
  .ai_socktype = SOCK_STREAM,
};
struct addrinfo * res = NULL;

getaddrinfo("www.example.com", "3490", &hints, &res);
int sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
connect(sockfd, res->ai_addr, res->ai_addrlen);
```
No need to call bind, since we only care about the outbound port since we're
not actively listning for connections.


## listen() -- Will somebody please call me?

You need to `listen()` and `accept()` in order to handle incoming connections.
```
int listen(
  int sockfd,   // Socket-descriptor returned from socket().
  int backlog   // Size of allowed queued connections.
);
```
`bind()` needs to be called before `listen()` in order to distinguish which
port should be re-directed to our software.


## accept() -- "Thank you for calling port 3490."

After calling `accept()` on an incoming connection, a new socket is created
representing this specific connection, with the original listening socket still
remaining.
```
#include <sys/types.h>
#include <sys/socket.h>

int accept(
  int sockfd,            // Descriptor from listen().
  struct socaddr * addr, // Usually points to local sockaddr_storage.
  socklen_t * addrlen    // Usually sizeof sockaddr_storage.
);
```
The function will at most write `addrlen` number of bytes to addr, if less is
written, the function fill set a new adjusted `addrlen` value.

```
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#incude <netdb.h>

#define MYPORT "3490"
#define BACKLOG 10

int
main(void)
{
  struct addrinfo hints = {
    .ai_family = AF_UNSPEC, // IPv4 or IPv6.
    .ai_socktype = SOCK_STREAM, // TCP.
    .ai_flags ` AI_PASSIVE // Auto-fill IP.
  };

  struct addrinfo * res = NULL;
  getaddrinfo(NULL, MYPORT, &hints, &res);
  int sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  bind(sockfd, res->ai_addr, res->ai_addrlen);
  listen(sockfd, BACKLOG);

  struct sockaddr_storage their_addr = {0};
  socklen_t addr_size = sizeof their_addr;
  int new_fd = accept(
    sockfd,
    (struct sockaddr *)&their_addr,
    &addr_size
  );
  // Ready to communicate on socked descriptor new_fd.
}

```


## send() and recv() -- Tank to me, baby!

This methods are use with stream sockets, for unconnected datagram sockets, use
`sendto()` and `recvfrom()`.

```
  int send(int sockfd, const void * msg, int len, int flags);
```

`send` returns an integer representing the number of bytes that were actually
sent, or `-1` for an error.

```
  int recv(int sockfd, void * buf, int len, int flags);
```

`recv` returns an integer representing bytes read into the buffer, 0 for a
closed connection or -1 for error.


## sendto() and recvfrom() -- Talk to me, DGRAM-style

These functions are used with unconnected datagram sockets. Since the socket is
not connected to a remote host, the destination address needs to be provided.
```
  int sendto(
    int sockfd,
    const void * msg,
    int len,
    unsigned int flags,
    const struct sockaddr * to,
    socklen_t tolen);
```
The same as `send()` but with the `to` and `tolen` added to the parameters,
where `to` is a pointer to a `struct sockaddr` and `tolen` an int which can be
set to `sizeof *to`.

```
  int recvfrom(
    int sockfd,
    void * buf,
    int len,
    unsigned int flags,
    struct sockaddr * from,
    int * fromlen
  );
```
Added parameters, `from` is a pointer to a local `struct sockaddr_storage`
filled with the IP address and port of the originating machine. `fromlen`
points to a local int, and will hold the actual length the address after the
function returns.

Need to use the `*_storage` variant, since a regular `struct sockaddr` is not
big enough. Also, if `connect()` is used on a datagram socket, `send()` and
`recv()` can be used as mentioned earlier.
