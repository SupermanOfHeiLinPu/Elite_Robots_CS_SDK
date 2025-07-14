#include <Elite/DashboardClient.hpp>
#include <Elite/DataType.hpp>
#include <Elite/EliteDriver.hpp>
#include <Elite/Log.hpp>
#include <Elite/RtsiClientInterface.hpp>

#include <boost/program_options.hpp>
#include <chrono>
#include <cmath>
#include <iostream>
#include <memory>
#include <thread>

using namespace ELITE;
using namespace std::chrono;
namespace po = boost::program_options;

static std::unique_ptr<EliteDriver> s_driver;
static std::unique_ptr<RtsiClientInterface> s_rtsi_client;
static std::unique_ptr<DashboardClient> s_dashboard;

int main(int argc, char** argv) {
    EliteDriverConfig config;

    // Parser param
    po::options_description desc(
        "Usage:\n"
        "\t./servo_example <--robot-ip=ip> [--local-ip=\"\"] [--use-headless-mode=true]\n"
        "Parameters:");
    desc.add_options()
        ("help,h", "Print help message")
        ("robot-ip", po::value<std::string>(&config.robot_ip)->required(),
            "\tRequired. IP address of the robot.")
        ("use-headless-mode", po::value<bool>(&config.headless_mode)->required()->implicit_value(true),
            "\tRequired. Use headless mode.")
        ("local-ip", po::value<std::string>(&config.local_ip)->default_value(""),
            "\tOptional. IP address of the local network interface.");

    po::variables_map vm;
    try {
        po::store(po::parse_command_line(argc, argv, desc), vm);

        if (vm.count("help")) {
            std::cout << desc << std::endl;
            return 0;
        }

        po::notify(vm);
    } catch (const po::error& e) {
        std::cerr << "Argument error: " << e.what() << "\n\n";
        std::cerr << desc << "\n";
        return 1;
    }

    if (config.headless_mode) {
        ELITE_LOG_WARN("Use headless mode. Please ensure the robot is not in local mode.");
    } else {
        ELITE_LOG_WARN(
            "It needs to be correctly configured, and the External Control plugin should be inserted into the task tree.");
    }

    config.script_file_path = "external_control.script";
    config.servoj_time = 0.004;
    s_driver = std::make_unique<EliteDriver>(config);
    s_rtsi_client = std::make_unique<RtsiClientInterface>();
    s_dashboard = std::make_unique<DashboardClient>();
    ELITE_LOG_INFO("Connecting to the dashboard");
    if (!s_dashboard->connect(config.robot_ip)) {
        ELITE_LOG_FATAL("Failed to connect to the dashboard.");
        return 1;
    }
    ELITE_LOG_INFO("Successfully connected to the dashboard");

    ELITE_LOG_INFO("Connecting to the RTSI");
    s_rtsi_client->connect(config.robot_ip);
    ELITE_LOG_INFO("Successfully connected to the RTSI");

    if (!s_rtsi_client->negotiateProtocolVersion()) {
        ELITE_LOG_FATAL("Failed to verify the RTSI version.");
        return 1;
    }
    ELITE_LOG_INFO("Successfully verify the RTSI version");

    VersionInfo version = s_rtsi_client->getControllerVersion();
    ELITE_LOG_INFO("Controller version is %s", version.toString().c_str());

    auto recipe = s_rtsi_client->setupOutputRecipe({"actual_joint_positions"});
    if (!recipe) {
        ELITE_LOG_FATAL("RTSI subscription failed.");
        return 1;
    }

    ELITE_LOG_INFO("Start powering on...");
    if (!s_dashboard->powerOn()) {
        ELITE_LOG_FATAL("Power-on failed");
        return 1;
    }
    ELITE_LOG_INFO("Power-on succeeded");

    ELITE_LOG_INFO("Start releasing brake...");
    if (!s_dashboard->brakeRelease()) {
        ELITE_LOG_FATAL("Release brake fail");
        return 1;
    }
    ELITE_LOG_INFO("Brake released");

    if (config.headless_mode) {
        if (!s_driver->isRobotConnected()) {
            if (!s_driver->sendExternalControlScript()) {
                ELITE_LOG_FATAL("Fail to send external control script");
                return 1;
            }
        }
    } else {
        if (!config.headless_mode && !s_dashboard->playProgram()) {
            ELITE_LOG_FATAL("Fail to play program");
            return 1;
        }
    }

    ELITE_LOG_INFO("Wait external control script run...");
    while (!s_driver->isRobotConnected()) {
        std::this_thread::sleep_for(10ms);
    }
    ELITE_LOG_INFO("External control script is running");

    if (!s_rtsi_client->start()) {
        ELITE_LOG_FATAL("Fail to start RTSI");
        return 1;
    }

    bool positive_rotation = false;
    bool negative_rotation = false;
    vector6d_t acutal_joint;
    vector6d_t target_joint;
    double increment = 0;
    bool first_point = true;
    while (!(positive_rotation && negative_rotation)) {
        if (!s_rtsi_client->receiveData(recipe)) {
            ELITE_LOG_FATAL("Fail to receive RTSI recipe");
            return 1;
        }
        if (!recipe->getValue("actual_joint_positions", acutal_joint)) {
            ELITE_LOG_FATAL("RTSI recipe don't has actual_joint_positions variable");
            return 1;
        }
        // If first point init target_joint
        if (first_point) {
            target_joint = acutal_joint;
            first_point = false;
        }

        // Set the increment of positive rotation and negative rotation
        if (positive_rotation == false) {
            increment = 0.0005;
            if (acutal_joint[5] >= 3) {
                positive_rotation = true;
            }
        } else if (negative_rotation == false) {
            increment = -0.0005;
            if (acutal_joint[5] <= -3) {
                negative_rotation = true;
            }
        }
        target_joint[5] += increment;

        if (!s_driver->writeServoj(target_joint, 100)) {
            ELITE_LOG_FATAL("Send servoj command to robot fail");
            return 1;
        }
    }
    ELITE_LOG_INFO("Motion finish");
    s_driver->stopControl();

    return 0;
}
