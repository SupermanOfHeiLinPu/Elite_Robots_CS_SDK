# EliteDriver

## Class
- EliteDriver

## Background
这个类提供了控制机器人运动、配置机器人的接口。

## Prerequisites
- 确保正确安装了SDK。

## Usage

下载`ExternalControl`插件，安装到机器人上，在配置中将`Host IP`设置为你的PC的IP，在任务树中插入`ExternalControl`节点。  
创建一个代码文件：
```bash
touch elite_driver_example.cpp
```

复制下面代码，使用你习惯的文本编辑器，粘贴到`elite_driver_example.cpp`中：
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

使用gcc编译器，编译`elite_driver_example.cpp`：
```bash
g++ elite_driver_example.cpp -o elite_driver_example -lelite-cs-series-sdk
```

拷贝`example/resource/`下的两个RTSI配方文件到当前目录。  

因为此代码会操作机器人运动，因此开始运行前请确保安全。
```
./elite_driver_example <robot ip> <locol ip>
```

一切正常的话机器人会在垂直平面内运动一个三角形轨迹。

## Examine the code

代码开头部分是C++的头文件，`EliteDriver.hpp`头文件包含了控制机器人运动的接口；`RtsiIOInterface.hpp`是RTSI读写机器人IO的接口，本示例中需要使用RTSI获取机器人的实时关节角。`DashboardClient.hpp`提供了Dashboard操作接口，本示例中使用此接口来让机器人开始运行任务；`DataType.hpp`头文件中包含了SDK中常用的数据类型。在SDK特有的头文件声明之后是C++标准库文件的声明。接着声明了`EliteDriver`、`RtsiIOInterface`、`DashboardClient`这几个类的指针。  
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


main函数是程序开始的地方，main函数中最开始的部分的功能是：如果运行`elite_driver_example`时，后面没有跟机器人IP和本机IP，则打印提示信息，并异常退出程序。

```cpp
int main(int argc, const char** argv) {
    if (argc < 3) {
        std::cout << "Must provide robot ip or local ip. Command like: ./trajectory_example 192.168.1.250 192.168.1.251" << std::endl;
        return 1;
    }
```

创建`EliteDriver`、`RtsiIOInterface`、`DashboardClient`这三个类的实例，并给指针赋值。`EliteDriver`的构造函数有三个参数需要提供，分别是机器人IP、本机IP、控制脚本（控制脚本在安装的时候会安装到`/usr/local/share/Elite/`目录下）。
```cpp
    s_driver = std::make_unique<EliteDriver>(argv[1], argv[2], "/usr/local/share/Elite/external_control.script");
    s_rtsi_client = std::make_unique<RtsiIOInterface>("output_recipe.txt", "input_recipe.txt", 250);
    s_dashboard = std::make_unique<DashboardClient>();
```

调用RTSI和Dashboard的`connect()`方法来连接到机器人
```cpp
    if (!s_dashboard->connect(argv[1])) {
        return 1;
    }
    std::cout << "Dashboard connected" << std::endl;

    s_rtsi_client->connect(argv[1]);
    std::cout << "RTSI connected" << std::endl;
```

设置轨迹运行完成的回调函数。此处使用了一个lambda表达式，如果运动结果为成功则将运动结束的标志变量置为true。
```cpp
    bool is_move_finish = false;
    s_driver->setTrajectoryResultCallback([&](TrajectoryMotionResult result){
        if (result == TrajectoryMotionResult::SUCCESS) {
            is_move_finish = true;
        }
    });
```

调用dashboard接口为机器人上电，释放抱闸，运行任务。开始运行任务后，任务树中的`ExternalControl`插件会请求一个控制脚本，而`EliteDriver`在接受到请求后会自动发送控制脚本，即`/usr/local/share/Elite/external_control.script`。
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

等待`EliteDrvier`与机器人建立连接。
```cpp
    while (!s_driver->isRobotConnected()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
```

通过RTSI获取机器人当前关节角，调用`EliteDriver`的`writeTrajectoryControlAction()`函数，发送开始运动的指令，点位数量为1，设置下次发送指令的超时时间为200ms（即200ms内需要给出下一条指令）。  
```cpp
    vector6d_t actual_joints = s_rtsi_client->getActualJointPositions();

    s_driver->writeTrajectoryControlAction(ELITE::TrajectoryControlAction::START, 1, 200);
```

将第四关节角设置为-1.57后调用`writeTrajectoryPoint()`函数发送给机器人。
```cpp
    actual_joints[3] = -1.57;
    s_driver->writeTrajectoryPoint(actual_joints, 3, 0, false);

    
```

等待运动结束，等待期间持续调用`writeTrajectoryControlAction()`函数给机器人发送空指令，否则会退出外部控制。
```cpp
    while (!is_move_finish) {
        s_driver->writeTrajectoryControlAction(ELITE::TrajectoryControlAction::NOOP, 0, 200);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    std::cout << "Joints moved to target" << std::endl;
```

通过RTSI获取到当前位姿。清除运动结束的标志位。调用`EliteDriver`的`writeTrajectoryControlAction()`函数，发送开始运动的指令，点位数量为3，设置下次发送指令的超时时间为200ms（即200ms内需要给出下一条指令）。 
```cpp
    vector6d_t actual_pose = s_rtsi_client->getAcutalTCPPose();

    is_move_finish = false;

    s_driver->writeTrajectoryControlAction(ELITE::TrajectoryControlAction::START, 3, 200);
```

设置点位，并发送给机器人，注意此处的点位是笛卡尔空间中的坐标，而之前的点位是关节空间的。
```cpp
    actual_pose[2] -= 0.2;
    s_driver->writeTrajectoryPoint(actual_pose, 3, 0.05, true);

    actual_pose[1] -= 0.2;
    s_driver->writeTrajectoryPoint(actual_pose, 3, 0.05, true);

    actual_pose[1] += 0.2;
    actual_pose[2] += 0.2;
    s_driver->writeTrajectoryPoint(actual_pose, 3, 0.05, true);
```

等待运动结束，等待期间持续调用`writeTrajectoryControlAction()`函数给机器人发送空指令，否则会退出外部控制。
```cpp
    while (!is_move_finish) {
        s_driver->writeTrajectoryControlAction(ELITE::TrajectoryControlAction::NOOP, 0, 200);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    std::cout << "Line moved to target" << std::endl;
```

发送取消轨迹运动的指令（因为到这里轨迹运动已经结束了，所以此处仅展示此功能）。调用`stopControl()`函数来结束控制。
```cpp
    s_driver->writeTrajectoryControlAction(ELITE::TrajectoryControlAction::CANCEL, 1, 200);
    s_driver->stopControl();
```

除了上述的接口以外，`EliteDriver`还有其他的接口，运动接口使用方式与示例中相似，非运动接口可直接查看其说明。
