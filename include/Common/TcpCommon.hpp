#ifndef __ELITE_TCP_COMMON_HPP__
#define __ELITE_TCP_COMMON_HPP__

#include <string>

namespace ELITE {

using SocketHandle =
#if defined(_WIN32)
    unsigned long long;
#else
    int;
#endif

static constexpr SocketHandle SOCKET_INVALID_HANDLE =
#if defined(_WIN32)
    static_cast<SocketHandle>(~0ULL);
#else
    -1;
#endif

int socketLastErrorCode();

std::string socketErrorString(int error_code);

int socketReceive(SocketHandle handle, void* buff, std::size_t size);

int socketWrite(SocketHandle handle, const void* buff, std::size_t size);

}  // namespace ELITE

#endif  // __ELITE_TCP_COMMON_HPP__