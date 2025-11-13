// SPDX-License-Identifier: MIT
// Copyright (c) 2025, Elite Robots.
#include "SerialCommunication.hpp"
#include "Log.hpp"

#include <boost/asio.hpp>
#include <mutex>

namespace ELITE {

class SerialCommunication::Impl {
   public:
    Impl();
    ~Impl();

    int tcp_port_;
    std::string ip_;
    std::mutex socket_mutex_;
    std::unique_ptr<boost::asio::io_context> io_context_;
    std::unique_ptr<boost::asio::ip::tcp::socket> socket_ptr_;
    std::shared_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> work_guard_ptr_;
};

SerialCommunication::Impl::Impl() {}

SerialCommunication::Impl::~Impl() {}

SerialCommunication::SerialCommunication(const std::string& ip, int port) {
    impl_ = std::make_unique<Impl>();
    impl_->tcp_port_ = port;
    impl_->ip_ = ip;
}

SerialCommunication::~SerialCommunication() {}

bool SerialCommunication::connect(int timeout_ms) {
    disconnect();
    try {
        std::lock_guard<std::mutex> lock(impl_->socket_mutex_);
        impl_->io_context_ = std::make_unique<boost::asio::io_context>();
        impl_->work_guard_ptr_ = std::make_unique<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>>(
            boost::asio::make_work_guard(*impl_->io_context_));
        impl_->socket_ptr_ = std::make_unique<boost::asio::ip::tcp::socket>(*impl_->io_context_);
        impl_->socket_ptr_->open(boost::asio::ip::tcp::v4());
        boost::asio::ip::tcp::no_delay no_delay_option(true);
        impl_->socket_ptr_->set_option(no_delay_option);
        boost::asio::socket_base::reuse_address sol_reuse_option(true);
        impl_->socket_ptr_->set_option(sol_reuse_option);
#if defined(__linux) || defined(linux) || defined(__linux__)
        boost::asio::detail::socket_option::boolean<IPPROTO_TCP, TCP_QUICKACK> quickack(true);
        impl_->socket_ptr_->set_option(quickack);
#endif
        boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::make_address(impl_->ip_), impl_->tcp_port_);
        boost::system::error_code ec = boost::asio::error::would_block;
        impl_->socket_ptr_->async_connect(endpoint, [&](const boost::system::error_code& error) { ec = error; });
        impl_->io_context_->run_for(std::chrono::milliseconds(timeout_ms));
        if (ec) {
            ELITE_LOG_ERROR("Serial connect to robot fail: %s", boost::system::system_error(ec).what());
            return false;
        }
    } catch (const boost::system::system_error& error) {
        ELITE_LOG_ERROR("Serial connect to robot fail: %s", error.what());
        return false;
    }
    return true;
}

void SerialCommunication::disconnect() {
    std::lock_guard<std::mutex> lock(impl_->socket_mutex_);
    if (impl_->socket_ptr_ && impl_->socket_ptr_->is_open()) {
        try {
            boost::system::error_code ignore_ec;
            impl_->socket_ptr_->cancel(ignore_ec);
            impl_->socket_ptr_->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignore_ec);
            impl_->socket_ptr_->close(ignore_ec);
            if (impl_->io_context_->stopped()) {
                impl_->io_context_->restart();
            }
            impl_->io_context_->run_for(std::chrono::milliseconds(500));
            impl_->work_guard_ptr_->reset();

            impl_->work_guard_ptr_.reset();
            impl_->io_context_.reset();
            impl_->socket_ptr_.reset();

        } catch (const std::exception& e) {
            ELITE_LOG_WARN("Serial port socket disconnect throw exception:%s", e.what());
        }
    }
}

bool SerialCommunication::isConnected() {
    std::lock_guard<std::mutex> lock(impl_->socket_mutex_);
    if (impl_->socket_ptr_ && impl_->socket_ptr_->is_open()) {
        return true;
    }
    return false;
}

int SerialCommunication::write(const uint8_t* data, size_t size) {
    std::lock_guard<std::mutex> lock(impl_->socket_mutex_);
    if (!impl_->socket_ptr_) {
        return -1;
    } else if(!impl_->socket_ptr_->is_open()) {
        return -1;
    }
    boost::system::error_code ec;
    int ret = boost::asio::write(*impl_->socket_ptr_, boost::asio::buffer(data, size), ec);
    if (ec) {
        ELITE_LOG_DEBUG("Serial socket send fail: %s", ec.message().c_str());
        return -1;
    }
    return ret;
}

int SerialCommunication::read(uint8_t* data, size_t size, int timeout_ms) {
    std::lock_guard<std::mutex> lock(impl_->socket_mutex_);
    if (!impl_->socket_ptr_) {
        return -1;
    } else if(!impl_->socket_ptr_->is_open()) {
        return -1;
    }
    int read_len = 0;
    boost::system::error_code error_code;
    if (timeout_ms <= 0) {
        read_len = boost::asio::read(*impl_->socket_ptr_, boost::asio::buffer(data, size), error_code);
        if (error_code && read_len <= 0) {
            ELITE_LOG_ERROR("Serial socket receive fail: %s", error_code.message().c_str());
            return -1;
        }
        return read_len;
    } else {
        boost::asio::async_read(*impl_->socket_ptr_, boost::asio::buffer(data, size),
                                [&](const boost::system::error_code& ec, std::size_t nb) {
                                    error_code = ec;
                                    read_len = nb;
                                });
        impl_->io_context_->run_for(std::chrono::milliseconds(timeout_ms));
        if (error_code && read_len <= 0) {
            ELITE_LOG_ERROR("Serial socket receive fail: %s", error_code.message().c_str());
            return -1;
        }
        return read_len;
    }
}

}  // namespace ELITE
