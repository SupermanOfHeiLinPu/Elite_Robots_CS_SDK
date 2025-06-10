#include <Elite/PrimaryPortInterface.hpp>
#include <Elite/RobotConfPackage.hpp>
#include <Elite/RobotException.hpp>
#include <string>
#include <memory>
#include <iostream>
#include <thread>
#include <chrono>

using namespace std::chrono;

// In a real-world example it would be better to get those values from command line parameters / a
// better configuration system such as Boost.Program_options
const std::string DEFAULT_ROBOT_IP = "192.168.51.244";

void robotExceptionCb(ELITE::RobotExceptionSharedPtr ex) {
    if (ex->getType() == ELITE::RobotException::Type::RUNTIME) {
        auto r_ex = std::static_pointer_cast<ELITE::RobotRuntimeException>(ex);
        std::cout << r_ex->getMessage() << std::endl;
    }
}

int main(int argc, const char **argv) {
    // Parse the ip arguments if given
    std::string robot_ip = DEFAULT_ROBOT_IP;
    if (argc > 1) {
        robot_ip = std::string(argv[1]);
    }

    auto primary = std::make_unique<ELITE::PrimaryPortInterface>();
    
    auto kin = std::make_shared<ELITE::KinematicsInfo>();
    
    primary->connect(robot_ip, 30001);

    primary->registerRobotExceptionCallback(robotExceptionCb);

    primary->getPackage(kin, 200);

    std::cout << "DH parameter a: ";
    for (auto i : kin->dh_a_) {
        std::cout << i << "\t";
    }
    std::cout << std::endl;

    std::cout << "DH parameter d: ";
    for (auto i : kin->dh_d_) {
        std::cout << i << "\t";
    }
    std::cout << std::endl;

    std::cout << "DH parameter alpha: ";
    for (auto i : kin->dh_alpha_) {
        std::cout << i << "\t";
    }
    std::cout << std::endl;

    std::string script = "def hello():\n\ttextmsg(\"hello world\")\nend\n";

    primary->sendScript(script);

    script = "def exFunc():\n\t1abcd\nend\n";
    primary->sendScript(script);    

    primary->disconnect();

    return 0;
}


