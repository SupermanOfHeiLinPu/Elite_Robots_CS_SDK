# Changelog for Elite Robots CS SDK

## [Unreleased]

### Added
- `ControllerLog::downloadSystemLog()`：下载机器人系统日志的接口。
- `UPGRADE::upgradeControlSoftware()`：远程升级机器人控制软件的接口。
- `EliteDriver`：
  - `writeFreedrive()`：新增 Freedrive 控制接口。
  - `registerRobotExceptionCallback()`： 新增机器人异常回调注册接口
  - `EliteDriver()`: 构造函数新增`stopj_acc`参数（停止运动的加速度）。
  - `writeServoj()`: 新增`cartesian`和`queue_mode`参数。
- `EliteDriverConfig`：新增构造配置类，支持更灵活的初始化。
  - 通过 `EliteDriver(const EliteDriverConfig& config)` 构造时，不再强制要求本地 IP。
  - 新增`servoj_queue_pre_recv_size`与`servoj_queue_pre_recv_timeout`参数。
- `PrimaryPortInterface`：
  - `getLocalIP()`：新增获取本地 IP 的接口。
  - `registerRobotExceptionCallback()`： 新增机器人异常回调注册接口
- 新增机器人异常接口：
  - `RobotException`：基类。
  - `RobotError`：机器人错误。
  - `RobotRuntimeException`：机器人脚本运行时错误。
- `DashboardClient`
  - 新增`sendAndReceive()`接口。
- 新增完整的 API 文档（Markdown）。
- 新增编译向导文档。

### Changed
- 重构 `TcpServer` 的多端口监听逻辑，统一在单线程中处理以下端口：
  - Reverse port
  - Trajectory port
  - Script sender port
  - Script command port
- 优化 `PrimaryPortInterface`：
  - 改用同步方式接收和解析数据。
  - 支持断线后自动重连。
- 优化 RTSI 模块性能：
  - 减少 `RtsiClientInterface` 的 Socket 数据拷贝次数。
  - 简化 `RtsiIOInterface.hpp` 的后台线程循环逻辑。
  - `RtsiIOInterface::getRecipeValue()` 与 `RtsiIOInterface::setInputRecipeValue()`接口从private变为public，并且添加了常用的显式实例化声明。
  - `RtsiIOInterface`改为保护继承。
- 调整了项目Readme的结构。
- 增强`external_control.script`的线程安全性
- 使用`boost::program_options`来解析示例程序的输入参数。


### Fixed
- 修复 `EliteDriver` 析构时可能因悬垂指针导致的崩溃问题。
- 修复 `DashboardClient::setSpeedScaling()` 接口设置新值返回失败的问题。

### Deprecated
- `EliteDriver` 的旧构造函数已废弃，未来版本将移除，请改用 `EliteDriverConfig`。

### Removed
- 移除`EliteException::Code::SOCKET_OPT_CANCEL` 

## [v1.1.0] - 2024-10-30
### Initial Release
- 首次公开版本发布。