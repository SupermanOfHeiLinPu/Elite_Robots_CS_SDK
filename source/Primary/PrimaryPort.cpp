#include "PrimaryPort.hpp"
#include "EliteException.hpp"
#include "Utils.hpp"
#include "Log.hpp"

using namespace std::chrono;

namespace ELITE
{
using namespace std::chrono;

PrimaryPort::PrimaryPort() {
    message_head_.resize(HEAD_LENGTH);
}

PrimaryPort::~PrimaryPort() {
    disconnect();
}


bool PrimaryPort::connect(const std::string& ip, int port) {
    socketConnect(ip, port);
    if (!socket_async_thread_) {
        std::lock_guard<std::mutex> lock(socket_mutex_);
        // Start async thread
        socket_async_thread_alive_ = true;
        socket_async_thread_.reset(new std::thread([&](std::string ip, int port){
            socketAsyncLoop(ip, port);
        }, ip, port));
    }
    return true;
}

void PrimaryPort::disconnect() {
    // Close socket and set thread flag
    {
        std::lock_guard<std::mutex> lock(socket_mutex_);
        socket_async_thread_alive_ = false;
        socket_ptr_.reset();
    }
    if (socket_async_thread_ && socket_async_thread_->joinable()) {
        socket_async_thread_->join();
    }
    socket_async_thread_.reset();
}

bool PrimaryPort::sendScript(const std::string& script) {
    std::lock_guard<std::mutex> lock(socket_mutex_);
    if (!socket_ptr_) {
        ELITE_LOG_ERROR("Don't connect to robot primary port");
        return false;
    }
    auto script_with_newline = std::make_shared<std::string>(script + "\n");
    boost::system::error_code ec;
    socket_ptr_->write_some(boost::asio::buffer(*script_with_newline), ec);
    if (ec) {
        ELITE_LOG_ERROR("Send script to robot fail : ", boost::system::system_error(ec).what());
        return false;
    } else {
        return true;
    }
}

bool PrimaryPort::getPackage(std::shared_ptr<PrimaryPackage> pkg, int timeout_ms) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        parser_sub_msg_.insert({pkg->getType(), pkg});
    }

    return pkg->waitUpdate(timeout_ms);
}

bool PrimaryPort::parserMessage() {
    std::lock_guard<std::mutex> lock(socket_mutex_);
    if (!socket_ptr_ || !socket_ptr_->is_open()) {
        ELITE_LOG_WARN("Don't connect to robot primary port");
        return false;
    }
    if (socket_ptr_->available() <= HEAD_LENGTH) {
        return true;
    }
    // Receive package head and parser it
    boost::system::error_code ec;
    int head_len = boost::asio::read(*socket_ptr_, boost::asio::buffer(message_head_, HEAD_LENGTH), ec);
    if (ec) {
        ELITE_LOG_ERROR("Primary port receive package head had expection: %s", boost::system::system_error(ec).what());
        return false;
    }
    uint32_t package_len = 0;
    UTILS::EndianUtils::unpack(message_head_.begin(), package_len);
    if (package_len <= HEAD_LENGTH) {
        ELITE_LOG_ERROR("Primary port package len error: %d", package_len);
        return false;
    }

    return parserMessageBody(message_head_[4], package_len);
}

bool PrimaryPort::parserMessageBody(int type, int package_len) {
    boost::system::error_code ec;
    int body_len = package_len - HEAD_LENGTH;
    message_body_.resize(body_len);
    // Receive package body
    boost::asio::read(*socket_ptr_, boost::asio::buffer(message_body_, body_len), ec);
    if (ec) {
        ELITE_LOG_ERROR("Primary port receive package body had expection: %s", boost::system::system_error(ec).what());
        return false;
    }
    // If RobotState message parser others don't do anything.
    if (type == ROBOT_STATE_MSG_TYPE) {
        uint32_t sub_len = 0;
        for (auto iter = message_body_.begin(); iter < message_body_.end(); iter += sub_len) {
            UTILS::EndianUtils::unpack(iter, sub_len);
            int sub_type = *(iter + 4);

            std::lock_guard<std::mutex> lock(mutex_);
            auto psm = parser_sub_msg_.find(sub_type);
            if (psm != parser_sub_msg_.end()) {
                psm->second->parser(sub_len, iter);
                psm->second->notifyUpated();
                parser_sub_msg_.erase(sub_type);
            }
        }
    }
    return true;
}

void PrimaryPort::socketAsyncLoop(const std::string& ip, int port) {
    while (socket_async_thread_alive_) {
        try {
            if (!parserMessage()) {
                socketConnect(ip, port);
            }
            std::this_thread::sleep_for(10ms);
        } catch(const std::exception& e) {
            ELITE_LOG_ERROR("Primary port async loop throw exception:%s", e.what());
        }
    }
}

bool PrimaryPort::socketConnect(const std::string& ip, int port) {
    try {
        std::lock_guard<std::mutex> lock(socket_mutex_);
        socket_ptr_.reset(new boost::asio::ip::tcp::socket(io_context_));
        socket_ptr_->open(boost::asio::ip::tcp::v4());
        socket_ptr_->set_option(boost::asio::ip::tcp::no_delay(true));
        socket_ptr_->set_option(boost::asio::socket_base::reuse_address(true));
        socket_ptr_->set_option(boost::asio::socket_base::keep_alive(false));
#if defined(__linux) || defined(linux) || defined(__linux__)
        socket_ptr_->set_option(boost::asio::detail::socket_option::boolean<IPPROTO_TCP, TCP_QUICKACK>(true));
#endif
        boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::make_address(ip), port);
        boost::system::error_code connect_ec;
        socket_ptr_->async_connect(endpoint, [&](const boost::system::error_code& ec){
            connect_ec = ec;
        });
        if (io_context_.stopped()) {
            io_context_.restart();
        }
        io_context_.run_for(std::chrono::steady_clock::duration(500ms));
        if (connect_ec) {
            socket_ptr_.reset();
            ELITE_LOG_ERROR("Connect to robot primary port fail: %s", boost::system::system_error(connect_ec).what());
            return false;
        }
    } catch(const boost::system::system_error &error) {
        throw EliteException(EliteException::Code::SOCKET_CONNECT_FAIL, error.what());
        return false;
    }
    return true;
}


}