// SPDX-License-Identifier: MIT
// Copyright (c) 2025, Elite Robots.
#include "TcpClient.hpp"

#include <chrono>
#include <cstring>

#if defined(_WIN32)
#include <Ws2tcpip.h>
#include <WinSock2.h>
#pragma comment(lib, "Ws2_32.lib")
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace ELITE {

TcpClient::TcpClient() : socket_fd_(SOCKET_INVALID_HANDLE) {}

TcpClient::~TcpClient() { close(); }

bool TcpClient::connect(const std::string& ip, int port, unsigned timeout_ms) {
    close();
    if (!createSocket()) {
        return false;
    }

    if (!socketClientSetOptions(socket_fd_)) {
        close();
        return false;
    }

    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));
    if (inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) != 1) {
        setLastError("Invalid IPv4 address: " + ip);
        close();
        return false;
    }

    if (!socketSetNonBlocking(socket_fd_, true)) {
        close();
        return false;
    }

    int ret = ::connect(socket_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    if (ret != 0) {
        const int ec = socketLastErrorCode();
        if (!socketWouldBlock(ec)) {
            setLastError("Socket connect failed: " + socketErrorString(ec));
            close();
            return false;
        }

        SocketIOStatus wait_status = socketWaitWritable(socket_fd_, timeout_ms);
        if (wait_status != SocketIOStatus::OK) {
            if (wait_status == SocketIOStatus::TIMEOUT) {
                setLastError("Socket connect timeout");
            } else {
                setLastError("Socket wait writable failed: " + socketErrorString(socketLastErrorCode()));
            }
            close();
            return false;
        }

        if (!checkConnectResult()) {
            close();
            return false;
        }
    }

    if (!socketSetNonBlocking(socket_fd_, false)) {
        close();
        return false;
    }
    return true;
}

void TcpClient::close() {
    socketClose(socket_fd_);
}

bool TcpClient::isOpen() const { return socket_fd_ != SOCKET_INVALID_HANDLE; }

std::size_t TcpClient::available() const {
    if (!isOpen()) {
        return 0;
    }
#if defined(_WIN32)
    u_long bytes = 0;
    if (::ioctlsocket(socket_fd_, FIONREAD, &bytes) != 0) {
        return 0;
    }
    return static_cast<std::size_t>(bytes);
#else
    int bytes = 0;
    if (::ioctl(socket_fd_, FIONREAD, &bytes) != 0 || bytes < 0) {
        return 0;
    }
    return static_cast<std::size_t>(bytes);
#endif
}

std::string TcpClient::localIP() const {
    if (!isOpen()) {
        return "";
    }

    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    SockLenType len = sizeof(addr);
    if (::getsockname(socket_fd_, reinterpret_cast<sockaddr*>(&addr), &len) != 0) {
        return "";
    }

    char ip[INET_ADDRSTRLEN] = {0};
    if (::inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip)) == nullptr) {
        return "";
    }
    return std::string(ip);
}

SocketIOStatus TcpClient::sendAll(const uint8_t* data, std::size_t size) {
    if (!isOpen()) {
        setLastError("Socket is not open");
        return SocketIOStatus::ERROR;
    }

    std::size_t sent_size = 0;
    const SocketIOStatus write_status = socketWriteAll(socket_fd_, data, size, sent_size);
    if (write_status == SocketIOStatus::ERROR) {
        setLastError("Socket send failed: " + socketErrorString(socketLastErrorCode()));
    }
    return write_status;
}

SocketIOStatus TcpClient::receiveAll(uint8_t* data, std::size_t size, unsigned timeout_ms) {
    if (!isOpen()) {
        setLastError("Socket is not open");
        return SocketIOStatus::ERROR;
    }
    if (size == 0) {
        return SocketIOStatus::OK;
    }
    if (data == nullptr) {
        setLastError("Socket receive buffer is null");
        return SocketIOStatus::ERROR;
    }

    const auto start = std::chrono::steady_clock::now();
    std::size_t total_received = 0;
    while (total_received < size) {
        int wait_ms = 0;
        if (timeout_ms == 0) {
            wait_ms = 0;
        } else {
            const auto now = std::chrono::steady_clock::now();
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
            if (elapsed >= static_cast<long long>(timeout_ms)) {
                setLastError("Socket receive timeout");
                return SocketIOStatus::TIMEOUT;
            }
            wait_ms = static_cast<int>(timeout_ms - static_cast<unsigned>(elapsed));
        }

        std::size_t chunk_received = 0;
        const SocketIOStatus read_status =
            socketReceive(socket_fd_, data + total_received, size - total_received, chunk_received, wait_ms);
        if (read_status == SocketIOStatus::OK) {
            total_received += chunk_received;
            continue;
        }
        if (read_status == SocketIOStatus::TIMEOUT) {
            setLastError("Socket receive timeout");
            return read_status;
        }
        if (read_status == SocketIOStatus::ERROR) {
            setLastError("Socket receive failed: " + socketErrorString(socketLastErrorCode()));
        }
        return read_status;
    }

    return SocketIOStatus::OK;
}

SocketIOStatus TcpClient::sendAll(const std::string& data) {
    return sendAll(reinterpret_cast<const uint8_t*>(data.data()), data.size());
}

SocketIOStatus TcpClient::receiveLine(std::string& line, unsigned timeout_ms) {
    line.clear();
    if (!isOpen()) {
        setLastError("Socket is not open");
        return SocketIOStatus::ERROR;
    }

    const auto start = std::chrono::steady_clock::now();
    while (true) {
        int wait_ms = 0;
        if (timeout_ms == 0) {
            wait_ms = 0;
        } else {
            const auto now = std::chrono::steady_clock::now();
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
            if (elapsed >= static_cast<long long>(timeout_ms)) {
                setLastError("Socket receive line timeout");
                return SocketIOStatus::TIMEOUT;
            }
            wait_ms = static_cast<int>(timeout_ms - static_cast<unsigned>(elapsed));
        }

        char c = 0;
        std::size_t received_size = 0;
        const SocketIOStatus read_status = socketReceive(socket_fd_, &c, 1, received_size, wait_ms);
        if (read_status == SocketIOStatus::OK) {
            if (received_size == 0) {
                return SocketIOStatus::CLOSED;
            }
            line.push_back(c);
            if (c == '\n') {
                return SocketIOStatus::OK;
            }
            continue;
        }

        if (read_status == SocketIOStatus::TIMEOUT) {
            setLastError("Socket receive line timeout");
            return read_status;
        }
        if (read_status == SocketIOStatus::ERROR) {
            setLastError("Socket receive line failed: " + socketErrorString(socketLastErrorCode()));
        }
        return read_status;
    }
}

bool TcpClient::createSocket() {
    close();
    socket_fd_ = static_cast<SocketHandle>(::socket(AF_INET, SOCK_STREAM, 0));
    if (!isOpen()) {
        setLastError("Create socket failed: " + socketErrorString(socketLastErrorCode()));
        return false;
    }
    return true;
}

bool TcpClient::checkConnectResult() {
    int connect_error = 0;
    SockLenType len = sizeof(connect_error);
    if (::getsockopt(socket_fd_, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&connect_error), &len) != 0) {
        setLastError("Get SO_ERROR failed: " + socketErrorString(socketLastErrorCode()));
        return false;
    }

    if (connect_error != 0) {
        setLastError("Socket connect failed: " + socketErrorString(connect_error));
        return false;
    }

    return true;
}

void TcpClient::setLastError(const std::string& message) { last_error_ = message; }

}  // namespace ELITE
