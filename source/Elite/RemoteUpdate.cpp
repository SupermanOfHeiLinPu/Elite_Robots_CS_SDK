#include <iostream>
#include <cstdio>
#include <string>
#if defined(__linux) || defined(linux) || defined(__linux__)
#include <unistd.h>
#include <sys/wait.h>
#include <pty.h>
#endif

#include "Elite/Logger.hpp"


namespace ELITE
{

namespace OTA
{

bool otaControlSoftware(std::string ip, std::string file, std::string password) {
    ELITE_LOG_INFO("OTA control software begin");
#if defined(__linux) || defined(linux) || defined(__linux__)
    int master_fd;
    pid_t pid = forkpty(&master_fd, nullptr, nullptr, nullptr);
    if (pid < 0) {
        ELITE_LOG_FATAL("OTA control software fail: fork failed");
        return false;
    }

    if (pid == 0) {
        // child process (run the update file)

        // Execute update script
        std::string script_arg = ("-ip=" + ip);
        execlp(file.c_str(), file.c_str(), script_arg.c_str(), NULL);
        ELITE_LOG_FATAL("OTA control software fail");
        return false;
    } else {        
        char buffer[4096];
        std::string script_output;
        ssize_t bytes_read;

        while (true) {
            // Read update script output
            bytes_read = read(master_fd, buffer, sizeof(buffer) - 1);
            // Script finish
            if (bytes_read <= 0) {
                ELITE_LOG_INFO("OTA control software finish");
                break;
            }
            buffer[bytes_read] = '\0';
            script_output += buffer;
            ELITE_LOG_DEBUG(buffer);
            // If you have never connected to the robot over ssh, you will have the following prompt
            if (script_output.find("Are you sure you want to continue connecting") != std::string::npos) {
                if(write(master_fd, "yes\n", sizeof("yes\n")) <= 0) {
                    ELITE_LOG_ERROR("OTA control software fail: write() fail");
                    break;
                }
            } else if (script_output.find("password") != std::string::npos) { // Need enter password
                password += "\n";
                if(write(master_fd, password.c_str(), password.size()) <= 0) {
                    ELITE_LOG_ERROR("OTA control software fail: write() fail");
                    break;
                }
                
            }
            script_output.clear();
        }
    }
    close(master_fd);
    // wait child finish
    wait(NULL);
#else
    return false;
#endif
    return true;
}


} // namespace OTA


} // namespace ELITE


