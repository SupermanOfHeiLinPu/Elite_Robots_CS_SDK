#include <gtest/gtest.h>

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "Elite/EliteDriver.hpp"

namespace {

std::string readExternalControlScript() {
    const std::vector<std::string> candidates = {
        "external_control.script",
        "test/external_control.script",
        "../test/external_control.script",
    };

    for (const auto& path : candidates) {
        std::ifstream ifs(path);
        if (ifs.good()) {
            std::stringstream buffer;
            buffer << ifs.rdbuf();
            return buffer.str();
        }
    }
    return "";
}

}  // namespace

TEST(EliteDriverConfig, ExtrapolateDefaults) {
    ELITE::EliteDriverConfig config;

    EXPECT_FLOAT_EQ(config.servoj_extrapolate_max_time, 0.08f);
    EXPECT_FLOAT_EQ(config.servoj_decelerate_time, 0.01f);
    EXPECT_FLOAT_EQ(config.servoj_hold_velocity_threshold, 0.05f);
    EXPECT_FLOAT_EQ(config.servoj_hold_stable_time, 0.04f);
}

TEST(ExternalControlScript, ExtrapolatePlaceholdersAndGuardsPresent) {
    const std::string content = readExternalControlScript();
    ASSERT_FALSE(content.empty()) << "external_control.script was not found in expected test paths.";

    EXPECT_NE(content.find("SERVOJ_EXTRAPOLATE_MAX_TIME = max(0.0, {{SERVOJ_EXTRAPOLATE_MAX_TIME_REPLACE}})"),
              std::string::npos);
    EXPECT_NE(content.find("SERVOJ_DECELERATE_TIME = max(0.0, {{SERVOJ_DECELERATE_TIME_REPLACE}})"), std::string::npos);
    EXPECT_NE(content.find("SERVOJ_HOLD_VELOCITY_THRESHOLD = max(0.0, {{SERVOJ_HOLD_VELOCITY_THRESHOLD_REPLACE}})"),
              std::string::npos);
    EXPECT_NE(content.find("SERVOJ_HOLD_STABLE_TIME = max(0.0, {{SERVOJ_HOLD_STABLE_TIME_REPLACE}})"), std::string::npos);

    EXPECT_NE(content.find("if SERVOJ_DECELERATE_TIME <= 0.0:"), std::string::npos);
    EXPECT_NE(content.find("return max(0.0, 1.0 - decelerate_duration / SERVOJ_DECELERATE_TIME)"), std::string::npos);
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
