#include "KdlKinematicsPlugin.hpp"
#include <Elite/ClassRegisterMacro.hpp>
#include <Elite/Log.hpp>

// The robot dimension
#define AXIS_COUNT (6)

namespace ELITE {

KdlKinematicsPlugin::KdlKinematicsPlugin() {}

KdlKinematicsPlugin::~KdlKinematicsPlugin() {}

void KdlKinematicsPlugin::setMDH(const vector6d_t& alpha, const vector6d_t& a, const vector6d_t& d) {
    std::lock_guard<std::mutex> lock(mutex_);

    dh_alpha_ = alpha;
    dh_a_ = a;
    dh_d_ = d;

    ik_solver_.reset();
    fk_solver_.reset();
    robot_chain_.reset();

    robot_chain_ = std::make_unique<KDL::Chain>();

    for (size_t i = 0; i < AXIS_COUNT; ++i) {
        // MDH constant transform: Rx(alpha) * Tx(a) * Tz(d)
        KDL::Frame frame = KDL::Frame(KDL::Rotation::RotX(dh_alpha_[i])) * KDL::Frame(KDL::Vector(dh_a_[i], 0.0, 0.0)) *
                           KDL::Frame(KDL::Vector(0.0, 0.0, dh_d_[i]));

        // Joint: RotZ(theta_i)
        KDL::Joint joint("Joint" + std::to_string(i), KDL::Joint::RotZ);

        robot_chain_->addSegment(KDL::Segment("Link" + std::to_string(i), joint, frame));
    }

    ik_solver_ = std::make_unique<KDL::ChainIkSolverPos_LMA>(*robot_chain_, 1e-10, 500);

    fk_solver_ = std::make_unique<KDL::ChainFkSolverPos_recursive>(*robot_chain_);
}

bool KdlKinematicsPlugin::getPositionFK(const vector6d_t& joint_angles, vector6d_t& poses) {
    if (!fk_solver_) {
        ELITE_LOG_ERROR("Please set Kinematics config first by setMDH()\n");
        return false;
    }
    KDL::JntArray kdl_joints = convertToKDLJoints(joint_angles);
    KDL::Frame result_frame;
    int ret = fk_solver_->JntToCart(kdl_joints, result_frame);
    if (ret < 0) {
        ELITE_LOG_ERROR("KDL FK solver failed, error code: %d\n", ret);
        return false;
    }

    poses[0] = result_frame.p.x();
    poses[1] = result_frame.p.y();
    poses[2] = result_frame.p.z();

    result_frame.M.GetRPY(poses[3], poses[4], poses[5]);

    return true;
}

bool KdlKinematicsPlugin::getPositionIK(const vector6d_t& pose, const vector6d_t& near, vector6d_t& solution,
                                        KinematicsResult& result) {
    if (!ik_solver_) {
        ELITE_LOG_ERROR("Please set Kinematics config first by setMDH()\n");
        result.kinematic_error = KinematicError::SOLVER_NOT_ACTIVE;
        return false;
    }

    // near → KDL JntArray (seed)
    KDL::JntArray kdl_joints_near = convertToKDLJoints(near);

    // pose(xyz + RPY) → KDL::Frame
    const double x = pose[0];
    const double y = pose[1];
    const double z = pose[2];
    const double roll = pose[3];
    const double pitch = pose[4];
    const double yaw = pose[5];

    KDL::Rotation rot = KDL::Rotation::RPY(roll, pitch, yaw);
    KDL::Vector pos(x, y, z);
    KDL::Frame frame(rot, pos);

    KDL::JntArray ret_joints(AXIS_COUNT);
    int ret = ik_solver_->CartToJnt(kdl_joints_near, frame, ret_joints);

    if (ret != KDL::SolverI::E_NOERROR) {
        ELITE_LOG_WARN("KDL IK solver failed, error code: %d\n", ret);
        result.kinematic_error = KinematicError::NO_SOLUTION;
        return false;
    }

    for (size_t i = 0; i < AXIS_COUNT; ++i) {
        solution[i] = ret_joints(i);
    }

    result.kinematic_error = KinematicError::OK;
    return true;
}

bool KdlKinematicsPlugin::getPositionIK(const vector6d_t& pose, const vector6d_t& near, std::vector<vector6d_t>& solutions,
                                        KinematicsResult& result) {
    solutions.resize(1);
    return getPositionIK(pose, near, solutions[0], result);
}

KDL::JntArray KdlKinematicsPlugin::convertToKDLJoints(const ELITE::vector6d_t& joints) {
    KDL::JntArray kdl_joints(AXIS_COUNT);
    for (int i = 0; i < AXIS_COUNT; i++) {
        kdl_joints(i) = (i < static_cast<int>(joints.size())) ? joints[i] : 0.0;
    }
    return kdl_joints;
}
}  // namespace ELITE


