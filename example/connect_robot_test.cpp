#include <Elite/PrimaryPortInterface.hpp>
#include <boost/asio.hpp>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <string>

using boost::asio::ip::tcp;
using namespace std::chrono;
using namespace ELITE;

#define SERVER_PORT 50002

// In a real-world example it would be better to get those values from command line parameters / a
// better configuration system such as Boost.Program_options
const std::string DEFAULT_ROBOT_IP = "192.168.51.244";

// The robot will send string
#define ROBOT_SOCKET_SEND_STRING "hello"

// The TCP server class
class TcpServer {
   public:
    TcpServer(boost::asio::io_context& io_context, short port)
        : io_context_(io_context), acceptor_(io_context, tcp::endpoint(tcp::v4(), port)) {
        startAccept();
    }

    std::future<std::string> getReceiveString() {
        std::lock_guard<std::mutex> lock(promise_mutex_);
        std::promise<std::string> promise;
        auto future = promise.get_future();
        promise_queue_.push(std::move(promise));
        return future;
    }

   private:
    void startAccept() {
        auto new_socket = std::make_shared<tcp::socket>(io_context_);
        acceptor_.async_accept(*new_socket, [this, new_socket](const boost::system::error_code& error) {
            if (!error) {
                std::cout << "New connection from: " << new_socket->remote_endpoint().address().to_string() << std::endl;
                startRead(new_socket);
            }
            startAccept();  // Continue accepting next client
        });
    }

    void startRead(std::shared_ptr<tcp::socket> socket) {
        boost::asio::async_read_until(*socket, receive_buffer_, '\n',
                                      [this, socket](const boost::system::error_code& ec, size_t bytes_transferred) {
                                          if (!ec) {
                                              std::istream is(&receive_buffer_);
                                              std::string message;
                                              std::getline(is, message);

                                              std::lock_guard<std::mutex> lock(promise_mutex_);
                                              if (!promise_queue_.empty()) {
                                                  auto promise = std::move(promise_queue_.front());
                                                  promise_queue_.pop();
                                                  promise.set_value(message);
                                              } else {
                                                  std::cerr << "Warning: Received message but no waiting future.\n";
                                              }

                                              // Continue reading next message
                                              startRead(socket);
                                          } else {
                                              std::cerr << "Read error: " << ec.message() << std::endl;
                                              socket->close();
                                          }
                                      });
    }

    boost::asio::io_context& io_context_;
    tcp::acceptor acceptor_;
    boost::asio::streambuf receive_buffer_;

    std::mutex promise_mutex_;
    std::queue<std::promise<std::string>> promise_queue_;
};

int main(int argc, char** argv) {
    std::string robot_ip = DEFAULT_ROBOT_IP;
    std::string local_ip = "";
    if (argc >= 2) {
        robot_ip = argv[1];
    }
    if (argc >= 3) {
        local_ip = argv[2];
    }

    try {
        // The backend captures exception packets by parsing the packet data from the robot's primary port. Start a TCP server and
        // listen on a background thread.
        boost::asio::io_context io_context;
        TcpServer server(io_context, SERVER_PORT);
        std::thread thread([&io_context]() {
            try {
                io_context.run();
            } catch (const std::exception& e) {
                std::cerr << "IO context error: " << e.what() << std::endl;
            }
        });
        // Connect to robot primary port
        PrimaryPortInterface primary;
        primary.connect(robot_ip);
        if (local_ip.empty()) {
            local_ip = primary.getLocalIP();
        }
        // Construct script
        std::string robot_script = "def socket_test():\n";
        robot_script += "\tsocket_open(\"" + local_ip + "\", " + std::to_string(SERVER_PORT) + ")\n";
        robot_script += "\tsocket_send_string(\"" ROBOT_SOCKET_SEND_STRING "\\n\")\n";
        robot_script += "end\n";
        // Print script
        std::cout << "Will send the script to robot primary port:" << std::endl;
        std::cout << robot_script;
        // Send script to robot primary port
        primary.sendScript(robot_script);

        // Wait receive string and determine if it is correct
        auto future_recv = server.getReceiveString();
        if (future_recv.wait_for(5s) == std::future_status::ready) {
            auto str = future_recv.get();
            if (str == ROBOT_SOCKET_SEND_STRING) {
                std::cout << "Success, robot connected to PC" << std::endl;
            } else {
                std::cout << "Fail, robot connected to PC but not send right string" << std::endl;
            }
        } else {
            std::cout << "Error: Time out, robot not connect to PC" << std::endl;
        }

        io_context.stop();
        thread.join();

    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return 0;
}