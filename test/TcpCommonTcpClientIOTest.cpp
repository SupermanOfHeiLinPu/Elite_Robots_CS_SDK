#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <string>
#include <thread>

#include "Common/TcpClient.hpp"
#include "Common/TcpCommon.hpp"

#if !defined(_WIN32)
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

using namespace ELITE;

#if !defined(_WIN32)
namespace {

volatile sig_atomic_t g_signal_seen = 0;

void testSignalHandler(int) { g_signal_seen = 1; }

class ScopedSignalHandler {
   public:
    explicit ScopedSignalHandler(int signal_no) : signal_no_(signal_no) {
        std::memset(&new_action_, 0, sizeof(new_action_));
        std::memset(&old_action_, 0, sizeof(old_action_));
        new_action_.sa_handler = testSignalHandler;
        sigemptyset(&new_action_.sa_mask);
        new_action_.sa_flags = 0;
        (void)::sigaction(signal_no_, &new_action_, &old_action_);
    }

    ~ScopedSignalHandler() { (void)::sigaction(signal_no_, &old_action_, nullptr); }

   private:
    int signal_no_;
    struct sigaction new_action_;
    struct sigaction old_action_;
};

class LocalTcpServer {
   public:
    LocalTcpServer() = default;
    ~LocalTcpServer() { stop(); }

    LocalTcpServer(const LocalTcpServer&) = delete;
    LocalTcpServer& operator=(const LocalTcpServer&) = delete;

    bool start() {
        listen_fd_ = static_cast<SocketHandle>(::socket(AF_INET, SOCK_STREAM, 0));
        if (listen_fd_ == SOCKET_INVALID_HANDLE) {
            return false;
        }

        int yes = 1;
        (void)::setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&yes), sizeof(yes));

        sockaddr_in addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = 0;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (::bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
            stop();
            return false;
        }

        if (::listen(listen_fd_, 1) != 0) {
            stop();
            return false;
        }

        SockLenType len = sizeof(addr);
        if (::getsockname(listen_fd_, reinterpret_cast<sockaddr*>(&addr), &len) != 0) {
            stop();
            return false;
        }
        port_ = static_cast<int>(ntohs(addr.sin_port));

        accept_thread_ = std::thread([this]() {
            sockaddr_in peer;
            std::memset(&peer, 0, sizeof(peer));
            SockLenType peer_len = sizeof(peer);
            SocketHandle accepted = static_cast<SocketHandle>(
                ::accept(listen_fd_, reinterpret_cast<sockaddr*>(&peer), &peer_len));
            if (accepted != SOCKET_INVALID_HANDLE) {
                accepted_fd_ = accepted;
                accepted_.store(true, std::memory_order_release);
            }
        });

        return true;
    }

    int port() const { return port_; }

    bool waitAccepted(std::chrono::milliseconds timeout) const {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            if (accepted_.load(std::memory_order_acquire)) {
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        return accepted_.load(std::memory_order_acquire);
    }

    void stop() {
        socketClose(listen_fd_);
        if (accept_thread_.joinable()) {
            accept_thread_.join();
        }
        socketClose(accepted_fd_);
    }

   private:
    SocketHandle listen_fd_{SOCKET_INVALID_HANDLE};
    SocketHandle accepted_fd_{SOCKET_INVALID_HANDLE};
    int port_{0};
    std::atomic<bool> accepted_{false};
    std::thread accept_thread_;
};

}  // namespace
#endif

TEST(TCP_COMMON_IO, SocketWaitReadableTimeout) {
#if defined(_WIN32)
    GTEST_SKIP() << "POSIX-only test";
#else
    int fds[2] = {-1, -1};
    ASSERT_EQ(::socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);

    SocketHandle read_fd = static_cast<SocketHandle>(fds[0]);
    SocketHandle peer_fd = static_cast<SocketHandle>(fds[1]);

    const auto start = std::chrono::steady_clock::now();
    const SocketIOStatus status = socketWaitReadable(read_fd, 30);
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);

    EXPECT_EQ(status, SocketIOStatus::TIMEOUT);
    EXPECT_GE(elapsed.count(), 20);

    socketClose(read_fd);
    socketClose(peer_fd);
#endif
}

TEST(TCP_COMMON_IO, SocketWaitReadableInterruptedThenTimeout) {
#if defined(_WIN32)
    GTEST_SKIP() << "POSIX-only test";
#else
    int fds[2] = {-1, -1};
    ASSERT_EQ(::socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);

    SocketHandle read_fd = static_cast<SocketHandle>(fds[0]);
    SocketHandle peer_fd = static_cast<SocketHandle>(fds[1]);

    ScopedSignalHandler signal_guard(SIGUSR1);
    g_signal_seen = 0;

    const pthread_t target_thread = pthread_self();
    std::thread interrupter([target_thread]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        (void)::pthread_kill(target_thread, SIGUSR1);
    });

    const SocketIOStatus status = socketWaitReadable(read_fd, 50);
    interrupter.join();

    EXPECT_EQ(status, SocketIOStatus::TIMEOUT);
    EXPECT_EQ(g_signal_seen, 1);

    socketClose(read_fd);
    socketClose(peer_fd);
#endif
}

TEST(TCP_CLIENT_IO, ReceiveAllTimeout) {
#if defined(_WIN32)
    GTEST_SKIP() << "POSIX-only test";
#else
    LocalTcpServer server;
    ASSERT_TRUE(server.start());

    TcpClient client;
    ASSERT_TRUE(client.connect("127.0.0.1", server.port(), 200));
    ASSERT_TRUE(server.waitAccepted(std::chrono::milliseconds(200)));

    std::array<uint8_t, 4> data{};
    const SocketIOStatus status = client.receiveAll(data.data(), data.size(), 50);

    EXPECT_EQ(status, SocketIOStatus::TIMEOUT);
    EXPECT_NE(client.lastError().find("timeout"), std::string::npos);

    client.close();
    server.stop();
#endif
}

TEST(TCP_CLIENT_IO, ReceiveAllInterruptedThenTimeout) {
#if defined(_WIN32)
    GTEST_SKIP() << "POSIX-only test";
#else
    LocalTcpServer server;
    ASSERT_TRUE(server.start());

    TcpClient client;
    ASSERT_TRUE(client.connect("127.0.0.1", server.port(), 200));
    ASSERT_TRUE(server.waitAccepted(std::chrono::milliseconds(200)));

    ScopedSignalHandler signal_guard(SIGUSR1);
    g_signal_seen = 0;

    const pthread_t target_thread = pthread_self();
    std::thread interrupter([target_thread]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        (void)::pthread_kill(target_thread, SIGUSR1);
    });

    std::array<uint8_t, 4> data{};
    const SocketIOStatus status = client.receiveAll(data.data(), data.size(), 60);
    interrupter.join();

    EXPECT_EQ(status, SocketIOStatus::TIMEOUT);
    EXPECT_EQ(g_signal_seen, 1);

    client.close();
    server.stop();
#endif
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
