#include "TcpServer.hpp"
#include <iostream>
#include "Log.hpp"

namespace ELITE {

TcpServer::TcpServer(int port, int recv_buf_size)
    : acceptor_(getContext(), boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port)), read_buffer_(recv_buf_size) {}

TcpServer::~TcpServer() {
    if (acceptor_.is_open()) {
        boost::system::error_code ec;
        acceptor_.close(ec);
    }
    std::lock_guard<std::mutex> lock(socket_mutex_);
    if (socket_) {
        boost::system::error_code ec;
        if (socket_->is_open()) {
            socket_->close(ec);
        }
        socket_.reset();
    }
}

void TcpServer::setReceiveCallback(ReceiveCallback cb) { receive_cb_ = std::move(cb); }

void TcpServer::startListen() { doAccept(); }

void TcpServer::doAccept() {
    auto new_socket = std::make_shared<boost::asio::ip::tcp::socket>(getContext());
    std::weak_ptr<TcpServer> weak_self = shared_from_this();
    // Accept call back
    auto accept_cb = [weak_self, new_socket](boost::system::error_code ec) {
        boost::system::error_code ignore_ec;
        if (auto self = weak_self.lock()) {
            if (!ec) {
                std::lock_guard<std::mutex> lock(self->socket_mutex_);
                // Close old connection
                if (self->socket_ && self->socket_->is_open()) {
                    auto local_point = self->socket_->local_endpoint(ignore_ec);
                    auto remote_point = self->socket_->remote_endpoint(ignore_ec);
                    self->socket_->cancel(ignore_ec);
                    self->socket_->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignore_ec);
                    self->socket_->close(ignore_ec);
                    ELITE_LOG_INFO("TCP port %d has new connection and close old client: %s:%d %s", local_point.port(),
                                   remote_point.address().to_string().c_str(), remote_point.port(),
                                   boost::system::system_error(ignore_ec).what());
                }
                // Update alive socket
                self->socket_ = new_socket;
                auto local_point = self->socket_->local_endpoint(ignore_ec);
                auto remote_point = self->socket_->remote_endpoint(ignore_ec);
                ELITE_LOG_INFO("TCP port %d accept client: %s:%d %s", local_point.port(),
                               remote_point.address().to_string().c_str(), remote_point.port(),
                               boost::system::system_error(ec).what());
                // Start async read
                self->doRead(new_socket);
            } else {
                std::lock_guard<std::mutex> lock(self->socket_mutex_);
                // Close old connection
                if (self->socket_ && self->socket_->is_open()) {
                    auto local_point = self->socket_->local_endpoint(ignore_ec);
                    auto remote_point = self->socket_->remote_endpoint(ignore_ec);
                    self->socket_->cancel(ignore_ec);
                    self->socket_->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignore_ec);
                    self->socket_->close(ignore_ec);
                    ELITE_LOG_ERROR("TCP port %d accept new connection fail(%s), and close old connection %s:%d %s",
                                    local_point.port(), boost::system::system_error(ec).what(),
                                    remote_point.address().to_string().c_str(), remote_point.port(),
                                    boost::system::system_error(ignore_ec).what());
                }
                self->socket_.reset();
            }
            self->doAccept();
        }
    };

    acceptor_.listen(1);
    acceptor_.set_option(boost::asio::socket_base::reuse_address(true));
    acceptor_.async_accept(*new_socket, accept_cb);
}

void TcpServer::doRead(std::shared_ptr<boost::asio::ip::tcp::socket> sock) {
    std::weak_ptr<TcpServer> weak_self = shared_from_this();
    auto read_cb = [weak_self, sock](boost::system::error_code ec, std::size_t n) {
        if (auto self = weak_self.lock()) {
            if (!ec) {
                if (self->receive_cb_) {
                    self->receive_cb_(self->read_buffer_.data(), n);
                }
                // Continue read
                self->doRead(sock);
            } else {
                if (sock->is_open()) {
                    boost::system::error_code ignore_ec;
                    auto local_point = sock->local_endpoint(ignore_ec);
                    auto remote_point = sock->remote_endpoint(ignore_ec);
                    sock->cancel(ignore_ec);
                    sock->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignore_ec);
                    sock->close(ignore_ec);
                    ELITE_LOG_INFO("TCP port %d close client: %s:%d %s. Reason: %s", local_point.port(),
                                   remote_point.address().to_string().c_str(), remote_point.port(),
                                   boost::system::system_error(ignore_ec).what(), boost::system::system_error(ec).what());
                }
            }
        }
    };
    boost::asio::async_read(*sock, boost::asio::buffer(read_buffer_), read_cb);
}

void TcpServer::start() {
    if (getServerThread()) {
        return;
    }
    getWorkGurad().reset(
        new boost::asio::executor_work_guard<boost::asio::io_context::executor_type>(boost::asio::make_work_guard(getContext())));
    getServerThread().reset(new std::thread([&]() {
        try {
            if (getContext().stopped()) {
                getContext().restart();
            }
            getContext().run();
            ELITE_LOG_INFO("TCP server exit thread");
        } catch (const boost::system::system_error& e) {
            ELITE_LOG_FATAL("TCP server thread error: %s", e.what());
        }
    }));
}

void TcpServer::stop() {
    getWorkGurad()->reset();
    getContext().stop();
    if (getServerThread() && getServerThread()->joinable()) {
        getServerThread()->join();
    }
    getWorkGurad().reset();
    getServerThread().reset();
}

void TcpServer::releaseClient() {
    std::weak_ptr<TcpServer> weak_self = shared_from_this();
    boost::asio::post(getContext(), [weak_self]() {
        if (auto self = weak_self.lock()) {
            std::lock_guard<std::mutex> lock(self->socket_mutex_);
            if (self->socket_) {
                if (self->socket_->is_open()) {
                    self->socket_->close();
                }
                self->socket_.reset();
            }
        }
    });
}

int TcpServer::writeClient(void* data, int size) {
    std::lock_guard<std::mutex> lock(socket_mutex_);
    if (socket_) {
        return boost::asio::write(*socket_, boost::asio::buffer(data, size));
    }
    return -1;
}

bool TcpServer::isClientConnected() {
    std::lock_guard<std::mutex> lock(socket_mutex_);
    if (socket_) {
        return socket_->is_open();
    }
    return false;
}

}  // namespace ELITE
