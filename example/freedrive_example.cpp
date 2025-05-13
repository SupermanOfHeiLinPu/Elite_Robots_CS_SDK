#include "Elite/EliteDriver.hpp"

#include <boost/asio.hpp>
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>

using namespace ELITE;
using namespace std::chrono;

// This is a flag that indicates whether to run or not
// When receive Ctrl+C or kill signal, the flag will be set false.
static std::atomic<bool> s_is_freedrive_running;

void freeDriveLoop(const std::string& robot_ip, const std::string& local_ip) {
    auto driver = std::make_unique<EliteDriver>(robot_ip, local_ip, "external_control.script", true);

    std::cout << "Wait robot connect" << std::endl;
    while (!driver->isRobotConnected()) {
        std::this_thread::sleep_for(4ms);
    }
    std::cout << "Robot connected" << std::endl;

    std::cout << "Start freedrive mode" << std::endl;
    driver->writeFreedrive(FreedriveAction::FREEDRIVE_START, 100);
    while (s_is_freedrive_running) {
        driver->writeFreedrive(FreedriveAction::FREEDRIVE_NOOP, 100);
        std::this_thread::sleep_for(10ms);
    }
    std::cout << "End freedrive mode" << std::endl;
    driver->writeFreedrive(FreedriveAction::FREEDRIVE_END, 100);

    driver->stopControl();
}


int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cout << "Must provide robot ip or local ip. Command like: ./freedrive_example 192.168.1.250 192.168.1.251" << std::endl;
        return 1;
    }

    s_is_freedrive_running = true;
    // Freediver is running in another thread
    std::thread freedrive_thread([&](std::string robot_ip, std::string local_ip) {
        freeDriveLoop(robot_ip, local_ip);
    }, argv[1], argv[2]);
    
    // Capture SIGINT (Ctrl+C) and SIGTERM (kill)
    boost::asio::io_context io_context;
    boost::asio::signal_set signal_set(io_context, SIGINT, SIGTERM);
    signal_set.async_wait([&](const boost::system::error_code& ec, int signal_number) {
        if (!ec) {
            std::cout << "Received exit signal, exiting...\n";
            s_is_freedrive_running = false;
            io_context.stop();
        }
    });

    // Boost asio task run
    while (s_is_freedrive_running) {
        if (io_context.stopped()) {
            io_context.restart();
        }
        io_context.run();
    }
    // Wait freedrive thread finish
    freedrive_thread.join();

    return 0;
}
