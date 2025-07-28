[English](./README.md)
# Elite Robots CS SDK

此SDK是艾利特机器人CS系列机械臂的C++库，借助该库，开发者可以基于C++实现驱动程序，从而充分利用艾利特机器人CS系列机械臂的多功能性来开发外部应用程序。


## Requirements
 * ***CS Controller*** (robot control software)  
    - For version **2.13.x**, require **≥ 2.13.4** (CS-Series).  
    - For version **2.14.x**, require **≥ 2.14.2**.  
    - If your robot's control software version is lower than these, an upgrade is recommended.

## Build & Install
如果你的操作系统是 Ubuntu20.04、Ubuntu22.04、Ubuntu24.04，那么可以用下面的指令直接安装elite-cs-series-sdk:
```bash
sudo add-apt-repository ppa:elite-robots/cs-robot-series-sdk
sudo apt update
sudo apt install elite-cs-series-sdk
```

如果需要编译安装，参考[编译向导](./doc/BuildGuide/BuildGuide.cn.md)

## User guide
[中文向导](./doc/UserGuide/cn/UserGuide.cn.md)  

## Architecture
[代码架构](./doc/Architecture/Arch.cn.md)

## API 说明
[API](./doc/API/cn/API.cn.md)

## Compatible Operating Systems
在以下系统平台上测试过：

 * Ubuntu 22.04 (Jammy Jellyfish)
 * Ubuntu 16.04 (Xenial Xerus)
 * Windowns 11

## Compiler
目前在以下编译器中通过编译

 * gcc 11.4.0
 * gcc 5.5.0
 * msvc 19.40.33808
