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
    std::unique_ptr<boost::asio::ip::tcp::acceptor> acceptor_;
    std::shared_ptr<boost::asio::io_context> io_context_;

   private:
    std::shared_ptr<boost::asio::ip::tcp::socket> socket_;
    std::vector<uint8_t> read_buffer_;
    ReceiveCallback receive_cb_;
    std::mutex socket_mutex_;

    static std::unique_ptr<std::thread> s_server_thread_;
    static std::shared_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> s_work_guard_ptr_;
    static std::shared_ptr<boost::asio::io_context> s_io_context_ptr_;

    virtual void doAccept();
    void doRead(std::shared_ptr<boost::asio::ip::tcp::socket> sock);
    void closeSocket(std::shared_ptr<boost::asio::ip::tcp::socket> sock, boost::system::error_code& ec);
};

}  // namespace ELITE
#endif
