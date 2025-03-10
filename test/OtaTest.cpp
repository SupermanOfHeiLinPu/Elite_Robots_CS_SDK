#include <gtest/gtest.h>
#include <string>
#include <cstdint>
#include <chrono>

#include "Elite/RemoteUpdate.hpp"
#include "Elite/Logger.hpp"

using namespace ELITE;

static std::string s_robot_ip;
static std::string s_upgrade_file;

TEST(otaTest, ota_control_software) {
    EXPECT_TRUE(ELITE::OTA::otaControlSoftware(s_robot_ip, s_upgrade_file, "elibot\n"));
}


int main(int argc, char** argv) {
    if (argc < 3 || argv[1] == nullptr || argv[2] == nullptr) {
        return 1;
    }
    ELITE::setLogLevel(ELITE::LogLevel::ELI_DEBUG);
    s_robot_ip = argv[1];
    s_upgrade_file = argv[2];
    
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
