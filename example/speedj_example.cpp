#include <Elite/EliteDriver.hpp>
#include <Elite/RtsiIOInterface.hpp>
#include <Elite/DataType.hpp>
#include <Elite/DashboardClient.hpp>

#include <iostream>
#include <memory>

using namespace ELITE;

static std::unique_ptr<EliteDriver> s_driver;
static std::unique_ptr<DashboardClient> s_dashboard;

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cout << "Must provide robot ip or local ip. Command like: ./speedj_example 192.168.1.250 192.168.1.251" << std::endl;
        return 1;
    }
    s_driver = std::make_unique<EliteDriver>(argv[1], argv[2], "external_control.script");
    s_dashboard = std::make_unique<DashboardClient>();
    
    if (!s_dashboard->connect(argv[1])) {
        return 1;
    }
    std::cout << "Dashboard connected" << std::endl;

    if(!s_dashboard->powerOn()) {
        return 1;
    }
    std::cout << "Robot power on" << std::endl;

    if (!s_dashboard->brakeRelease()) {
        return 1;
    }
    std::cout << "Robot brake released" << std::endl;

    if (!s_dashboard->playProgram()) {
        return 1;
    }
    std::cout << "Program run" << std::endl;

    while (!s_driver->isRobotConnected()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Reverse rotation for 10 seconds
    vector6d_t speedj_vector{0, 0, 0, 0, 0, -0.1};
    s_driver->writeSpeedj(speedj_vector, 0);
    std::this_thread::sleep_for(std::chrono::seconds(5));

    // Rotate forward for 10 seconds
    speedj_vector = {0, 0, 0, 0, 0, 0.1};
    s_driver->writeSpeedj(speedj_vector, 0);
    std::this_thread::sleep_for(std::chrono::seconds(5));

    s_driver->stopControl();

    return 0;
}
