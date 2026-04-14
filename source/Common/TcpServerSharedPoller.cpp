// SPDX-License-Identifier: MIT
// Copyright (c) 2025, Elite Robots.
#include "TcpServerSharedPoller.hpp"
#include "TcpCommon.hpp"

#include <chrono>
#include <string>
#include <thread>
#include <vector>

#if defined(_WIN32)
#include <WinSock2.h>
#include <Ws2tcpip.h>
#else
#include <sys/epoll.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#endif

#include "Log.hpp"

namespace ELITE {

TcpServerSharedPoller::TcpServerSharedPoller() {}

bool TcpServerSharedPoller::start() {
    if (running_.load()) {
        return false;
    }
    running_.store(true);

    try {
        io_thread_.reset(new std::thread([this]() { eventLoop(); }));
    } catch (...) {
        running_.store(false);
        return false;
    }
    return true;
}

bool TcpServerSharedPoller::registerServer(TcpServerBase* server) {
    if (running_.load()) {
        return false;
    }
    if (server) {
        servers_.insert(server);
    }
    return true;
}

bool TcpServerSharedPoller::unregisterServer(TcpServerBase* server) {
    if (running_.load()) {
        return false;
    }
    if (server) {
        servers_.erase(server);
    }
    return true;
}

void TcpServerSharedPoller::eventLoop() {
#if defined(__linux__) || defined(linux)
    eventLoopLinux();
#elif defined(_WIN32)
    eventLoopWindows();
#else
#error "TcpServerSharedPoller::eventLoop only supports Linux and Windows"
#endif
}

#if defined(__linux__) || defined(linux)
void TcpServerSharedPoller::addEpollFd(int epoll_fd, SocketHandle fd, TcpServerBase* server, bool is_client) {
    if (fd == SOCKET_INVALID_HANDLE) {
        return;
    }
    epoll_event ev{};
    ev.events = EPOLLIN;
    uintptr_t raw = reinterpret_cast<uintptr_t>(server);
    if (is_client) {
        raw |= static_cast<uintptr_t>(1);
    }
    ev.data.ptr = reinterpret_cast<void*>(raw);
    (void)::epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev);
}

void TcpServerSharedPoller::delEpollFd(int epoll_fd, SocketHandle fd) {
    if (fd == SOCKET_INVALID_HANDLE) {
        return;
    }
    (void)::epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
}

void TcpServerSharedPoller::eventLoopLinux() {
    int epoll_fd = ::epoll_create1(0);
    if (epoll_fd < 0) {
        ELITE_LOG_ERROR("Create epoll instance fail: %s", socketErrorString(socketLastErrorCode()).c_str());
        return;
    }

    // Initial registration of existing servers.
    for (auto server : servers_) {
        if (!server) {
            continue;
        }
        addEpollFd(epoll_fd, server->listen_fd_, server, false);
    }

    while (running_.load()) {
        constexpr int poll_idle_wait_time_ms = 200;
        epoll_event events[64];
        int n = ::epoll_wait(epoll_fd, events, 64, poll_idle_wait_time_ms);
        for (int i = 0; i < n; ++i) {
            uintptr_t raw = reinterpret_cast<uintptr_t>(events[i].data.ptr);
            bool is_client = (raw & static_cast<uintptr_t>(1)) != 0;
            TcpServerBase* server = reinterpret_cast<TcpServerBase*>(raw & ~static_cast<uintptr_t>(1));
            if (!server || !server->listening_.load()) {
                continue;
            }
            if (is_client) {
                server->onClientReadEvent();
            } else {
                // Accept event may change the client fd, so need to update epoll registration.
                SocketHandle old_client_fd = server->client_fd_;
                if (old_client_fd != SOCKET_INVALID_HANDLE) {
                    delEpollFd(epoll_fd, old_client_fd);
                }
                server->onAcceptEvent();
                if (server->client_fd_ != SOCKET_INVALID_HANDLE) {
                    addEpollFd(epoll_fd, server->client_fd_, server, true);
                }
            }
        }
    }
    ::close(epoll_fd);
}
#elif defined(_WIN32)
void TcpServerSharedPoller::eventLoopWindows() {
    while (running_.load()) {
        auto servers = snapshotServers();
        if (servers.empty()) {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait_for(lock, POLL_IDLE_WAIT, [this]() { return !running_.load() || !servers_.empty(); });
            continue;
        }

        fd_set readfds;
        FD_ZERO(&readfds);
        std::vector<std::pair<TcpServer*, bool> > ready_map;

        for (auto* server : servers) {
            if (!server || !server->listening_.load()) {
                continue;
            }
            std::lock_guard<std::mutex> lock(server->socket_mutex_);
            if (server->listen_fd_ != TcpServer::INVALID_SOCKET_HANDLE) {
                FD_SET(static_cast<SOCKET>(server->listen_fd_), &readfds);
                ready_map.emplace_back(server, false);
            }
            if (server->client_fd_ != TcpServer::INVALID_SOCKET_HANDLE) {
                FD_SET(static_cast<SOCKET>(server->client_fd_), &readfds);
                ready_map.emplace_back(server, true);
            }
        }

        if (ready_map.empty()) {
            std::this_thread::sleep_for(POLL_IDLE_WAIT);
            continue;
        }

        timeval tv;
        tv.tv_sec = static_cast<long>(POLL_IDLE_WAIT.count() / 1000);
        tv.tv_usec = static_cast<long>((POLL_IDLE_WAIT.count() % 1000) * 1000);
        int n = ::select(0, &readfds, nullptr, nullptr, &tv);
        if (n <= 0) {
            continue;
        }

        for (const auto& item : ready_map) {
            TcpServer* server = item.first;
            bool is_client = item.second;
            if (!server || !server->listening_.load()) {
                continue;
            }
            SOCKET fd = static_cast<SOCKET>(is_client ? server->client_fd_ : server->listen_fd_);
            if (fd != INVALID_SOCKET && FD_ISSET(fd, &readfds)) {
                if (is_client) {
                    server->onClientReadEvent();
                } else {
                    server->onAcceptEvent();
                }
            }
        }
    }
}
#endif

void TcpServerSharedPoller::shutdown() {
    if (shutting_down_.exchange(true)) {
        return;
    }

    running_.store(false);
    if (io_thread_ && io_thread_->joinable()) {
        io_thread_->join();
    }
    io_thread_.reset();

    for (auto* server : servers_) {
        if (!server) {
            continue;
        }
        server->stopListen();
    }
}

TcpServerSharedPoller::~TcpServerSharedPoller() { shutdown(); }

}  // namespace ELITE
