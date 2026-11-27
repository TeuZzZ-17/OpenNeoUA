// Minimal non-blocking UDP sockets for ZNDNet: the subset of SDL2_net the engine used.
// IPaddress keeps SDL_net's layout (host and port in network byte order), so packets,
// address comparisons and netplay with older builds stay unchanged.
#ifndef NETLIB_UDPSOCKET_H_INCLUDED
#define NETLIB_UDPSOCKET_H_INCLUDED

#include <cstdint>

struct IPaddress
{
    uint32_t host = 0;
    uint16_t port = 0;
};

struct UDPpacket
{
    int channel = -1;
    uint8_t *data = nullptr;
    int len = 0;
    int maxlen = 0;
    int status = 0;
    IPaddress address;
};

typedef struct UdpSocketImpl *UDPsocket;

namespace Net
{
// Starts the socket layer (Winsock on Windows). True when sockets are usable.
bool Init();

// Binds a non-blocking broadcast-capable UDP socket; port in host byte order,
// 0 = any free port. nullptr on failure.
UDPsocket UdpOpen(uint16_t port);
void UdpClose(UDPsocket sock);

// Sends packet->len bytes of packet->data to packet->address. True on success.
bool UdpSend(UDPsocket sock, const UDPpacket *packet);

// Fills the nullptr-terminated packet array without blocking. Returns how many
// packets arrived (0 when none), -1 on a socket error.
int UdpRecv(UDPsocket sock, UDPpacket **packets);

// Resolves a host name or dotted IPv4 address; port in host byte order.
// A null host means "any address". False when the name is unknown.
bool ResolveHost(IPaddress *out, const char *host, uint16_t port);
}

#endif // NETLIB_UDPSOCKET_H_INCLUDED
