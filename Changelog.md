# Changelog for Elite Robots CS SDK

## [Unreleased]

### Added
- `ControllerLog::downloadSystemLog()`: Interface for downloading robot system logs.
- `UPGRADE::upgradeControlSoftware()`: Interface for remotely upgrading robot control software.
- `EliteDriver`:
  - `writeFreedrive()`: New interface for Freedrive control.
  - `registerRobotExceptionCallback()`: New interface for registering robot exception callbacks.
  - `EliteDriver()`: Constructor updated with new `stopj_acc` parameter (acceleration for stopping motion).
- `EliteDriverConfig`: New configuration class for more flexible initialization.
  - Local IP is no longer required when constructing via `EliteDriver(const EliteDriverConfig& config)`.
- `PrimaryPortInterface`:
  - `getLocalIP()`: New interface for retrieving the local IP address.
  - `registerRobotExceptionCallback()`: New interface for registering robot exception callbacks.
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
- The automatic waypoint extrapolation functionality during servoj motion (triggered by delayed data reception) has been eliminated from `external_control.script`.

### Fixed
- Fixed crash issue caused by dangling pointers during `EliteDriver` destruction

### Deprecated
- Legacy `EliteDriver` constructor is now deprecated and will be removed in future versions - migrate to `EliteDriverConfig`

### Removed
- Removed `EliteException::Code::SOCKET_OPT_CANCEL`.

## [v1.1.0] - 2024-10-30
### Initial Release
- First public version release