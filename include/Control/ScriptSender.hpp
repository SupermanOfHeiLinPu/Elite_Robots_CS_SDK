// SPDX-License-Identifier: MIT
// Copyright (c) 2025, Elite Robots.
//
// ScriptSender.hpp
// Provides the ScriptSender class for sending robot scripts.
#ifndef __SCRIPT_SENDER_HPP__
#define __SCRIPT_SENDER_HPP__

#include "TcpServer.hpp"

#include <memory>
#include <string>

namespace ELITE {

class ScriptSender : public TcpServer {
   private:
    const std::string PROGRAM_REQUEST_ = std::string("request_program");
    std::string recv_request_buffer_;
    const std::string& program_;

    void onReceive(const uint8_t data[], int size);

   public:
    ScriptSender(int port, const std::string& program);
    ~ScriptSender();
};

}  // namespace ELITE

#endif
