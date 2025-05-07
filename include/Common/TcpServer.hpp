#ifndef __TCP_SERVER_HPP__
#define __TCP_SERVER_HPP__

#include <atomic>
#include <boost/asio.hpp>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace ELITE {

class TcpServer : public std::enable_shared_from_this<TcpServer> {
   public:
    using ReceiveCallback = std::function<void(const uint8_t[], int)>;

    TcpServer(int port, int recv_buf_size);
    ~TcpServer();

    static void start();
    static void stop();

    void setReceiveCallback(ReceiveCallback cb);
    void releaseClient();
    int writeClient(void* data, int size);
    void startListen();
    bool isClientConnected();

   protected:
    static boost::asio::io_context& getContext() {
        static boost::asio::io_context io_context;
        return io_context;
    }
    static std::shared_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>>& getWorkGurad() {
        static std::shared_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> work_guard_ptr;
        return work_guard_ptr;
    }
    static std::unique_ptr<std::thread>& getServerThread() {
        static std::unique_ptr<std::thread> server_thread;
        return server_thread;
    }

    boost::asio::ip::tcp::acceptor acceptor_;
    std::shared_ptr<boost::asio::ip::tcp::socket> socket_;
    std::vector<uint8_t> read_buffer_;
    ReceiveCallback receive_cb_;
    std::mutex socket_mutex_;

    virtual void doAccept();
    void doRead(std::shared_ptr<boost::asio::ip::tcp::socket> sock);
};

}  // namespace ELITE
#endif
