#include "TcpCommon.hpp"
#include <cstring>
#if defined(_WIN32)
#include <Ws2tcpip.h>
#include <WinSock2.h>
#pragma comment(lib, "Ws2_32.lib")
#else
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace ELITE
{

#if defined(_WIN32)
class WinSockInitializer {
   public:
    WinSockInitializer() {
        WSADATA data;
        initialized_ = (WSAStartup(MAKEWORD(2, 2), &data) == 0);
    }

    ~WinSockInitializer() {
        if (initialized_) {
            WSACleanup();
        }
    }

   private:
    bool initialized_{false};
};

WinSockInitializer g_winsock_initializer;
#endif

int socketLastErrorCode() {
#if defined(_WIN32)
    return WSAGetLastError();
#else
    return errno;
#endif
}

std::string socketErrorString(int error_code) {
#if defined(_WIN32)
    return std::to_string(error_code);
#else
    return std::strerror(error_code);
#endif
}

int socketReceive(SocketHandle handle, void* buff, std::size_t size) {
#if defined(_WIN32)
    return ::recv(client_fd_, reinterpret_cast<char*>(buff), static_cast<int>(size), 0);
#else
    return static_cast<int>(::recv(handle, buff, size, 0));
#endif
}

int socketWrite(SocketHandle handle, const void* buff, std::size_t size) {
#if defined(_WIN32)
        return ::send(handle, reinterpret_cast<const char*>(buff), static_cast<int>(size), 0);
#else
        return static_cast<int>(::send(handle, buff, size, 0));
#endif
}

} // namespace ELITE


