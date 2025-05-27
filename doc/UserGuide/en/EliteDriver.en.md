# EliteDriver

## Class
- EliteDriver

## Background
This class provides interfaces for controlling robot movement and configuring the robot.

## Prerequisites
- Ensure that the SDK is installed correctly.

## Usage

Download the `ExternalControl` plugin and install it on the robot. In the configuration, set `Host IP` to the IP of your PC and insert the `ExternalControl` node in the task tree.
Create a code file:
```bash
touch elite_driver_example.cpp
```

Copy the following code and paste it into `elite_driver_example.cpp` using your preferred text editor:
```cpp
#include <Elite/DashboardClient.hpp>
#include <Elite/DataType.hpp>
#include <Elite/EliteDriver.hpp>
#include <Elite/RtsiIOInterface.hpp>

#include <iostream>
#include <memory>
#include <thread>

using namespace ELITE;

static std::unique_ptr<EliteDriver> s_driver;
static std::unique_ptr<RtsiIOInterface> s_rtsi_client;
static std::unique_ptr<DashboardClient> s_dashboard;

int main(int argc, const char** argv) {
    if (argc < 2) {
        std::cout << "Must provide robot ip or local ip. Command like: \"./trajectory_example robot_ip\" or \"./trajectory_example "
                     "robot_ip local_ip\""
                  << std::endl;
        return 1;
    }
    std::string robot_ip = argv[1];
    std::string local_ip = "";
    if (argc >= 3) {
        local_ip = argv[2];
    }
    EliteDriverConfig config;
    config.robot_ip = robot_ip;
    config.script_file_path = "external_control.script";
    config.local_ip = local_ip;
    s_driver = std::make_unique<EliteDriver>(config);

    s_rtsi_client = std::make_unique<RtsiIOInterface>("output_recipe.txt", "input_recipe.txt", 250);
    s_dashboard = std::make_unique<DashboardClient>();

    if (!s_dashboard->connect(argv[1])) {
        return 1;
    }
    std::cout << "Dashboard connected" << std::endl;

    s_rtsi_client->connect(argv[1]);
    std::cout << "RTSI connected" << std::endl;

    bool is_move_finish = false;
    s_driver->setTrajectoryResultCallback([&](TrajectoryMotionResult result) {
        if (result == TrajectoryMotionResult::SUCCESS) {
            is_move_finish = true;
        }
    });

    if (!s_dashboard->powerOn()) {
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

    vector6d_t actual_joints = s_rtsi_client->getActualJointPositions();

    s_driver->writeTrajectoryControlAction(ELITE::TrajectoryControlAction::START, 1, 200);

    actual_joints[3] = -1.57;
    s_driver->writeTrajectoryPoint(actual_joints, 3, 0, false);

    while (!is_move_finish) {
        s_driver->writeTrajectoryControlAction(ELITE::TrajectoryControlAction::NOOP, 0, 200);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    std::cout << "Joints moved to target" << std::endl;

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
    std::cout << "Line moved to target" << std::endl;

    s_driver->writeTrajectoryControlAction(ELITE::TrajectoryControlAction::CANCEL, 1, 200);
    s_driver->stopControl();
    return 0;
}
```

Compile `elite_driver_example.cpp` using the gcc compiler:
```bash
g++ elite_driver_example.cpp -o elite_driver_example -lelite-cs-series-sdk
```

Copy the two RTSI recipe files under `example/resource/` to the current directory.

Since this code will operate the robot's movement, please ensure safety before starting to run.
```
./elite_driver_example <robot ip> <local ip>
```

If everything is normal, the robot will move in a triangular trajectory in the vertical plane.

## Examine the code

At the beginning of the code are C++ header files. The `EliteDriver.hpp` header file contains interfaces for controlling robot movement. The `RtsiIOInterface.hpp` is the interface for reading and writing robot IOs through RTSI. In this example, RTSI is needed to obtain the real-time joint angles of the robot. The `DashboardClient.hpp` provides the Dashboard operation interface. In this example, this interface is used to make the robot start running tasks. The `DataType.hpp` header file contains commonly used data types in the SDK. After the declarations of the SDK-specific header files are the declarations of C++ standard library files. Then the pointers of the classes `EliteDriver`, `RtsiIOInterface`, and `DashboardClient` are declared.
```cpp
#include <Elite/EliteDriver.hpp>
#include <Elite/RtsiIOInterface.hpp>
#include <Elite/DataType.hpp>
#include <Elite/DashboardClient.hpp>

#include <memory>
#include <thread>
#include <iostream>

using namespace ELITE;

static std::unique_ptr<EliteDriver> s_driver;
static std::unique_ptr<RtsiIOInterface> s_rtsi_client;
static std::unique_ptr<DashboardClient> s_dashboard;

```


The main function is where the program starts. The function of the beginning part of the main function is: if there are no robot IP and local IP provided when running `elite_driver_example`, print a prompt message and exit the program abnormally.

```cpp
int main(int argc, const char** argv) {
    if (argc < 3) {
        std::cout << "Must provide robot ip or local ip. Command like:./trajectory_example 192.168.1.250 192.168.1.251" << std::endl;
        return 1;
    }
```

Create instances of the classes `EliteDriver`, `RtsiIOInterface`, and `DashboardClient`, and assign values to the pointers. The constructor of `EliteDriver` requires three parameters to be provided, which are the robot IP, the local IP, and the control script (the control script will be installed under the `/usr/local/share/Elite/` directory during installation).
```cpp
    s_driver = std::make_unique<EliteDriver>(argv[1], argv[2], "/usr/local/share/Elite/external_control.script");
    s_rtsi_client = std::make_unique<RtsiIOInterface>("output_recipe.txt", "input_recipe.txt", 250);
    s_dashboard = std::make_unique<DashboardClient>();
```

Call the `connect()` methods of RTSI and Dashboard to connect to the robot.
```cpp
    if (!s_dashboard->connect(argv[1])) {
        return 1;
    }
    std::cout << "Dashboard connected" << std::endl;

    s_rtsi_client->connect(argv[1]);
    std::cout << "RTSI connected" << std::endl;
```

Set the callback function for the completion of the trajectory. Here, a lambda expression is used. If the motion result is success, set the motion end flag variable to true.
```cpp
    bool is_move_finish = false;
    s_driver->setTrajectoryResultCallback([&](TrajectoryMotionResult result){
        if (result == TrajectoryMotionResult::SUCCESS) {
            is_move_finish = true;
        }
    });
```

Call the dashboard interfaces to power on the robot, release the brake, and run the task. After starting to run the task, the `ExternalControl` plugin in the task tree will request a control script. When `EliteDriver` receives the request, it will automatically send the control script, that is, `/usr/local/share/Elite/external_control.script`.
```cpp
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
```

Wait for `EliteDrvier` to establish a connection with the robot.
```cpp
    while (!s_driver->isRobotConnected()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
```

Obtain the current joint angles of the robot through RTSI. Call the `writeTrajectoryControlAction()` function of `EliteDriver` to send the start movement command. The number of points is 1, and the timeout for sending the next command is set to 200ms (that is, the next command needs to be given within 200ms).
```cpp
    vector6d_t actual_joints = s_rtsi_client->getActualJointPositions();

    s_driver->writeTrajectoryControlAction(ELITE::TrajectoryControlAction::START, 1, 200);
```

Set the fourth joint angle to -1.57 and then call the `writeTrajectoryPoint()` function to send it to the robot.
```cpp
    actual_joints[3] = -1.57;
    s_driver->writeTrajectoryPoint(actual_joints, 3, 0, false);

    
```

Wait for the movement to end. During the waiting period, continuously call the `writeTrajectoryControlAction()` function to send a null command to the robot; otherwise, it will exit external control.
```cpp
    while (!is_move_finish) {
        s_driver->writeTrajectoryControlAction(ELITE::TrajectoryControlAction::NOOP, 0, 200);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    std::cout << "Joints moved to target" << std::endl;
```

Obtain the current pose through RTSI. Clear the movement end flag. Call the `writeTrajectoryControlAction()` function of `EliteDriver` to send the start movement command. The number of points is 3, and the timeout for sending the next command is set to 200ms (that is, the next command needs to be given within 200ms).
```cpp
    vector6d_t actual_pose = s_rtsi_client->getAcutalTCPPose();

    is_move_finish = false;

    s_driver->writeTrajectoryControlAction(ELITE::TrajectoryControlAction::START, 3, 200);
```

Set the points and send them to the robot. Note that the points here are coordinates in Cartesian space, while the previous points are in joint space.
```cpp
    actual_pose[2] -= 0.2;
    s_driver->writeTrajectoryPoint(actual_pose, 3, 0.05, true);

    actual_pose[1] -= 0.2;
    s_driver->writeTrajectoryPoint(actual_pose, 3, 0.05, true);

    actual_pose[1] += 0.2;
    actual_pose[2] += 0.2;
    s_driver->writeTrajectoryPoint(actual_pose, 3, 0.05, true);
```

Wait for the movement to end. During the waiting period, continuously call the `writeTrajectoryControlAction()` function to send a null command to the robot; otherwise, it will exit external control.
```cpp
    while (!is_move_finish) {
        s_driver->writeTrajectoryControlAction(ELITE::TrajectoryControlAction::NOOP, 0, 200);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    std::cout << "Line moved to target" << std::endl;
```

Send the command to cancel the trajectory movement (since the trajectory movement has ended here, so this is only to demonstrate this function). Call the `stopControl()` function to end the control.
```cpp
    s_driver->writeTrajectoryControlAction(ELITE::TrajectoryControlAction::CANCEL, 1, 200);
    s_driver->stopControl();
```

In addition to the interfaces mentioned above, `EliteDriver` also has other interfaces. The usage of motion interfaces is similar to that in the example. For non-motion interfaces, you can directly view their descriptions.