# Changelog for Elite Robots CS SDK

## [Unreleased]

### Added
- `ControllerLog::downloadSystemLog()`: Added interface for downloading robot system logs
- `UPGRADE::upgradeControlSoftware()`: Added interface for remote control software upgrades
- `EliteDriver::writeFreedrive()`: New Freedrive control interface
- `EliteDriverConfig`: New configuration class for more flexible initialization
  - Local IP is no longer mandatory when using `EliteDriver(const EliteDriverConfig& config)`
- `PrimaryPortInterface::getLocalIP()`: Added interface to retrieve local IP
- Added comprehensive API documentation (Markdown format)
- Added compilation guide documentation
- New configurations added to the `EliteDriver` constructor:
  - `stopj_acc` parameter.
  - `servoj_pre_recv_size` parameter.

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
- The `external_control.script` has been updated to remove the function that automatically infers the next position when data isn't provided in time during servoj motion. Instead, it now initiates movement only after pre-storing a specified number of positions.

### Fixed
- Fixed crash issue caused by dangling pointers during `EliteDriver` destruction

### Deprecated
- Legacy `EliteDriver` constructor is now deprecated and will be removed in future versions - migrate to `EliteDriverConfig`

### Removed
- Removed `EliteException::Code::SOCKET_OPT_CANCEL`.

## [v1.1.0] - 2024-10-30
### Initial Release
- First public version release