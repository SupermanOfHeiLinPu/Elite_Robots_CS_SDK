[Home](./UserGuide.cn.md)

# 透传:servoj

## 目标
调用SDK的相关接口，让机器人以当前位姿为起点，末端正反转。

## 背景说明
SDK 控制机器人的方式是：通过插件或30001发送一个控制脚本给机器人运行，这个控制脚本会通过TCP/IP协议与SDK连接，并接收控制指令。

在继续阅读此章节之前，请确保已经了解了 [`DashboardClient`](./Power-on-Robot.cn.md) 和 [`RtsiIOInterface`](./Get-Robot-State.cn.md) 两个类的使用。

## 任务

### 1. 
