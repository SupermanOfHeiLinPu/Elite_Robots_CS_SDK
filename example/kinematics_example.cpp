// SPDX-License-Identifier: MIT
// Copyright (c) 2025, Elite Robots.
#include <Elite/Log.hpp>
#include <Elite/PrimaryPortInterface.hpp>
#include <Elite/RobotConfPackage.hpp>
#include <Elite/RobotException.hpp>
#include <Elite/ClassLoader.hpp>
#include <Elite/RtsiIOInterface.hpp>
#include <Elite/KinematicsBase.hpp>

#include <boost/program_options.hpp>
#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

using namespace std::chrono;
using namespace ELITE;
namespace po = boost::program_options;


int main(int argc, const char** argv) {
    // Parse the ip arguments if given
    std::string robot_ip;

    // Parser param
    po::options_description desc(
        "Usage:\n"
        "\t./primary_example <--robot-ip=ip>\n"
        "Parameters:");
    desc.add_options()
        ("help,h", "Print help message")
        ("robot-ip", po::value<std::string>(&robot_ip)->required(),
            "\tRequired. IP address of the robot.");

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

    auto primary = std::make_unique<PrimaryPortInterface>();

    auto kin_info = std::make_shared<KinematicsInfo>();

    // Connect robot 30001 port and get MDH param
    if (!primary->connect(robot_ip, 30001)) {
        ELITE_LOG_FATAL("Connect robot 30001 port fail.");
        return 1;
    }
    if (!primary->getPackage(kin_info, 200)) {
        ELITE_LOG_FATAL("Get robot kinematics info fail.");
        return 1;
    }
    primary->disconnect();
    ELITE_LOG_INFO("Getted robot kinematics info.");

    std::unique_ptr<RtsiIOInterface> io_interface = std::make_unique<RtsiIOInterface>("output_recipe.txt", "input_recipe.txt", 250);

    auto current_joint = io_interface->getActualJointPositions();
    ELITE_LOG_INFO("Getted robot actual joint positions.");

    auto current_tcp = io_interface->getActualTCPPose();
    ELITE_LOG_INFO("Getted robot actual TCP positions.");

    ClassLoader loader("./libelite_kdl_kin_plugin.so");

    if(!loader.loadLib()) {
        ELITE_LOG_FATAL("Load plugin fail.");
        return 1;
    }

    auto kin_slover = loader.createUniqueInstance<KinematicsBase>("ELITE::KdlKinematicsPlugin");
    if (kin_slover == nullptr) {
        ELITE_LOG_FATAL("Create KinematicsBase fail");
        return 1;
    }
    
    kin_slover->setMDH(kin_info->dh_alpha_, kin_info->dh_a_, kin_info->dh_d_);

    vector6d_t fk_pose;
    kin_slover->getPositionFK(current_joint, fk_pose);

    vector6d_t ik_joints;
    KinematicsResult ik_result;
    kin_slover->getPositionIK(current_tcp, current_joint, ik_joints, ik_result);

    for (auto i : fk_pose) {
        std::cout << i << ", ";
    }
    std::cout << std::endl;

    for (auto i : ik_joints) {
        std::cout << i << ", ";
    }
    std::cout << std::endl;
    return 0;
}
