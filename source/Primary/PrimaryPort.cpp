// SPDX-License-Identifier: MIT
// Copyright (c) 2025, Elite Robots.
#include "PrimaryPort.hpp"
#include "Log.hpp"
#include "TcpClient.hpp"
#include "Utils.hpp"

using namespace std::chrono;

namespace ELITE {
using namespace std::chrono;

PrimaryPort::PrimaryPort() : message_head_received_(0) { message_head_.resize(HEAD_LENGTH); }

PrimaryPort::~PrimaryPort() { disconnect(); }

bool PrimaryPort::connect(const std::string& ip, int port) {
    std::lock_guard<std::mutex> lock(socket_mutex_);
    if (!socketConnect(ip, port)) {
        return false;
    }
    if (!socket_async_thread_) {
        // Start async thread
        socket_async_thread_alive_ = true;
        socket_async_thread_.reset(new std::thread([&](std::string ip, int port) { socketAsyncLoop(ip, port); }, ip, port));
    }
    return true;
}

void PrimaryPort::disconnect() {
    // Close socket and set thread flag
    {
        std::lock_guard<std::mutex> lock(socket_mutex_);
        socket_async_thread_alive_ = false;
        socketDisconnect();
        tcp_client_.reset();
    }
    if (socket_async_thread_ && socket_async_thread_->joinable()) {
        socket_async_thread_->join();
    }
    socket_async_thread_.reset();
}

bool PrimaryPort::sendScript(const std::string& script) {
    std::lock_guard<std::mutex> lock(socket_mutex_);
    if (!tcp_client_ || !tcp_client_->isOpen()) {
        ELITE_LOG_ERROR("Don't connect to robot primary port");
        return false;
    }
    const std::string script_with_newline = script + "\n";
    const SocketIOStatus write_status = tcp_client_->sendAll(script_with_newline);
    if (write_status != SocketIOStatus::OK) {
        ELITE_LOG_ERROR("Send script to robot fail : %s", tcp_client_->lastError().c_str());
        return false;
    }
    return true;
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
    if (!tcp_client_ || !tcp_client_->isOpen()) {
        return false;
    }

    // Receive package head without dropping partial data in non-blocking mode.
    while (message_head_received_ < message_head_.size()) {
        SocketIOStatus read_status = tcp_client_->receiveAll(&message_head_[message_head_received_], 1, 0);
        if (read_status == SocketIOStatus::OK) {
            ++message_head_received_;
            continue;
        }
        if (read_status == SocketIOStatus::TIMEOUT) {
            return true;
        }
        ELITE_LOG_ERROR("Primary port receive package head had expection: %s", tcp_client_->lastError().c_str());
        message_head_received_ = 0;
        return false;
    }

    message_head_received_ = 0;

    uint32_t package_len = 0;
    EndianUtils::unpack(message_head_.begin(), package_len);
    if (package_len <= HEAD_LENGTH) {
        ELITE_LOG_ERROR("Primary port package len error: %d", package_len);
        return false;
    }

    return parserMessageBody(message_head_[4], package_len);
}

RobotErrorSharedPtr PrimaryPort::parserRobotError(uint64_t timestamp, RobotError::Source source,
                                                  const std::vector<uint8_t>& msg_body, int offset) {
    int32_t code = 0;
    EndianUtils::unpack(msg_body.begin() + offset, code);
    offset += sizeof(int32_t);

    int32_t sub_code = 0;
    EndianUtils::unpack(msg_body.begin() + offset, sub_code);
    offset += sizeof(int32_t);

    int32_t level = 0;
    EndianUtils::unpack(msg_body.begin() + offset, level);
    offset += sizeof(int32_t);

    uint32_t data_type;
    EndianUtils::unpack(msg_body.begin() + offset, data_type);
    offset += sizeof(uint32_t);

    switch ((RobotError::DataType)data_type) {
        case RobotError::DataType::NONE:
        case RobotError::DataType::UNSIGNED:
        case RobotError::DataType::HEX: {
            uint32_t data;
            EndianUtils::unpack(msg_body.begin() + offset, data);
            return std::make_shared<RobotError>(timestamp, code, sub_code, static_cast<RobotError::Source>(source),
                                                static_cast<RobotError::Level>(level), static_cast<RobotError::DataType>(data_type),
                                                data);

        } break;
        case RobotError::DataType::SIGNED:
        case RobotError::DataType::JOINT: {
            int32_t data;
            EndianUtils::unpack(msg_body.begin() + offset, data);
            return std::make_shared<RobotError>(timestamp, code, sub_code, static_cast<RobotError::Source>(source),
                                                static_cast<RobotError::Level>(level), static_cast<RobotError::DataType>(data_type),
                                                data);
        } break;
        case RobotError::DataType::STRING: {
            std::string data;
            for (size_t i = offset; i < msg_body.size(); i++) {
                data.push_back(msg_body[i]);
            }

            return std::make_shared<RobotError>(timestamp, code, sub_code, static_cast<RobotError::Source>(source),
                                                static_cast<RobotError::Level>(level), static_cast<RobotError::DataType>(data_type),
                                                data);
        } break;
        case RobotError::DataType::FLOAT: {
            float data;
            EndianUtils::unpack(msg_body.begin() + offset, data);
            return std::make_shared<RobotError>(timestamp, code, sub_code, static_cast<RobotError::Source>(source),
                                                static_cast<RobotError::Level>(level), static_cast<RobotError::DataType>(data_type),
                                                data);
        } break;
    }
    return nullptr;
}

RobotRuntimeExceptionSharedPtr PrimaryPort::paraserRuntimeException(uint64_t timestamp, const std::vector<uint8_t>& msg_body,
                                                                    int offset) {
    int32_t line;
    EndianUtils::unpack(msg_body.begin() + offset, line);
    offset += sizeof(int32_t);

    int32_t column;
    EndianUtils::unpack(msg_body.begin() + offset, column);
    offset += sizeof(int32_t);

    std::string text_msg;
    for (size_t i = offset; i < msg_body.size(); i++) {
        text_msg.push_back(msg_body[i]);
    }

    return std::make_shared<RobotRuntimeException>(timestamp, line, column, std::move(text_msg));
}

RobotExceptionSharedPtr PrimaryPort::parserException(const std::vector<uint8_t>& msg_body) {
    uint64_t timestamp;
    int offset = 0;
    EndianUtils::unpack<uint64_t>(msg_body.begin(), timestamp);
    offset += sizeof(uint64_t);

    // Only robot error message
    RobotError::Source source = static_cast<RobotError::Source>(msg_body[offset]);
    offset++;

    RobotException::Type type = static_cast<RobotException::Type>(msg_body[offset]);
    offset++;

    if (type == RobotException::Type::ROBOT_ERROR) {
        return parserRobotError(timestamp, source, msg_body, offset);
    } else if (type == RobotException::Type::SCRIPT_RUNTIME) {
        return paraserRuntimeException(timestamp, msg_body, offset);
    } else {
        return nullptr;
    }
}

bool PrimaryPort::parserMessageBody(int type, int package_len) {
    if (!tcp_client_ || !tcp_client_->isOpen()) {
        return false;
    }

    int body_len = package_len - HEAD_LENGTH;
    message_body_.resize(body_len);

    // Receive package body
    const SocketIOStatus read_status = tcp_client_->receiveAll(message_body_.data(), message_body_.size(), 500);
    if (read_status == SocketIOStatus::TIMEOUT) {
        ELITE_LOG_ERROR("Primary port receive package body timeout");
        return false;
    }
    if (read_status != SocketIOStatus::OK) {
        ELITE_LOG_ERROR("Primary port receive package body had expection: %s", tcp_client_->lastError().c_str());
        return false;
    }

    // If RobotState message parser others don't do anything.
    if (type == ROBOT_STATE_MSG_TYPE) {
        uint32_t sub_len = 0;
        for (auto iter = message_body_.begin(); iter < message_body_.end(); iter += sub_len) {
            EndianUtils::unpack(iter, sub_len);
            int sub_type = *(iter + 4);

            std::lock_guard<std::mutex> lock(mutex_);
            auto psm = parser_sub_msg_.find(sub_type);
            if (psm != parser_sub_msg_.end()) {
                psm->second->parser(sub_len, iter);
                psm->second->notifyUpated();
                parser_sub_msg_.erase(sub_type);
            }
        }
    } else if (type == ROBOT_EXCEPTION_MSG_TYPE) {
        if (robot_exception_cb_) {
            RobotExceptionSharedPtr ex = parserException(message_body_);
            if (ex) {
                robot_exception_cb_(ex);
            }
        }
    }
    return true;
}

bool PrimaryPort::socketReconnect(const std::string& ip, int port, bool is_last_connect_success) {
    // Disconnect and reconnect
    std::lock_guard<std::mutex> lock(socket_mutex_);
    socketDisconnect();
    return socketConnect(ip, port, is_last_connect_success);
}

void PrimaryPort::socketAsyncLoop(const std::string& ip, int port) {
    bool is_last_connect_success = true;
    while (socket_async_thread_alive_) {
        try {
            if (!parserMessage()) {
                auto now = std::chrono::system_clock::now();
                auto duration = now.time_since_epoch();
                auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
                auto ex = std::make_shared<RobotException>(RobotException::Type::ROBOT_DISCONNECTED, timestamp);
                if (robot_exception_cb_ && is_last_connect_success) {
                    robot_exception_cb_(ex);
                }
                is_last_connect_success = socketReconnect(ip, port, is_last_connect_success);
            }
            std::this_thread::sleep_for(10ms);
        } catch (const std::exception& e) {
            ELITE_LOG_ERROR("Primary port async loop throw exception:%s", e.what());
        }
    }
}

bool PrimaryPort::socketConnect(const std::string& ip, int port, bool is_last_connect_success) {
    tcp_client_ = std::make_unique<TcpClient>();
    if (!tcp_client_->connect(ip, port, 500)) {
        if (is_last_connect_success) {
            ELITE_LOG_ERROR("Connect to robot primary port fail: %s", tcp_client_->lastError().c_str());
        }
        tcp_client_.reset();
        message_head_received_ = 0;
        return false;
    }
    message_head_received_ = 0;
    return true;
}

void PrimaryPort::socketDisconnect() {
    message_head_received_ = 0;
    if (tcp_client_) {
        tcp_client_->close();
    }
}

std::string PrimaryPort::getLocalIP() {
    std::lock_guard<std::mutex> lock(socket_mutex_);
    if (tcp_client_ && tcp_client_->isOpen()) {
        return tcp_client_->localIP();
    }
    return "";
}

}  // namespace ELITE