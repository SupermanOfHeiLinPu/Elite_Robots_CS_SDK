#include <future>

#include "ScriptCommandInterface.hpp"
#include "ControlCommon.hpp"
#include "Log.hpp"

namespace ELITE {

ScriptCommandInterface::ScriptCommandInterface(int port, std::shared_ptr<TcpServer::StaticResource> resource)
    : ReversePort(port, 4, resource) {
    server_->startListen();
}

ScriptCommandInterface::~ScriptCommandInterface() {}

bool ScriptCommandInterface::zeroFTSensor() {
    int32_t buffer[SCRIPT_COMMAND_DATA_SIZE] = {0};
    buffer[0] = htonl(static_cast<int32_t>(Cmd::ZERO_FTSENSOR));
    return write(buffer, sizeof(buffer)) > 0;
}

bool ScriptCommandInterface::setPayload(double mass, const vector3d_t& cog) {
    int32_t buffer[SCRIPT_COMMAND_DATA_SIZE] = {0};
    buffer[0] = htonl(static_cast<int32_t>(Cmd::SET_PAYLOAD));
    buffer[1] = htonl(static_cast<int32_t>((mass * CONTROL::COMMON_ZOOM_RATIO)));
    buffer[2] = htonl(static_cast<int32_t>((cog[0] * CONTROL::COMMON_ZOOM_RATIO)));
    buffer[3] = htonl(static_cast<int32_t>((cog[1] * CONTROL::COMMON_ZOOM_RATIO)));
    buffer[4] = htonl(static_cast<int32_t>((cog[2] * CONTROL::COMMON_ZOOM_RATIO)));
    return write(buffer, sizeof(buffer)) > 0;
}

bool ScriptCommandInterface::setToolVoltage(const ToolVoltage& vol) {
    int32_t buffer[SCRIPT_COMMAND_DATA_SIZE] = {0};
    buffer[0] = htonl(static_cast<int32_t>(Cmd::SET_TOOL_VOLTAGE));
    buffer[1] = htonl(static_cast<int32_t>(vol) * CONTROL::COMMON_ZOOM_RATIO);
    return write(buffer, sizeof(buffer)) > 0;
}

bool ScriptCommandInterface::startForceMode(const vector6d_t& task_frame, const vector6int32_t& selection_vector,
                                            const vector6d_t& wrench, const ForceMode& mode, const vector6d_t& limits) {
    int32_t buffer[SCRIPT_COMMAND_DATA_SIZE] = {0};
    buffer[0] = htonl(static_cast<int32_t>(Cmd::START_FORCE_MODE));
    int32_t* bp = &buffer[1];
    for (auto& tf : task_frame) {
        *bp = htonl(static_cast<int32_t>((tf * CONTROL::COMMON_ZOOM_RATIO)));
        bp++;
    }
    for (auto& sv : selection_vector) {
        *bp = htonl(sv);
        bp++;
    }
    for (auto& wr : wrench) {
        *bp = htonl(static_cast<int32_t>((wr * CONTROL::COMMON_ZOOM_RATIO)));
        bp++;
    }
    *bp = htonl(static_cast<int32_t>(mode));
    bp++;
    for (auto& li : limits) {
        *bp = htonl(static_cast<int32_t>((li * CONTROL::COMMON_ZOOM_RATIO)));
        bp++;
    }
    return write(buffer, sizeof(buffer)) > 0;
}

bool ScriptCommandInterface::endForceMode() {
    int32_t buffer[SCRIPT_COMMAND_DATA_SIZE] = {0};
    buffer[0] = htonl(static_cast<int32_t>(Cmd::END_FORCE_MODE));
    return write(buffer, sizeof(buffer)) > 0;
}

bool ScriptCommandInterface::startToolRs485(const SerialConfig& config, int tcp_port) {
    int32_t buffer[SCRIPT_COMMAND_DATA_SIZE] = {0};
    buffer[0] = htonl(static_cast<int32_t>(Cmd::START_TOOL_COMMUNICATION));
    buffer[1] = htonl(static_cast<int32_t>(config.baud_rate));
    buffer[2] = htonl(static_cast<int32_t>(config.parity));
    buffer[3] = htonl(static_cast<int32_t>(config.stop_bits));
    buffer[4] = htonl(tcp_port);
    if(write(buffer, sizeof(buffer)) <= 0) {
        return false;
    }
    return waitForSerialResult(SerialResult::START, 5000);
}

bool ScriptCommandInterface::endToolRs485() {
    int32_t buffer[SCRIPT_COMMAND_DATA_SIZE] = {0};
    buffer[0] = htonl(static_cast<int32_t>(Cmd::END_TOOL_COMMUNICATION));
    if (write(buffer, sizeof(buffer)) <= 0) {
        return false;
    }
    return waitForSerialResult(SerialResult::END, 5000);
}

bool ScriptCommandInterface::startBoardRs485(const SerialConfig& config, int tcp_port) {
    int32_t buffer[SCRIPT_COMMAND_DATA_SIZE] = {0};
    buffer[0] = htonl(static_cast<int32_t>(Cmd::START_BOARD_RS485));
    buffer[1] = htonl(static_cast<int32_t>(config.baud_rate));
    buffer[2] = htonl(static_cast<int32_t>(config.parity));
    buffer[3] = htonl(static_cast<int32_t>(config.stop_bits));
    buffer[4] = htonl(tcp_port);
    if(write(buffer, sizeof(buffer)) <= 0) {
        return false;
    }
    return waitForSerialResult(SerialResult::START, 5000);
}

bool ScriptCommandInterface::endBoardRs485() {
    int32_t buffer[SCRIPT_COMMAND_DATA_SIZE] = {0};
    buffer[0] = htonl(static_cast<int32_t>(Cmd::END_BOARD_RS485));
    if (write(buffer, sizeof(buffer)) <= 0) {
        return false;
    }
    return waitForSerialResult(SerialResult::END, 5000);
}

bool ScriptCommandInterface::waitForSerialResult(SerialResult expected_result, int timeout_ms) {
    std::promise<SerialResult> result_promise;
    server_->setReceiveCallback([&](const uint8_t data[], int size) {
        SerialResult result = (SerialResult)htonl(*((const uint32_t*)data));
        result_promise.set_value(result);
    });
    auto result_future = result_promise.get_future();
    auto status = result_future.wait_for(std::chrono::milliseconds(timeout_ms));
    if (status == std::future_status::ready) {
        SerialResult result = result_future.get();
        server_->unsetReceiveCallback();
        if (result == expected_result) {
            return true;
        } else {
            ELITE_LOG_ERROR("Serial command failed, result: %d", static_cast<int>(result));
            return false;
        }
    } else {
        server_->unsetReceiveCallback();
        ELITE_LOG_ERROR("Serial command timeout");
        return false;
    }
}

}  // namespace ELITE