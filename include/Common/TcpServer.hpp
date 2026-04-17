// SPDX-License-Identifier: MIT
// Copyright (c) 2025, Elite Robots.
#ifndef __TCP_SERVER_HPP__
#define __TCP_SERVER_HPP__

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include "TcpCommon.hpp"
#include "TcpServerSharedPoller.hpp"

namespace ELITE {

class TcpServer : public TcpServerBase {
   public:
    using ReceiveCallback = std::function<void(const uint8_t[], int)>;

    TcpServer(int port, int rev_msg_size);
    ~TcpServer();

    std::string lastError() const;

    void setReceiveCallback(ReceiveCallback cb);
    void unsetReceiveCallback();
    int writeClient(void* data, int size);
    bool startListen();
    bool isClientConnected();

   protected:
    // Event handlers called by TcpServerSharedPoller thread.
    virtual void onClientReadEvent();
    virtual void onAcceptEvent();
    virtual void stopListen();

   private:
    
    int rev_msg_size_;
    ReceiveCallback receive_cb_;
    std::mutex receive_cb_mutex_;
    std::atomic<bool> initialized_{false};
    std::string last_error_;

    bool initialize();
    int createBindListen();
    bool createAndBindListenSocketWithRetry();

    bool setSocketOptions(SocketHandle sock, bool is_server_socket);
    bool updateEndpointInfo(SocketHandle sock, bool is_local);
    void callReceiveCallback(const uint8_t data[], int size);
    int readSocket(uint8_t data[]);

    friend class TcpServerSharedPoller;
};

}  // namespace ELITE

#endif
