# Changelog for Elite Robots CS SDK

## [Unrelease]

### Added
- Add a "servoj" example with speed planning.
- The `RtsiIOInterface`:
  - Adds a string list constructor.
  - Add an interface to determine if connected: `isConnected()`.
  - Add an interface to determine if data synchronization has started: `isStarted()`.

### Changed
- Adjust the "timeout" value of "trajectory_socket" in `external_control.script`.
- The `RtsiIOInterface` allows input of empty paths and empty lists.

### Fixed
- Fix the issue where the step size calculated by the `extrapolate()` function in `external_control.script` is a fixed steptime.
- Fix the error of missing the "float" type when parsing the 30001 robot error exception message.

## [v1.2.0] - 2025-08-14

### Added
- `ControllerLog::downloadSystemLog()`: Interface for downloading robot system logs.
- `UPGRADE::upgradeControlSoftware()`: Interface for remotely upgrading robot control software.
- `EliteDriver`:
  - `writeFreedrive()`: New interface for Freedrive control.
  - `registerRobotExceptionCallback()`: New interface for registering robot exception callbacks.
  - `EliteDriver()`: Constructor updated with new `stopj_acc` parameter (acceleration for stopping motion).
  - `writeServoj()`: Added `cartesian` and `queue_mode` parameters.
- `EliteDriverConfig`: New configuration class for more flexible initialization.
  - Local IP is no longer required when constructing via `EliteDriver(const EliteDriverConfig& config)`.
  - Added `servoj_queue_pre_recv_size` and `servoj_queue_pre_recv_timeout` parameters.
- `PrimaryPortInterface`:
  - `getLocalIP()`: New interface for retrieving the local IP address.
  - `registerRobotExceptionCallback()`: New interface for registering robot exception callbacks.
- New robot exception interfaces added:
  - `RobotException`: Base class.
  - `RobotError`: Robot errors.
  - `RobotRuntimeException`: Runtime errors in robot scripts.
  - Changed `RtsiIOInterface` to protected inheritance.
- `DashboardClient`
  - Add `sendAndReceive()` interface.
- Complete API documentation (Markdown) added.
- Compilation guide documentation added.

### Changed
- Refactored `TcpServer` multi-port listening logic to handle these ports in a single thread:
  - Reverse port
  - Trajectory port
  - Script sender port
  - Script command port
- Optimized `PrimaryPortInterface`:
  - Switched to synchronous data reception and parsing
  - Added automatic reconnection after disconnection
- Improved RTSI module performance:
  - Reduced socket data copy operations in `RtsiClientInterface`
  - Simplified background thread loop logic in `RtsiIOInterface.hpp`
  - Changed `RtsiIOInterface::getRecipeValue()` and `RtsiIOInterface::setInputRecipeValue()` from private to public, with added explicit instantiation declarations
- Restructured project Readme documentation
- Enhance the thread safety of `external_control.script`.
- Use `boost::program_options` to parse the input parameters of the sample program.

### Fixed
- Fixed crash issue caused by dangling pointers during `EliteDriver` destruction
- Fixed the issue where the `DashboardClient::setSpeedScaling()` interface returns failure when setting a new value.

### Deprecated
- Legacy `EliteDriver` constructor is now deprecated and will be removed in future versions - migrate to `EliteDriverConfig`

### Removed
- Removed `EliteException::Code::SOCKET_OPT_CANCEL`.

## [v1.1.0] - 2024-10-30
### Initial Release
- First public version release