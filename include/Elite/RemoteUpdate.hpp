#include <string>

namespace ELITE
{

namespace OTA
{

/**
 * @brief Upgrade the robot control software
 * 
 * @param ip Robot ip
 * @param file Upgrade file
 * @param password robot controller ssh password
 * @return true success
 * @return false fail
 * @note Windows not support yet
 */
bool otaControlSoftware(std::string ip, std::string file, std::string password);


}
}