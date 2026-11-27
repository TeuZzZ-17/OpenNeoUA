// Minimal non-blocking UDP sockets (Winsock / POSIX), see udpsocket.h.
#include "udpsocket.h"

#include <cstring>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET NativeSocket;
static const NativeSocket NO_SOCKET = INVALID_SOCKET;
#else
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int NativeSocket;
static const NativeSocket NO_SOCKET = -1;
#endif

struct UdpSocketImpl
{
    NativeSocket fd = NO_SOCKET;
};

namespace Net
{

static void CloseNative(NativeSocket fd)
{
#ifdef _WIN32
    closesocket(fd);
#else
    close(fd);
#endif
}

static bool WouldBlock()
{
#ifdef _WIN32
    const int err = WSAGetLastError();
    // WSAECONNRESET: an earlier datagram hit a closed port (ICMP); not fatal for UDP.
    return err == WSAEWOULDBLOCK || err == WSAECONNRESET;
#else
    return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR;
#endif
}

bool Init()
{
#ifdef _WIN32
    static bool started = false;
    if ( !started )
    {
        WSADATA data;
        started = WSAStartup(MAKEWORD(2, 2), &data) == 0;
    }
    return started;
#else
    return true;
#endif
}

UDPsocket UdpOpen(uint16_t port)
{
    NativeSocket fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if ( fd == NO_SOCKET )
        return nullptr;

    sockaddr_in local;
    std::memset(&local, 0, sizeof(local));
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = htonl(INADDR_ANY);
    local.sin_port = htons(port);

    const int yes = 1;
    setsockopt(fd, SOL_SOCKET, SO_BROADCAST, (const char *)&yes, sizeof(yes));

#ifdef _WIN32
    u_long nonBlocking = 1;
    const bool configured = ioctlsocket(fd, FIONBIO, &nonBlocking) == 0;
#else
    const int flags = fcntl(fd, F_GETFL, 0);
    const bool configured = flags >= 0 && fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
#endif

    if ( !configured || bind(fd, (const sockaddr *)&local, sizeof(local)) != 0 )
    {
        CloseNative(fd);
        return nullptr;
    }

    UdpSocketImpl *sock = new UdpSocketImpl;
    sock->fd = fd;
    return sock;
}

void UdpClose(UDPsocket sock)
{
    if ( !sock )
        return;
    CloseNative(sock->fd);
    delete sock;
}

bool UdpSend(UDPsocket sock, const UDPpacket *packet)
{
    if ( !sock || !packet || packet->len < 0 )
        return false;

    sockaddr_in to;
    std::memset(&to, 0, sizeof(to));
    to.sin_family = AF_INET;
    to.sin_addr.s_addr = packet->address.host;
    to.sin_port = packet->address.port;
    return sendto(sock->fd, (const char *)packet->data, packet->len, 0,
                  (const sockaddr *)&to, sizeof(to)) == packet->len;
}

int UdpRecv(UDPsocket sock, UDPpacket **packets)
{
    if ( !sock || !packets )
        return -1;

    int count = 0;
    for (; packets[count]; ++count)
    {
        UDPpacket *pkt = packets[count];
        sockaddr_in from;
        socklen_t fromLen = sizeof(from);
        const int got = (int)recvfrom(sock->fd, (char *)pkt->data, pkt->maxlen, 0,
                                      (sockaddr *)&from, &fromLen);
        if ( got < 0 )
            return WouldBlock() ? count : (count ? count : -1);

        pkt->len = got;
        pkt->channel = -1;
        pkt->status = got;
        pkt->address.host = from.sin_addr.s_addr;
        pkt->address.port = from.sin_port;
    }
    return count;
}

bool ResolveHost(IPaddress *out, const char *host, uint16_t port)
{
    if ( !out )
        return false;

    out->port = htons(port);
    if ( !host )
    {
        out->host = htonl(INADDR_ANY);
        return true;
    }

    in_addr numeric;
    if ( inet_pton(AF_INET, host, &numeric) == 1 )
    {
        out->host = numeric.s_addr;
        return true;
    }

    addrinfo hints;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    addrinfo *found = nullptr;
    if ( getaddrinfo(host, nullptr, &hints, &found) != 0 || !found )
    {
        out->host = htonl(INADDR_NONE);
        return false;
    }

    out->host = ((const sockaddr_in *)found->ai_addr)->sin_addr.s_addr;
    freeaddrinfo(found);
    return true;
}

}
