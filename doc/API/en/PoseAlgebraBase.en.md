# PoseAlgebraBase Module

## Introduction

The PoseAlgebraBase module defines a common pose algebra interface for plugin implementations.
The SDK currently provides two built-in plugin implementations:

- ELITE::ElitePoseAlgebra
- ELITE::EigenPoseAlgebra

This interface supports basic pose operations (inverse, compose, add, subtract, conversion, distance), and coordinate frame conversion between world and local frames.

---

## Header Files

```cpp
// Pose algebra interface
#include <Elite/PoseAlgebraBase.hpp>

// Plugin loading (when loading pose algebra plugins at runtime)
#include <Elite/ClassLoader.hpp>
```

---

## Coordinate Frame Conversion APIs (New)

These APIs convert poses between:

- world frame (global/base frame)
- local frame (a reference frame defined by world_ref_pose)

### Definitions

- world_ref_pose: Pose of the local reference frame expressed in world frame.
- world_pose: Target pose expressed in world frame.
- local_pose: Target pose expressed in the local frame.

### Mathematical relationship

```text
local_pose = inverse(world_ref_pose) * world_pose
world_pose = world_ref_pose * local_pose
```

---

## Matrix-based interfaces

```cpp
virtual bool worldToLocal(
    const PoseMatrix& world_ref_pose,
    const PoseMatrix& world_pose,
    PoseMatrix& local_pose,
    PoseAlgebraResult& result
) const = 0;

virtual bool localToWorld(
    const PoseMatrix& world_ref_pose,
    const PoseMatrix& local_pose,
    PoseMatrix& world_pose,
    PoseAlgebraResult& result
) const = 0;
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| world_ref_pose | const PoseMatrix& | Local frame pose in world frame. |
| world_pose | const PoseMatrix& | Input target pose in world frame. Only for worldToLocal. |
| local_pose | PoseMatrix& or const PoseMatrix& | Output local pose in worldToLocal, or input local pose in localToWorld. |
| world_pose (output) | PoseMatrix& | Output world pose in localToWorld. |
| result | PoseAlgebraResult& | Detailed status (error code and message). |

### Return value

- true: conversion succeeded.
- false: conversion failed; inspect result.error and result.message.

---

## Vector-based interfaces

```cpp
virtual bool worldToLocal(
    const vector6d_t& world_ref_pose,
    const vector6d_t& world_pose,
    vector6d_t& local_pose,
    PoseAlgebraResult& result
) const = 0;

virtual bool localToWorld(
    const vector6d_t& world_ref_pose,
    const vector6d_t& local_pose,
    vector6d_t& world_pose,
    PoseAlgebraResult& result
) const = 0;
```

The vector format is:

```text
[x, y, z, roll, pitch, yaw]
```

Units:

- translation: meters
- rotation: radians

---

## Usage Example

```cpp
ELITE::vector6d_t base_pose   = {0.4, -0.2, 0.5, 0.1, 0.2, -0.3};
ELITE::vector6d_t tool_offset = {0.05, 0.0, 0.12, 0.0, 0.0, 1.570796};
ELITE::vector6d_t world_pose;
ELITE::vector6d_t local_pose;
ELITE::PoseAlgebraResult result;

// local -> world
if (!pose_algebra->localToWorld(base_pose, tool_offset, world_pose, result)) {
    // handle error
}

// world -> local (recover tool_offset)
if (!pose_algebra->worldToLocal(base_pose, world_pose, local_pose, result)) {
    // handle error
}
```

---

## Notes

1. For plugin usage, ensure both application and plugins are linked with the SDK shared library.
2. Inputs must be finite values; invalid matrices or non-finite vectors will return failure.
3. For matrix interfaces, rotation validity and homogeneous transform constraints are validated by plugin implementations.
