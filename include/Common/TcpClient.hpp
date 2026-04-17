// SPDX-License-Identifier: MIT
// Copyright (c) 2025, Elite Robots.
#ifndef __TCP_CLIENT_HPP__
#define __TCP_CLIENT_HPP__

#include <cstddef>
#include <cstdint>
#include <string>
#include "TcpCommon.hpp"

namespace ELITE {

class TcpClient {
   public:
    TcpClient();
    ~TcpClient();

    TcpClient(const TcpClient&) = delete;
    TcpClient& operator=(const TcpClient&) = delete;

    bool connect(const std::string& ip, int port, unsigned timeout_ms = 500);
    void close();

    bool isOpen() const;
    std::size_t available() const;
    std::string localIP() const;

    SocketIOStatus sendAll(const uint8_t* data, std::size_t size);
    SocketIOStatus sendAll(const std::string& data);
    SocketIOStatus receiveAll(uint8_t* data, std::size_t size, unsigned timeout_ms);
    SocketIOStatus receiveLine(std::string& line, unsigned timeout_ms);

    const std::string& lastError() const { return last_error_; }

   private:
    bool createSocket();
    bool checkConnectResult();

    void setLastError(const std::string& message);

    SocketHandle socket_fd_;
    std::string last_error_;
};

}  // namespace ELITE

#endif
