// SPDX-License-Identifier: MIT
// Copyright (c) 2025, Elite Robots.
#include "ScriptSender.hpp"
#include "ControlCommon.hpp"
#include "EliteException.hpp"
#include "Log.hpp"

using namespace ELITE;

ScriptSender::ScriptSender(int port, const std::string& program)
    : program_(program), TcpServer(port, 1024) {
    setReceiveCallback([this](const uint8_t data[], int size) { onReceive(data, size); });
}

ScriptSender::~ScriptSender() {}

void ScriptSender::onReceive(const uint8_t data[], int size) {
    if (size <= 0) {
        return;
    }

    recv_request_buffer_.append(reinterpret_cast<const char*>(data), static_cast<std::size_t>(size));
    std::size_t newline_pos = std::string::npos;
    while ((newline_pos = recv_request_buffer_.find('\n')) != std::string::npos) {
        std::string request = recv_request_buffer_.substr(0, newline_pos);
        recv_request_buffer_.erase(0, newline_pos + 1);

        if (!request.empty() && request.back() == '\r') {
            request.pop_back();
        }

        if (request == PROGRAM_REQUEST_) {
            ELITE_LOG_INFO("Robot request external control script.");
            if (writeClient(const_cast<char*>(program_.data()), static_cast<int>(program_.size())) < 0) {
                ELITE_LOG_ERROR("Script sender send script fail");
                return;
            }
        }
    }
}
