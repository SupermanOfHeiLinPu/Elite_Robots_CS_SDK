#include <gtest/gtest.h>
#include <string>
#include <thread>
#include "Common/TcpServer.hpp"
#include "boost/asio.hpp"
#include <iostream>

using namespace std::chrono;
using namespace ELITE;

#define SERVER_TEST_PORT (50001)

class TcpClient
{
public:
    boost::asio::io_context io_context;
    std::unique_ptr<boost::asio::ip::tcp::socket> socket_ptr;
    std::unique_ptr<boost::asio::ip::tcp::resolver> resolver_ptr;
    TcpClient() = default;
    
    TcpClient(const std::string& ip, int port) {
        connect(ip, port);
    }

    ~TcpClient() = default;

    void connect(const std::string& ip, int port) {
        try {
            socket_ptr.reset(new boost::asio::ip::tcp::socket(io_context));
            resolver_ptr.reset(new boost::asio::ip::tcp::resolver(io_context));
            socket_ptr->open(boost::asio::ip::tcp::v4());
            boost::asio::ip::tcp::no_delay no_delay_option(true);
            socket_ptr->set_option(no_delay_option);
            boost::asio::socket_base::reuse_address sol_reuse_option(true);
            socket_ptr->set_option(sol_reuse_option);
#if defined(__linux) || defined(linux) || defined(__linux__)
            boost::asio::detail::socket_option::boolean<IPPROTO_TCP, TCP_QUICKACK> quickack(true);
            socket_ptr->set_option(quickack);
#endif
            boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::make_address(ip), port);
            socket_ptr->async_connect(endpoint, [&](const boost::system::error_code& error) {
                if (error) {
                    throw boost::system::system_error(error);
                }
            });
            io_context.run();
        
        } catch(const boost::system::system_error &error) {
            throw error;
        }
    }
    
};



TEST(TCP_SERVER, TCP_SERVER_TEST) {
    TcpServer::start();
    std::shared_ptr<TcpServer> server = std::make_shared<TcpServer>(SERVER_TEST_PORT, 4);
    server->startListen();
    // Wait listen
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    TcpClient client("127.0.0.1", SERVER_TEST_PORT);
    // Wait connection
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_TRUE(server->isClientConnected());

    int send_data = 12345;

    server->setReceiveCallback([&](const uint8_t data[], int nb) {
        EXPECT_EQ(nb, 4);
        EXPECT_EQ(*(int*)data, send_data);
    });

    client.socket_ptr->send(boost::asio::buffer(&send_data, sizeof(send_data)));
    // Wait send
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    TcpServer::stop();
}

TEST(TCP_SERVER, TCP_SERVER_MULIT_CONNECT) {
    TcpServer::start();
    std::shared_ptr<TcpServer> server = std::make_shared<TcpServer>(SERVER_TEST_PORT, 4);
    server->startListen();
    // Wait listen
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    TcpClient client1;
    TcpClient client2;
    const std::string client_send_string = "client_send_string\n";    

    client1.connect("127.0.0.1", SERVER_TEST_PORT);
    // Wait connected
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    ASSERT_FALSE(server->isClientConnected());
    
    server->setReceiveCallback([&](const uint8_t data[], int nb) {
        EXPECT_EQ(nb, client_send_string.length());

        std::string str((const char *)data);
        EXPECT_EQ(str, client_send_string);
    });

    ASSERT_EQ(client1.socket_ptr->write_some(boost::asio::buffer(client_send_string)), client_send_string.length());
    // Wait for recv
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // New connection
    client2.connect("127.0.0.1", SERVER_TEST_PORT);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    ASSERT_FALSE(server->isClientConnected());

    ASSERT_EQ(client2.socket_ptr->write_some(boost::asio::buffer(client_send_string)), client_send_string.length());
    // Wait for recv
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    client1.socket_ptr->close();
    client2.socket_ptr->close();
    TcpServer::stop();
}


int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}