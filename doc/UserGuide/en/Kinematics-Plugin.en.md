[Home](./UserGuide.en.md)

# Kinematics Plugin

## Goal

Install the dependencies for the kinematics plugin, build the KDL-based kinematics plugin, load it at runtime using `ClassLoader`, and use the Forward Kinematics (FK) and Inverse Kinematics (IK) interfaces.

## Background

The SDK provides a plugin loading mechanism inspired by ROS 2's ClassLoader (located in `include/ClassLoader/` and `source/ClassLoader/`). This allows users to dynamically load shared libraries at runtime and instantiate registered derived class objects.

The kinematics plugin is implemented on top of [Orocos KDL](https://www.orocos.org/kdl.html) and provides FK and IK solvers. The abstract base class interface is defined in `include/KinematicsBase/KinematicsBase.hpp`, and the KDL-based implementation is located in `plugin/kinematics/KdlKinematicsPlugin/`.

Key classes and macros:

| Name | File | Description |
|------|------|-------------|
| `ELITE::ClassLoader` | `include/ClassLoader/ClassLoader.hpp` | Loads a shared library at runtime and creates registered plugin instances |
| `ELITE::KinematicsBase` | `include/KinematicsBase/KinematicsBase.hpp` | Abstract base class for kinematics solvers |
| `ELITE::KdlKinematicsPlugin` | `plugin/kinematics/KdlKinematicsPlugin/` | KDL-based kinematics plugin, inherits from `KinematicsBase` |
| `ELITE_CLASS_LOADER_REGISTER_CLASS` | `include/ClassLoader/ClassRegisterMacro.hpp` | Macro used to register a derived class with the plugin system |

## Installing Dependencies

### SDK Base Dependencies

Install the SDK base dependencies first. Refer to the [Build and Installation Guide](../../BuildGuide/BuildGuide.en.md) for details.

### Kinematics Plugin Dependencies

The kinematics plugin additionally requires **Orocos KDL** and **Eigen3**:

```bash
sudo apt update
sudo apt install liborocos-kdl-dev
sudo apt install libeigen3-dev
```

## Building

### Build the SDK and Kinematics Plugin

Use the `-DELITE_COMPILE_KIN_PLUGIN=ON` CMake option to build the kinematics plugin together with the SDK:

```bash
cd <clone of this repository>

mkdir build && cd build

cmake -DELITE_COMPILE_KIN_PLUGIN=ON ..

make -j
```

After a successful build, the kinematics plugin shared library `libelite_kdl_kinematics.so` will be output to the `build/plugin/kinematics/` directory.

### Install (Optional)

```bash
sudo make install
sudo ldconfig
```

After installation, the plugin shared library will be located under `<CMAKE_INSTALL_LIBDIR>/elite-cs-series-sdk-plugins/kinematics/`.

## Usage

### 1. Retrieve MDH Parameters from the Robot

The kinematics plugin requires the robot's MDH (Modified Denavit-Hartenberg) parameters to initialize its solver. These can be obtained by connecting to the robot's **30001 primary port**:

```cpp
#include <Elite/PrimaryPortInterface.hpp>
#include <Elite/RobotConfPackage.hpp>

auto primary = std::make_unique<ELITE::PrimaryPortInterface>();
auto kin_info = std::make_shared<ELITE::KinematicsInfo>();

if (!primary->connect(robot_ip, 30001)) {
    // Handle connection failure
}
if (!primary->getPackage(kin_info, 200)) {
    // Handle retrieval failure
}
primary->disconnect();
// kin_info->dh_alpha_, kin_info->dh_a_, kin_info->dh_d_ are the MDH parameters
```

### 2. Load the Plugin

Use `ClassLoader` to dynamically load the kinematics plugin shared library:

```cpp
#include <Elite/ClassLoader.hpp>
#include <Elite/KinematicsBase.hpp>

// Provide the path to the shared library
ELITE::ClassLoader loader("./libelite_kdl_kinematics.so");

if (!loader.loadLib()) {
    // Handle load failure
}
```

### 3. Create a Plugin Instance

Use `createUniqueInstance` to create an instance of `KinematicsBase`, passing the fully qualified name of the derived class registered in the plugin:

```cpp
auto kin_solver = loader.createUniqueInstance<ELITE::KinematicsBase>("ELITE::KdlKinematicsPlugin");
if (kin_solver == nullptr) {
    // Handle creation failure
}
```

> **Note:** `"ELITE::KdlKinematicsPlugin"` is the class name used when the KDL kinematics plugin registers itself with the plugin system via the `ELITE_CLASS_LOADER_REGISTER_CLASS` macro.

### 4. Initialize the Solver (Set MDH Parameters)

```cpp
kin_solver->setMDH(kin_info->dh_alpha_, kin_info->dh_a_, kin_info->dh_d_);
```

### 5. Forward Kinematics (FK)

Given a set of joint angles, compute the end-effector pose (x, y, z, roll, pitch, yaw):

```cpp
ELITE::vector6d_t joint_angles = { /* 6 joint angles in radians */ };
ELITE::vector6d_t fk_pose;

if (!kin_solver->getPositionFK(joint_angles, fk_pose)) {
    // Handle FK failure
}
// fk_pose = { x, y, z, roll, pitch, yaw }
```

### 6. Inverse Kinematics (IK)

Given a target end-effector pose (x, y, z, roll, pitch, yaw) and a seed joint configuration, compute the joint angles:

```cpp
ELITE::vector6d_t target_pose = { /* x, y, z, roll, pitch, yaw */ };
ELITE::vector6d_t near_joints = { /* seed joint angles (initial guess) */ };
ELITE::vector6d_t ik_solution;
ELITE::KinematicsResult ik_result;

if (!kin_solver->getPositionIK(target_pose, near_joints, ik_solution, ik_result)) {
    // Handle IK failure; inspect ik_result.kinematic_error for details
}
// ik_solution contains the solved joint angles
```

Possible values of `KinematicsResult::kinematic_error`:

| Value | Description |
|-------|-------------|
| `KinematicError::OK` | Solution found successfully |
| `KinematicError::SOLVER_NOT_ACTIVE` | Solver not initialized; call `setMDH()` first |
| `KinematicError::NO_SOLUTION` | No valid joint solution could be found for the target pose |

## Full Example

A complete usage example is available in `example/kinematics_example.cpp`.

This example demonstrates the full workflow:
1. Connect to the robot's 30001 port and retrieve MDH parameters
2. Connect to the robot's RTSI port to read current joint angles and TCP pose
3. Load the KDL kinematics plugin
4. Call FK and IK interfaces and print the results

Build and run the example:

```bash
# Enable example and plugin compilation
cmake -DELITE_COMPILE_EXAMPLES=ON -DELITE_COMPILE_KIN_PLUGIN=ON ..
make -j

# Copy the plugin library to the example directory (or configure LD_LIBRARY_PATH)
cp build/plugin/kinematics/libelite_kdl_kinematics.so build/example/

# Run the example
./build/example/kinematics_example --robot-ip <robot_ip>
```

## Writing a Custom Kinematics Plugin

To implement a custom kinematics solver using the same plugin mechanism:

1. Inherit from `ELITE::KinematicsBase` and implement the `setMDH()`, `getPositionFK()`, and `getPositionIK()` interfaces.
2. At the end of your source file, register the plugin class using the `ELITE_CLASS_LOADER_REGISTER_CLASS` macro:

```cpp
#include <Elite/ClassRegisterMacro.hpp>
ELITE_CLASS_LOADER_REGISTER_CLASS(YourNamespace::YourKinematicsPlugin, ELITE::KinematicsBase);
```

3. Build the plugin as a shared library (`.so`). When loading it with `ClassLoader`, pass the corresponding class name string to `createUniqueInstance`.

---
[Home](./UserGuide.en.md)
