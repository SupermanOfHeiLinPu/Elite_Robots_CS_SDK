#include "TcpServer.hpp"
#include <iostream>
#include "Log.hpp"

namespace ELITE {

TcpServer::TcpServer(int port, int recv_buf_size)
    : acceptor_(getContext(), boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port)), read_buffer_(recv_buf_size) {}

TcpServer::~TcpServer() {
    std::lock_guard<std::mutex> lock(socket_mutex_);
    if (socket_) {
        boost::system::error_code ec;
        socket_->close(ec);
    }
    acceptor_.close();
}

void TcpServer::setReceiveCallback(ReceiveCallback cb) { receive_cb_ = std::move(cb); }

void TcpServer::startListen() { doAccept(); }

void TcpServer::doAccept() {
    // Accept call back
    auto accept_cb = [self = shared_from_this()](boost::system::error_code ec, boost::asio::ip::tcp::socket sock) {
        if (!ec) {
            std::lock_guard<std::mutex> lock(self->socket_mutex_);
            if (self->socket_) {
                self->socket_->close();
            }
            self->socket_.reset(new boost::asio::ip::tcp::socket(std::move(sock)));
        } else {
            std::lock_guard<std::mutex> lock(self->socket_mutex_);
            self->socket_.reset();
        }
        self->doRead();
        self->doAccept();
    };

    acceptor_.listen(1);
    acceptor_.set_option(boost::asio::socket_base::reuse_address(true));
    acceptor_.async_accept(getContext(), accept_cb);
}

void TcpServer::doRead() {
    std::lock_guard<std::mutex> lock(socket_mutex_);
    if (!socket_) {
        return;
    }
    auto read_cb = [self = shared_from_this()](boost::system::error_code ec, std::size_t n) {
        if (!ec) {
            if (self->receive_cb_) {
                self->receive_cb_(self->read_buffer_.data(), n);
            }
            self->doRead();
        } else {
            std::cout << boost::system::system_error(ec).what() << std::endl;
            boost::system::error_code ignore_ec;
            self->socket_->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignore_ec);
            self->socket_->close(ignore_ec);
        }
    };
    boost::asio::async_read(*socket_, boost::asio::buffer(read_buffer_), read_cb);
}

void TcpServer::start() {
    if (getServerThread()) {
        return;
    }
    getServerThread().reset(new std::thread([&]() {
        try {
            getWorkGurad();
            getContext().run();
            ELITE_LOG_INFO("TCP server exit thread");
        } catch (const boost::system::system_error& e) {
            ELITE_LOG_INFO("TCP server thread error: %s", e.what());
        }
    }));
}

void TcpServer::stop() {
    getWorkGurad().reset();
    getContext().stop();
    if (getServerThread() && getServerThread()->joinable()) {
        getServerThread()->join();
    }

    getServerThread().reset();
}

void TcpServer::releaseClient() {
    auto self = shared_from_this();
    boost::asio::post(getContext(), [self]() {
        std::lock_guard<std::mutex> lock(self->socket_mutex_);
        if (self->socket_) {
            self->socket_->close();
            self->socket_.reset();
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
