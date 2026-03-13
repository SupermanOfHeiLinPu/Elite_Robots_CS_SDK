[Home](./UserGuide.cn.md)

# 运动学插件

## 目标

安装运动学插件的依赖，编译运动学插件（基于 KDL），并通过 `ClassLoader` 加载插件，使用正运动学（FK）和逆运动学（IK）接口。

## 背景说明

SDK 提供了一套仿照 ROS 2 ClassLoader 实现的插件加载机制（位于 `include/ClassLoader/` 和 `source/ClassLoader/`），允许用户在运行时动态加载共享库，并实例化其中注册的派生类对象。

运动学插件基于 [Orocos KDL](https://www.orocos.org/kdl.html) 实现，提供正向运动学（FK）和逆向运动学（IK）求解，插件的基类接口定义在 `include/KinematicsBase/KinematicsBase.hpp` 中，KDL 的具体实现位于 `plugin/kinematics/KdlKinematicsPlugin/`。

核心类与宏说明：

| 名称 | 文件 | 说明 |
|------|------|------|
| `ELITE::ClassLoader` | `include/ClassLoader/ClassLoader.hpp` | 运行时加载共享库，并创建注册的插件实例 |
| `ELITE::KinematicsBase` | `include/KinematicsBase/KinematicsBase.hpp` | 运动学求解器的抽象基类 |
| `ELITE::KdlKinematicsPlugin` | `plugin/kinematics/KdlKinematicsPlugin/` | 基于 KDL 的运动学插件，继承自 `KinematicsBase` |
| `ELITE_CLASS_LOADER_REGISTER_CLASS` | `include/ClassLoader/ClassRegisterMacro.hpp` | 将派生类注册到插件系统的宏 |

## 依赖安装

### 基础依赖（SDK 本体）

安装 SDK 的基础依赖，具体请参考[编译安装指南](../../BuildGuide/BuildGuide.cn.md)。

### 运动学插件专属依赖

运动学插件需要额外安装 **Orocos KDL** 和 **Eigen3**：

```bash
sudo apt update
sudo apt install liborocos-kdl-dev
sudo apt install libeigen3-dev
```

## 编译

### 编译 SDK 与运动学插件

在 CMake 配置时，使用 `-DELITE_COMPILE_KIN_PLUGIN=ON` 选项来同时编译运动学插件：

```bash
cd <clone of this repository>

mkdir build && cd build

cmake -DELITE_COMPILE_KIN_PLUGIN=ON ..

make -j
```

编译成功后，运动学插件的共享库 `libelite_kdl_kinematics.so` 会输出到 `build/plugin/kinematics/` 目录。

### 安装（可选）

```bash
sudo make install
sudo ldconfig
```

安装后，插件共享库会位于 `<CMAKE_INSTALL_LIBDIR>/elite-cs-series-sdk-plugins/kinematics/` 目录中。

## 使用方式

### 1. 获取机器人 MDH 参数

运动学插件需要机器人的 MDH（Modified Denavit-Hartenberg）参数来初始化求解器。这些参数可通过连接机器人的 **30001 主端口** 获取：

```cpp
#include <Elite/PrimaryPortInterface.hpp>
#include <Elite/RobotConfPackage.hpp>

auto primary = std::make_unique<ELITE::PrimaryPortInterface>();
auto kin_info = std::make_shared<ELITE::KinematicsInfo>();

if (!primary->connect(robot_ip, 30001)) {
    // 连接失败处理
}
if (!primary->getPackage(kin_info, 200)) {
    // 获取失败处理
}
primary->disconnect();
// kin_info->dh_alpha_, kin_info->dh_a_, kin_info->dh_d_ 即为 MDH 参数
```

### 2. 加载插件

使用 `ClassLoader` 动态加载运动学插件的共享库：

```cpp
#include <Elite/ClassLoader.hpp>
#include <Elite/KinematicsBase.hpp>

// 传入共享库的路径
ELITE::ClassLoader loader("./libelite_kdl_kinematics.so");

if (!loader.loadLib()) {
    // 加载失败处理
}
```

### 3. 创建插件实例

通过 `createUniqueInstance` 创建 `KinematicsBase` 的实例，传入在插件中注册的派生类全限定名称：

```cpp
auto kin_solver = loader.createUniqueInstance<ELITE::KinematicsBase>("ELITE::KdlKinematicsPlugin");
if (kin_solver == nullptr) {
    // 创建失败处理
}
```

> **说明：** `"ELITE::KdlKinematicsPlugin"` 是 KDL 运动学插件通过 `ELITE_CLASS_LOADER_REGISTER_CLASS` 宏注册到插件系统时使用的类名。

### 4. 初始化求解器（设置 MDH 参数）

```cpp
kin_solver->setMDH(kin_info->dh_alpha_, kin_info->dh_a_, kin_info->dh_d_);
```

### 5. 正向运动学（FK）

给定关节角度，计算末端执行器的位姿（x, y, z, roll, pitch, yaw）：

```cpp
ELITE::vector6d_t joint_angles = { /* 6 个关节角度，单位：弧度 */ };
ELITE::vector6d_t fk_pose;

if (!kin_solver->getPositionFK(joint_angles, fk_pose)) {
    // FK 失败处理
}
// fk_pose = { x, y, z, roll, pitch, yaw }
```

### 6. 逆向运动学（IK）

给定末端位姿（x, y, z, roll, pitch, yaw）和参考关节角度，求解关节角度：

```cpp
ELITE::vector6d_t target_pose = { /* x, y, z, roll, pitch, yaw */ };
ELITE::vector6d_t near_joints = { /* 参考关节角度（初始猜测） */ };
ELITE::vector6d_t ik_solution;
ELITE::KinematicsResult ik_result;

if (!kin_solver->getPositionIK(target_pose, near_joints, ik_solution, ik_result)) {
    // IK 失败处理，可通过 ik_result.kinematic_error 查看错误原因
}
// ik_solution 即为求解到的关节角度
```

`KinematicsResult::kinematic_error` 的可能取值：

| 枚举值 | 说明 |
|--------|------|
| `KinematicError::OK` | 求解成功 |
| `KinematicError::SOLVER_NOT_ACTIVE` | 求解器未初始化，请先调用 `setMDH()` |
| `KinematicError::NO_SOLUTION` | 无法找到满足目标位姿的关节解 |

## 完整示例

完整的运动学插件使用示例请参考 `example/kinematics_example.cpp`。

该示例展示了如下完整流程：
1. 连接机器人 30001 端口，获取 MDH 参数
2. 连接机器人 RTSI 端口，读取当前关节角度和末端位姿
3. 加载 KDL 运动学插件
4. 调用 FK 和 IK 接口，并输出结果

编译并运行示例：

```bash
# 编译时开启示例编译选项
cmake -DELITE_COMPILE_EXAMPLES=ON -DELITE_COMPILE_KIN_PLUGIN=ON ..
make -j

# 将插件库拷贝到示例程序所在目录（或配置 LD_LIBRARY_PATH）
cp build/plugin/kinematics/libelite_kdl_kinematics.so build/example/

# 运行示例
./build/example/kinematics_example --robot-ip <机器人IP>
```

## 自定义运动学插件

如果需要实现自定义的运动学求解器，可以参照 KDL 插件的实现方式：

1. 继承 `ELITE::KinematicsBase`，实现 `setMDH()`、`getPositionFK()` 和 `getPositionIK()` 接口。
2. 在源文件末尾使用 `ELITE_CLASS_LOADER_REGISTER_CLASS` 宏注册插件类：

```cpp
#include <Elite/ClassRegisterMacro.hpp>
ELITE_CLASS_LOADER_REGISTER_CLASS(YourNamespace::YourKinematicsPlugin, ELITE::KinematicsBase);
```

3. 将插件编译为共享库（`.so`），使用 `ClassLoader` 加载时传入对应类名字符串即可。

---
[Home](./UserGuide.cn.md)
