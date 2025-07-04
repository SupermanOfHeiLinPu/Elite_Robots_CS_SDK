#include <Elite/DashboardClient.hpp>
#include <Elite/DataType.hpp>
#include <Elite/EliteDriver.hpp>
#include <Elite/RtsiClientInterface.hpp>

#include <cmath>
#include <iostream>
#include <memory>
#include <thread>
#include <boost/program_options.hpp>

using namespace ELITE;
using namespace std::chrono;
namespace po = boost::program_options;

static std::unique_ptr<EliteDriver> s_driver;
static std::unique_ptr<RtsiClientInterface> s_rtsi_client;
static std::unique_ptr<DashboardClient> s_dashboard;

// 定义一个合适的 epsilon 值，这个值可以根据具体情况调整
const double EPSILON = 1e-4;

bool areAlmostEqual(double a, double b) { return std::fabs(a - b) < EPSILON; }

bool areAlmostEqual(const vector6d_t& a, const vector6d_t& b) {
    for (size_t i = 0; i < a.size(); i++) {
        if (!areAlmostEqual(a[i], b[i])) {
            return false;
        }
    }
    return true;
}

template <typename T>
void printArray(T& l) {
    for (auto& i : l) {
        std::cout << i << " ";
    }
    std::cout << std::endl;
}

void waitRobotArrive(std::vector<std::shared_ptr<RtsiRecipe>> recipe_list, const ELITE::vector6d_t& target) {
    vector6d_t acutal_joint;
    while (true) {
        if (s_rtsi_client->receiveData(recipe_list) > 0) {
            if (!recipe_list[0]->getValue("actual_joint_positions", acutal_joint)) {
                std::cout << "Recipe has wrong" << std::endl;
                continue;
            }
            if (areAlmostEqual(acutal_joint, target)) {
                break;
            } else {
                if (!s_driver->writeServoj(target, 100)) {
                    return;
                }
            }
        } else {
            std::cout << "Couldn't receive data" << std::endl;
        }
    }
}

int main(int argc, char** argv) {
    EliteDriverConfig config;

    // Parser param
    po::options_description desc("Usage:\n"
        "\t./servo_example <--robot-ip=ip> [--local-ip=\"\"] [--use-headless-mode=true]\n"
        "Parameters:");
    desc.add_options()
        ("help,h", "Print help message")
        ("robot-ip", po::value<std::string>(&config.robot_ip)->required(), "\tRequired. IP address of the robot.")
        ("use-headless-mode", po::value<bool>(&config.headless_mode)->required()->implicit_value(true), "\tRequired. Use headless mode.")
        ("local-ip", po::value<std::string>(&config.local_ip)->default_value(""), "\tOptional. IP address of the local network interface.");

    po::variables_map vm;
    try {
        po::store(po::parse_command_line(argc, argv, desc), vm);

        if (vm.count("help")) {
            std::cout << desc << std::endl;
            return 0;
        }

        po::notify(vm);
    } catch(const po::error& e) {
        std::cerr << "Argument error: " << e.what() << "\n\n";
        std::cerr << desc << "\n";
        return 1;
    }
    

    config.script_file_path = "external_control.script";
    config.servoj_time = 0.004;
    s_driver = std::make_unique<EliteDriver>(config);
    s_rtsi_client = std::make_unique<RtsiClientInterface>();
    s_dashboard = std::make_unique<DashboardClient>();

    if (!s_dashboard->connect(config.robot_ip)) {
        return 1;
    }
    std::cout << "Dashboard connected" << std::endl;

    s_rtsi_client->connect(config.robot_ip);
    std::cout << "RTSI connected" << std::endl;

    if (!s_rtsi_client->negotiateProtocolVersion()) {
        return 1;
    }

    std::cout << "Controller version is " << s_rtsi_client->getControllerVersion().toString() << std::endl;

    auto recipe = s_rtsi_client->setupOutputRecipe({"actual_joint_positions"}, 250);
    if (!recipe) {
        std::cout << "RTSI setup output recipe fail" << std::endl;
        return 1;
    }

    if (!s_dashboard->powerOn()) {
        std::cout << "Dashboard power on fail" << std::endl;
        return 1;
    }
    std::cout << "Robot power on" << std::endl;

    if (!s_dashboard->brakeRelease()) {
        std::cout << "Dashboard brake release fail" << std::endl;
        return 1;
    }
    std::cout << "Robot brake released" << std::endl;

    if (config.headless_mode) {
        if (!s_driver->isRobotConnected()) {
            if (!s_driver->sendExternalControlScript()){
                std::cout << "Send external control script fail." << std::endl;
                return 1;
            }
        }
    } else {
        if (!config.headless_mode && !s_dashboard->playProgram()) {
            std::cout << "Dashboard play program fail" << std::endl;
            return 1;
        }
    }

    while (!s_driver->isRobotConnected()) {
        std::this_thread::sleep_for(10ms);
    }
    std::cout << "External control script run" << std::endl;

    if (!s_rtsi_client->start()) {
        return 1;
    }
    std::vector<std::shared_ptr<RtsiRecipe>> recipe_list{recipe};
    vector6d_t acutal_joint;

    if (s_rtsi_client->receiveData(recipe_list) <= 0) {
        std::cout << "RTSI recipe receive none" << std::endl;
        return 1;
    }

    if (!recipe_list[0]->getValue("actual_joint_positions", acutal_joint)) {
        std::cout << "Recipe has wrong" << std::endl;
        return 1;
    }

    if (!s_rtsi_client->pause()) {
        return 1;
    }

    // Make target points
    std::vector<vector6d_t> target_positions_1;
    vector6d_t target_joint = acutal_joint;

    if (acutal_joint[5] <= 3) {
        for (double target = acutal_joint[5]; target < 3; target += 0.001) {
            target_joint[5] = target;
            target_positions_1.push_back(target_joint);
        }
    } else {
        for (double target = acutal_joint[5]; target > 3; target -= 0.001) {
            target_joint[5] = target;
            target_positions_1.push_back(target_joint);
        }
    }

    std::vector<vector6d_t> target_positions_2;
    for (double target = (*(target_positions_1.end() - 1))[5]; target > -3; target -= 0.002) {
        target_joint[5] = target;
        target_positions_2.push_back(target_joint);
    }

    for (auto& target : target_positions_1) {
        if (!s_driver->writeServoj(target, 100)) {
            return 1;
        }
        std::this_thread::sleep_for(4000us);
    }

    if (!s_rtsi_client->start()) {
        return 1;
    }

    waitRobotArrive(recipe_list, *(target_positions_1.end() - 1));

    if (!s_rtsi_client->pause()) {
        return 1;
    }

    for (auto& target : target_positions_2) {
        if (!s_driver->writeServoj(target, 100)) {
            return 1;
        }
        std::this_thread::sleep_for(4000us);
    }

    if (!s_rtsi_client->start()) {
        return 1;
    }

    waitRobotArrive(recipe_list, *(target_positions_2.end() - 1));

    s_driver->stopControl();

    return 0;
}
