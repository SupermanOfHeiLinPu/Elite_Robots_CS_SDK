// SPDX-License-Identifier: MIT
// Copyright (c) 2025, Elite Robots.
#include "TrajectoryInterface.hpp"
#include <boost/asio.hpp>
#include "ControlCommon.hpp"
#include "EliteException.hpp"
#include "Log.hpp"

using namespace ELITE;

TrajectoryInterface::TrajectoryInterface(int port, std::shared_ptr<TcpServer::StaticResource> resource_)
    : ReversePort(port, sizeof(TrajectoryMotionResult), resource_) {
    server_->setReceiveCallback([&](const uint8_t data[], int nb) {
        if (nb != sizeof(TrajectoryMotionResult)) {
            return;
        }
        int32_t receive_value = *((const int32_t*)data);
        receive_value = ::ntohl(receive_value);
        TrajectoryMotionResult motion_result = static_cast<TrajectoryMotionResult>(receive_value);
        if (motion_result_func_) {
            motion_result_func_(motion_result);
        }
    });
    server_->startListen();
}

TrajectoryInterface::~TrajectoryInterface() {}

bool TrajectoryInterface::writeTrajectoryPoint(const vector6d_t& positions, float time, float blend_radius, bool cartesian) {
    int32_t buffer[TRAJECTORY_MESSAGE_LEN] = {0};    
    for (size_t i = 0; i < 6; i++) {
        int32_t rounded_pos = static_cast<int32_t>(::round(positions[i] * CONTROL::POS_ZOOM_RATIO));
        buffer[i] = ::htonl(rounded_pos);
    }
    buffer[18] = htonl(round(time * CONTROL::TIME_ZOOM_RATIO));
    buffer[19] = htonl(round(blend_radius * CONTROL::POS_ZOOM_RATIO));
    if (cartesian) {
        buffer[20] = htonl((int)TrajectoryMotionType::CARTESIAN);
    } else {
        buffer[20] = htonl((int)TrajectoryMotionType::JOINT);
    }

    return write(buffer, sizeof(buffer)) > 0;
}