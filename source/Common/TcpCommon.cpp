#include "TcpCommon.hpp"
#include <chrono>
#include <cstdint>
#include <cstring>
#include <limits>
#if defined(_WIN32)
#include <WinSock2.h>
#include <Ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#endif

namespace ELITE {

constexpr static auto PLATFORM_EINTR =
#if defined(_WIN32)
    WSAEINTR;
#elif defined(__linux__)
    EINTR;
#endif

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

static inline int selectReadable(SocketHandle handle, unsigned timeout_ms) {
    fd_set read_set;
    FD_ZERO(&read_set);
    FD_SET(handle, &read_set);

    timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
#if defined(_WIN32)
    return ::select(0, &read_set, nullptr, nullptr, &tv);
#else
    return ::select(handle + 1, &read_set, nullptr, nullptr, &tv);
#endif
}

static inline int selectWritable(SocketHandle handle, unsigned timeout_ms) {
    fd_set write_set;
    FD_ZERO(&write_set);
    FD_SET(handle, &write_set);

    timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
#if defined(_WIN32)
    return ::select(0, nullptr, &write_set, nullptr, &tv);
#else
    return ::select(handle + 1, nullptr, &write_set, nullptr, &tv);
#endif
}

SocketIOStatus socketWaitReadable(SocketHandle handle, unsigned timeout_ms) {
    if (handle == SOCKET_INVALID_HANDLE) {
        return SocketIOStatus::ERROR;
    }

    while (true) {
        const int ready = selectReadable(handle, timeout_ms);
        if (ready > 0) {
            return SocketIOStatus::OK;
        }
        if (ready == 0) {
            return SocketIOStatus::TIMEOUT;
        }

        if (socketLastErrorCode() == PLATFORM_EINTR) {
            continue;
        }
        return SocketIOStatus::ERROR;
    }
}

SocketIOStatus socketWaitWritable(SocketHandle handle, unsigned timeout_ms) {
    if (handle == SOCKET_INVALID_HANDLE) {
        return SocketIOStatus::ERROR;
    }

    while (true) {
        const int ready = selectWritable(handle, timeout_ms);
        if (ready > 0) {
            return SocketIOStatus::OK;
        }
        if (ready == 0) {
            return SocketIOStatus::TIMEOUT;
        }

        if (socketLastErrorCode() == PLATFORM_EINTR) {
            continue;
        }
        return SocketIOStatus::ERROR;
    }
}

SocketIOStatus socketReceive(SocketHandle handle, void* buff, std::size_t size, std::size_t& received_size) {
    received_size = 0;
    if (size == 0) {
        return SocketIOStatus::OK;
    }
    if (buff == nullptr) {
        return SocketIOStatus::ERROR;
    }

#if defined(_WIN32)
    const int recv_size =
        static_cast<int>(size > static_cast<std::size_t>(std::numeric_limits<int>::max()) ? std::numeric_limits<int>::max() : size);
    const int n = ::recv(handle, reinterpret_cast<char*>(buff), recv_size, 0);
#else
    const ssize_t n = ::recv(handle, buff, size, 0);
#endif

    if (n > 0) {
        received_size = static_cast<std::size_t>(n);
        return SocketIOStatus::OK;
    }
    if (n == 0) {
        return SocketIOStatus::CLOSED;
    }

    const int ec = socketLastErrorCode();
    if (ec == PLATFORM_EINTR || socketWouldBlock(ec)) {
        return SocketIOStatus::TIMEOUT;
    }

    return SocketIOStatus::ERROR;
}

SocketIOStatus socketReceiveAll(SocketHandle handle, void* buff, std::size_t size, std::size_t& received_size, int timeout_ms) {
    received_size = 0;
    if (timeout_ms <= 0) {
        return SocketIOStatus::ERROR;
    }
    if (!buff) {
        return SocketIOStatus::ERROR;
    }

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    while (received_size < size) {
        const auto now = std::chrono::steady_clock::now();
        const auto remaining_ms = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count();
        if (remaining_ms <= 0) {
            return SocketIOStatus::TIMEOUT;
        }

        auto wait_ret = socketWaitReadable(handle, static_cast<unsigned>(remaining_ms));
        if(wait_ret != SocketIOStatus::OK) {
            return wait_ret;
        }

        int remaining_size = static_cast<int>(size - received_size);
        if (remaining_size <= 0) {
            break;
        }
        
#if defined(_WIN32)
        const int n = ::recv(handle, reinterpret_cast<char*>(buff) + received_size, static_cast<int>(remaining_size), 0);
#else
        const ssize_t n = ::recv(handle, static_cast<char*>(buff) + received_size, remaining_size, 0);
#endif

        if (n == 0) {
            // peer has performed an orderly shutdown
            return SocketIOStatus::CLOSED;
        } else if (n < 0) {
            const int ec = socketLastErrorCode();
            if (ec == PLATFORM_EINTR || socketWouldBlock(ec)) {
                continue;
            }
            return SocketIOStatus::ERROR;
        }

        received_size += n;
    }

    return SocketIOStatus::OK;
}

SocketIOStatus socketReceiveLine(SocketHandle handle, std::string& line, int timeout_ms) {
    if (timeout_ms <= 0) {
        return SocketIOStatus::ERROR;
    }

    line.clear();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    while (true) {
        const auto now = std::chrono::steady_clock::now();
        const auto remaining_ms = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count();
        if (remaining_ms <= 0) {
            return SocketIOStatus::TIMEOUT;
        }

        auto wait_ret = socketWaitReadable(handle, static_cast<unsigned>(remaining_ms));
        if(wait_ret != SocketIOStatus::OK) {
            return wait_ret;
        }
        char c = 0;
#if defined(_WIN32)
        const int n = ::recv(handle, &c, 1, 0);
#else
        const ssize_t n = ::recv(handle, &c, 1, 0);
#endif

        if (n == 0) {
            // peer has performed an orderly shutdown
            return SocketIOStatus::CLOSED;
        } else if (n < 0) {
            const int ec = socketLastErrorCode();
            if (ec == PLATFORM_EINTR || socketWouldBlock(ec)) {
                continue;
            }
            return SocketIOStatus::ERROR;
        }

        line.push_back(c);
        if (c == '\n') {
            return SocketIOStatus::OK;
        }
    }

    return SocketIOStatus::OK;
}

SocketIOStatus socketWrite(SocketHandle handle, const void* buff, std::size_t size, std::size_t& sent_size) {
    sent_size = 0;
    if (size == 0) {
        return SocketIOStatus::OK;
    }
    if (buff == nullptr) {
        return SocketIOStatus::ERROR;
    }

#if defined(_WIN32)
    const int send_size =
        static_cast<int>(size > static_cast<std::size_t>(std::numeric_limits<int>::max()) ? std::numeric_limits<int>::max() : size);
    const int n = ::send(handle, reinterpret_cast<const char*>(buff), send_size, 0);
#elif defined(__linux__)
    int flags = 0;
#if defined(MSG_NOSIGNAL)
    flags = MSG_NOSIGNAL;
#endif
    const ssize_t n = ::send(handle, buff, size, flags);
#else
    const ssize_t n = ::send(handle, buff, size, 0);
#endif

    if (n > 0) {
        sent_size = static_cast<std::size_t>(n);
        return SocketIOStatus::OK;
    }
    if (n == 0) {
        return SocketIOStatus::CLOSED;
    }

    const int ec = socketLastErrorCode();
    if (ec == PLATFORM_EINTR || socketWouldBlock(ec)) {
        return SocketIOStatus::TIMEOUT;
    }

    return SocketIOStatus::ERROR;
}

bool socketClientSetOptions(SocketHandle handle) {
    int yes = 1;
    int no = 0;

    if (::setsockopt(handle, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&yes), sizeof(yes)) != 0) {
        return false;
    }

    if (::setsockopt(handle, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&yes), sizeof(yes)) != 0) {
        return false;
    }

    if (::setsockopt(handle, SOL_SOCKET, SO_KEEPALIVE, reinterpret_cast<const char*>(&no), sizeof(no)) != 0) {
        return false;
    }

#if defined(__linux__)
    (void)::setsockopt(handle, IPPROTO_TCP, TCP_QUICKACK, reinterpret_cast<const char*>(&yes), sizeof(yes));
#endif
    return true;
}

bool socketClientSetPriority(SocketHandle handle, int priority) {
#if defined(__linux__)
    if (::setsockopt(handle, SOL_SOCKET, SO_PRIORITY, reinterpret_cast<const char*>(&priority), sizeof(priority)) != 0) {
        return false;
    }
#endif
    return true;
}

bool socketSetNonBlocking(SocketHandle handle, bool non_blocking) {
#if defined(_WIN32)
    u_long mode = non_blocking ? 1 : 0;
    if (::ioctlsocket(handle, FIONBIO, &mode) != 0) {
        return false;
    }
#else
    int flags = ::fcntl(handle, F_GETFL, 0);
    if (flags < 0) {
        return false;
    }
    if (non_blocking) {
        flags |= O_NONBLOCK;
    } else {
        flags &= ~O_NONBLOCK;
    }
    if (::fcntl(handle, F_SETFL, flags) != 0) {
        return false;
    }
#endif
    return true;
}

void socketClose(SocketHandle& handle) {
    if (handle == SOCKET_INVALID_HANDLE) {
        return;
    }

#if defined(_WIN32)
    (void)::shutdown(handle, SD_BOTH);
    (void)::closesocket(handle);
#else
    (void)::shutdown(handle, SHUT_RDWR);
    (void)::close(handle);
#endif
    handle = SOCKET_INVALID_HANDLE;
}

bool socketWouldBlock(int error_code) {
#if defined(_WIN32)
    return (error_code == WSAEWOULDBLOCK || error_code == WSAEINPROGRESS || error_code == WSAEALREADY);
#else
    return (error_code == EWOULDBLOCK || error_code == EAGAIN || error_code == EINPROGRESS || error_code == EALREADY);
#endif
}

SocketIOStatus socketWriteAll(SocketHandle handle, const void* buff, std::size_t size, std::size_t& sent_size) {
    sent_size = 0;
    if (size == 0) {
        return SocketIOStatus::OK;
    }
    if (buff == nullptr) {
        return SocketIOStatus::ERROR;
    }

    const auto* data = static_cast<const uint8_t*>(buff);
    while (sent_size < size) {
        std::size_t chunk_sent = 0;
        const SocketIOStatus status = socketWrite(handle, data + sent_size, size - sent_size, chunk_sent);
        if (status == SocketIOStatus::OK) {
            if (chunk_sent == 0) {
                return SocketIOStatus::CLOSED;
            }
            sent_size += chunk_sent;
            continue;
        }
        if (status == SocketIOStatus::TIMEOUT) {
            const SocketIOStatus wait_status = socketWaitWritable(handle, 1000);
            if (wait_status == SocketIOStatus::ERROR) {
                return SocketIOStatus::ERROR;
            }
            continue;
        }

        return status;
    }
    return SocketIOStatus::OK;
}

}  // namespace ELITE
