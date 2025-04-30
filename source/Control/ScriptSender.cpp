#include "ScriptSender.hpp"
#include "ControlCommon.hpp"
#include "EliteException.hpp"
#include "Log.hpp"
#include <boost/asio.hpp>

using namespace ELITE;



ScriptSender::ScriptSender(int port, const std::string& program) : program_(program), TcpServer(port, 0) {
    
}


ScriptSender::~ScriptSender() {
    
}

void ScriptSender::doAccept() {
    // Accept call back
    auto accept_cb = [&](boost::system::error_code ec, boost::asio::ip::tcp::socket sock) {
        {
            std::lock_guard<std::mutex> lock(socket_mutex_);
            if (socket_) {
                socket_->close();
            }
            if (ec) {
                socket_.reset();
            } else {
                socket_.reset(new boost::asio::ip::tcp::socket(std::move(sock)));
            }   
        }
        responseRequest();
        doAccept();
    };
    acceptor_.listen(1);
    acceptor_.async_accept(getContext(), accept_cb);
}

void ScriptSender::responseRequest() {
    std::lock_guard<std::mutex> lock(socket_mutex_);
    if (!socket_) {
        return;
    }
    boost::asio::async_read_until(
        *socket_,
        recv_request_buffer_,
        '\n',
        [&](boost::system::error_code ec, std::size_t len) {
            if (ec) {
                if (socket_->is_open()) {
                    ELITE_LOG_INFO("Connection to script sender interface dropped: %s", boost::system::system_error(ec).what());
                    releaseClient();
                }
                return;
            }
            ELITE_LOG_INFO("Robot request external control script.");
            std::string request;
            std::istream response_stream(&recv_request_buffer_);
            std::getline(response_stream, request);
            if (request == PROGRAM_REQUEST_) {
                boost::system::error_code wec;
                socket_->write_some(boost::asio::buffer(program_), wec);
                if (wec) {
                    ELITE_LOG_ERROR("Script sender send script fail: %s", boost::system::system_error(wec).what());
                    return;
                }
            }
            responseRequest();
        }
    );
}
