#include <Elite/DashboardClient.hpp>
#include <Elite/DataType.hpp>
#include <Elite/EliteDriver.hpp>
#include <Elite/RtsiIOInterface.hpp>
#include <Elite/Log.hpp>

#include <boost/program_options.hpp>
#include <iostream>
#include <memory>
#include <thread>

using namespace ELITE;
namespace po = boost::program_options;

static std::unique_ptr<EliteDriver> s_driver;
static std::unique_ptr<RtsiIOInterface> s_rtsi_client;
static std::unique_ptr<DashboardClient> s_dashboard;

int main(int argc, const char** argv) {
    EliteDriverConfig config;
    // Parser param
    po::options_description desc(
        "Usage:\n"
        "\t./trajectory_example <--robot-ip=ip> [--local-ip=\"\"] [--use-headless-mode=true]\n"
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
    s_driver = std::make_unique<EliteDriver>(config);

    s_rtsi_client = std::make_unique<RtsiIOInterface>("output_recipe.txt", "input_recipe.txt", 250);
    s_dashboard = std::make_unique<DashboardClient>();

    ELITE_LOG_INFO("Connecting to the dashboard");
    if (!s_dashboard->connect(config.robot_ip)) {
        ELITE_LOG_FATAL("Failed to connect to the dashboard.");
        return 1;
    }
    ELITE_LOG_INFO("Successfully connected to the dashboard");

    ELITE_LOG_INFO("Connecting to the RTSI");
    if (!s_rtsi_client->connect(config.robot_ip)) {
        ELITE_LOG_FATAL("Fail to connect or config to the RTSI");
        return 1;
    }
    ELITE_LOG_INFO("Successfully connected to the RTSI");

    bool is_move_finish = false;
    s_driver->setTrajectoryResultCallback([&](TrajectoryMotionResult result) {
        if (result == TrajectoryMotionResult::SUCCESS) {
            is_move_finish = true;
        }
    });

    ELITE_LOG_INFO("Start powering on...");
    if (!s_dashboard->powerOn()) {
        ELITE_LOG_FATAL("Power-on failed");
        return 1;
    }
    ELITE_LOG_INFO("Power-on succeeded");

    ELITE_LOG_INFO("Start releasing brake...");
    if (!s_dashboard->brakeRelease()) {
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
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    ELITE_LOG_INFO("External control script is running");

    vector6d_t actual_joints = s_rtsi_client->getActualJointPositions();

    s_driver->writeTrajectoryControlAction(ELITE::TrajectoryControlAction::START, 1, 200);

    actual_joints[3] = -1.57;
    s_driver->writeTrajectoryPoint(actual_joints, 3, 0, false);

    while (!is_move_finish) {
        s_driver->writeTrajectoryControlAction(ELITE::TrajectoryControlAction::NOOP, 0, 200);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
   ELITE_LOG_INFO("Joints moved to target");

    vector6d_t actual_pose = s_rtsi_client->getAcutalTCPPose();

    is_move_finish = false;

    s_driver->writeTrajectoryControlAction(ELITE::TrajectoryControlAction::START, 3, 200);

    actual_pose[2] -= 0.2;
    s_driver->writeTrajectoryPoint(actual_pose, 3, 0.05, true);

    actual_pose[1] -= 0.2;
    s_driver->writeTrajectoryPoint(actual_pose, 3, 0.05, true);

    actual_pose[1] += 0.2;
    actual_pose[2] += 0.2;
    s_driver->writeTrajectoryPoint(actual_pose, 3, 0.05, true);

    while (!is_move_finish) {
        s_driver->writeTrajectoryControlAction(ELITE::TrajectoryControlAction::NOOP, 0, 200);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    ELITE_LOG_INFO("Line moved to target");

    s_driver->writeTrajectoryControlAction(ELITE::TrajectoryControlAction::CANCEL, 1, 200);
    s_driver->stopControl();
    return 0;
}