// SPDX-License-Identifier: MIT
// Copyright (c) 2025, Elite Robots.
#include "TcpServer.hpp"
#include "Log.hpp"

#include <chrono>
#include <cstring>
#include <thread>

#if defined(_WIN32)
#include <WinSock2.h>
#include <Ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#endif

namespace ELITE {

TcpServer::TcpServer(int port, int recv_buf_size) : TcpServerBase(port, recv_buf_size) {}

int TcpServer::createBindListen() {
    listen_fd_ = static_cast<SocketHandle>(::socket(AF_INET, SOCK_STREAM, 0));
    if (listen_fd_ == SOCKET_INVALID_HANDLE) {
        return socketLastErrorCode();
    }
    setSocketOptions(listen_fd_, true);
    setNonBlocking(listen_fd_, true);
    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(local_port_));
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (::bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0) {
        return 0;
    } else {
        return socketLastErrorCode();
    }
}

bool TcpServer::createAndBindListenSocketWithRetry() {
    int last_error = 0;
    static constexpr int bind_retry_times = 30;
    static constexpr auto bind_retry_interval = std::chrono::milliseconds(100);
    for (int i = 0; i < bind_retry_times; i++) {
        int error_code = createBindListen();
        if (error_code == 0) {
            last_error = 0;
            break;
        }
        closeSocket(listen_fd_);

        if (last_error != error_code) {
            last_error = error_code;
            ELITE_LOG_WARN("TCP port %d bind fail %s, retry bind %d/%d", local_port_, socketErrorString(last_error), i,
                           bind_retry_times);
        }

        std::this_thread::sleep_for(bind_retry_interval);
    }

    if (last_error != 0 || listen_fd_ == SOCKET_INVALID_HANDLE) {
        last_error_ = "Create TCP server on port " + std::to_string(local_port_) + " fail: " + socketErrorString(last_error);
        ELITE_LOG_FATAL("%s", last_error_.c_str());
        return false;
    }

    return true;
}

bool TcpServer::initialize() {
    if (initialized_.load()) {
        return true;
    }

    std::lock_guard<std::mutex> lock(socket_mutex_);
    if (!createAndBindListenSocketWithRetry()) {
        return false;
    }

    if (::listen(listen_fd_, 1) != 0) {
        const int ec = socketLastErrorCode();
        last_error_ = "TCP port " + std::to_string(local_port_) + " listen fail: " + socketErrorString(ec);
        closeSocket(listen_fd_);
        ELITE_LOG_FATAL("%s", last_error_.c_str());
        return false;
    }

    updateEndpointInfo(listen_fd_, true);
    last_error_.clear();
    initialized_.store(true);
    return true;
}

TcpServer::~TcpServer() { stopListen(); }

void TcpServer::setReceiveCallback(ReceiveCallback cb) {
    std::lock_guard<std::mutex> lock(receive_cb_mutex_);
    receive_cb_ = std::move(cb);
}

void TcpServer::unsetReceiveCallback() {
    std::lock_guard<std::mutex> lock(receive_cb_mutex_);
    receive_cb_ = nullptr;
}

bool TcpServer::startListen() {
    if (listening_.exchange(true)) {
        return false;
    }

    if (!initialize()) {
        listening_.store(false);
        ELITE_LOG_ERROR("TCP port %d initialize fail: %s", local_port_, lastError().c_str());
        return false;
    }

    return true;
}

std::string TcpServer::lastError() const {
    std::lock_guard<std::mutex> lock(socket_mutex_);
    return last_error_;
}

void TcpServer::stopListen() {
    if (!listening_.exchange(false)) {
        return;
    }
    closeSocket(client_fd_);
    closeSocket(listen_fd_);
}

void TcpServer::onAcceptEvent() {
    std::lock_guard<std::mutex> lock(socket_mutex_);
    if (!listening_.load() || listen_fd_ == SOCKET_INVALID_HANDLE) {
        return;
    }

    sockaddr_in remote_addr;
    std::memset(&remote_addr, 0, sizeof(remote_addr));
#if defined(_WIN32)
    int addr_len = sizeof(remote_addr);
#else
    socklen_t addr_len = sizeof(remote_addr);
#endif

    SocketHandle new_client = static_cast<SocketHandle>(::accept(listen_fd_, reinterpret_cast<sockaddr*>(&remote_addr), &addr_len));
    if (new_client == SOCKET_INVALID_HANDLE) {
        const int ec = socketLastErrorCode();
        if (!wouldBlockError(ec)) {
            ELITE_LOG_ERROR("TCP port %d accept fail: %s", local_port_, socketErrorString(ec).c_str());
        }
        return;
    }

    if (client_fd_ != SOCKET_INVALID_HANDLE) {
        ELITE_LOG_INFO("TCP port %d has new connection and close old client: %s:%d", local_port_, remote_ip_.c_str(), remote_port_);
        closeSocket(client_fd_);
    }

    setSocketOptions(new_client, false);
    setNonBlocking(new_client, true);
    client_fd_ = new_client;
    updateEndpointInfo(client_fd_, false);
    ELITE_LOG_INFO("TCP port %d accept client: %s:%d", local_port_, remote_ip_.c_str(), remote_port_);
}

int TcpServer::readSocket(uint8_t read_buf[]) {
    std::lock_guard<std::mutex> lock(socket_mutex_);
    if (!listening_.load() || client_fd_ == SOCKET_INVALID_HANDLE) {
        return -1;
    }
    int read_len = socketReceive(client_fd_, read_buf, recv_buf_size_);
    if (read_len <= 0) {
        const int ec = socketLastErrorCode();
        if (!wouldBlockError(ec)) {
            ELITE_LOG_INFO("TCP port %d close client: %s:%d. Reason: %s", local_port_, remote_ip_.c_str(), remote_port_,
                           socketErrorString(ec).c_str());
            closeSocket(client_fd_);
            return read_len;
        }
    }
    return read_len;
}

void TcpServer::onClientReadEvent() { 
    std::vector<uint8_t> read_buf(recv_buf_size_); 
    int read_len = readSocket(read_buf.data());
    if (read_len > 0) {
        callReceiveCallback(read_buf.data(), read_len);
    }
}

int TcpServer::writeClient(void* data, int size) {
    std::lock_guard<std::mutex> lock(socket_mutex_);
    if (client_fd_ == SOCKET_INVALID_HANDLE || size <= 0) {
        return -1;
    }

    int total = 0;
    uint8_t* bytes = reinterpret_cast<uint8_t*>(data);
    while (total < size) {
        int n = socketWrite(client_fd_, bytes + total, size - total);
        if (n <= 0) {
            const int ec = socketLastErrorCode();
            if (wouldBlockError(ec)) {
                continue;
            }
            ELITE_LOG_INFO("Port %d write TCP client fail: %s", local_port_, socketErrorString(ec).c_str());
            return -1;
        }
        total += n;
    }
    return total;
}

bool TcpServer::isClientConnected() {
    std::lock_guard<std::mutex> lock(socket_mutex_);
    return client_fd_ != SOCKET_INVALID_HANDLE;
}

void TcpServer::closeSocket(SocketHandle& sock) {
    if (sock == SOCKET_INVALID_HANDLE) {
        return;
    }
#if defined(_WIN32)
    ::shutdown(sock, SD_BOTH);
    ::closesocket(sock);
#else
    ::shutdown(sock, SHUT_RDWR);
    ::close(sock);
#endif
    sock = SOCKET_INVALID_HANDLE;
}

bool TcpServer::setNonBlocking(SocketHandle sock, bool non_blocking) {
#if defined(_WIN32)
    u_long mode = non_blocking ? 1 : 0;
    return (::ioctlsocket(sock, FIONBIO, &mode) == 0);
#else
    int flags = ::fcntl(sock, F_GETFL, 0);
    if (flags < 0) {
        return false;
    }
    if (non_blocking) {
        flags |= O_NONBLOCK;
    } else {
        flags &= ~O_NONBLOCK;
    }
    return (::fcntl(sock, F_SETFL, flags) == 0);
#endif
}

bool TcpServer::setSocketOptions(SocketHandle sock, bool is_server_socket) {
    int yes = 1;
    int keepalive = 1;

    (void)::setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&yes), sizeof(yes));
    if (!is_server_socket) {
        (void)::setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&yes), sizeof(yes));
        (void)::setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, reinterpret_cast<const char*>(&keepalive), sizeof(keepalive));
#if defined(__linux__) || defined(linux)
        (void)::setsockopt(sock, IPPROTO_TCP, TCP_QUICKACK, reinterpret_cast<const char*>(&yes), sizeof(yes));
        int priority = 6;
        (void)::setsockopt(sock, SOL_SOCKET, SO_PRIORITY, reinterpret_cast<const char*>(&priority), sizeof(priority));
#endif
    }
    return true;
}

bool TcpServer::updateEndpointInfo(SocketHandle sock, bool is_local) {
    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
#if defined(_WIN32)
    int len = sizeof(addr);
#else
    socklen_t len = sizeof(addr);
#endif

    int ret = is_local ? ::getsockname(sock, reinterpret_cast<sockaddr*>(&addr), &len)
                       : ::getpeername(sock, reinterpret_cast<sockaddr*>(&addr), &len);
    if (ret != 0) {
        return false;
    }

    char ip[INET_ADDRSTRLEN] = {0};
    if (::inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip)) == nullptr) {
        return false;
    }

    if (is_local) {
        local_ip_ = ip;
        local_port_ = ntohs(addr.sin_port);
    } else {
        remote_ip_ = ip;
        remote_port_ = ntohs(addr.sin_port);
    }
    return true;
}

bool TcpServer::wouldBlockError(int error_code) const {
#if defined(_WIN32)
    return (error_code == WSAEWOULDBLOCK || error_code == WSAEINPROGRESS || error_code == WSAEALREADY || error_code == WSAEINTR);
#else
    return (error_code == EAGAIN || error_code == EWOULDBLOCK || error_code == EINTR);
#endif
}

void TcpServer::callReceiveCallback(const uint8_t data[], int size) {
    std::lock_guard<std::mutex> lock(receive_cb_mutex_);
    if (receive_cb_) {
        receive_cb_(data, size);
    }
}

}  // namespace ELITE
