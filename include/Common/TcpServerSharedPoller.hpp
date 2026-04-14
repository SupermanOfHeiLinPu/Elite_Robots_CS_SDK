// SPDX-License-Identifier: MIT
// Copyright (c) 2025, Elite Robots.
#ifndef __TCP_SERVER_SHARED_POLLER_HPP__
#define __TCP_SERVER_SHARED_POLLER_HPP__

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_set>
#include <vector>

#include "TcpCommon.hpp"

namespace ELITE {

class TcpServerSharedPoller;

class TcpServerBase {
   public:
    TcpServerBase(int port, int recv_buf_size) : local_port_(port), recv_buf_size_(recv_buf_size) {}
    virtual ~TcpServerBase() = default;

   protected:
    friend class TcpServerSharedPoller;
    mutable std::mutex socket_mutex_;
    std::atomic<bool> listening_{false};
    SocketHandle listen_fd_{SOCKET_INVALID_HANDLE};
    SocketHandle client_fd_{SOCKET_INVALID_HANDLE};
    std::string local_ip_;
    int local_port_;
    std::string remote_ip_;
    int remote_port_{0};
    int recv_buf_size_;

   protected:
    virtual void onClientReadEvent() = 0;

    virtual void onAcceptEvent() = 0;

    virtual void stopListen() = 0;
};

class TcpServerSharedPoller {
   public:
    TcpServerSharedPoller();
    ~TcpServerSharedPoller();
    bool start();
    void shutdown();
    bool registerServer(TcpServerBase* server);
    bool unregisterServer(TcpServerBase* server);

    TcpServerSharedPoller(const TcpServerSharedPoller&) = delete;
    TcpServerSharedPoller& operator=(const TcpServerSharedPoller&) = delete;

   private:
    void eventLoop();
#if defined(__linux__) || defined(linux)
    void eventLoopLinux();
    void addEpollFd(int epoll_fd, SocketHandle fd, TcpServerBase* server, bool is_client);
    void delEpollFd(int epoll_fd, SocketHandle fd);
#elif defined(_WIN32)
    void eventLoopWindows();
#else
#error "TcpServerSharedPoller only supports Linux and Windows"
#endif

    std::atomic<bool> shutting_down_{false};
    std::atomic<bool> running_{false};
    std::unordered_set<TcpServerBase*> servers_;
    std::unique_ptr<std::thread> io_thread_;
};

}  // namespace ELITE

#endif
