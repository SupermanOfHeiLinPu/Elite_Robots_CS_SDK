#include "KdlKinematicsPlugin.hpp"

namespace ELITE {

KdlKinematicsPlugin::KdlKinematicsPlugin() {}

KdlKinematicsPlugin::~KdlKinematicsPlugin() {}

void KdlKinematicsPlugin::setMDH(const vector6d_t& alpha, const vector6d_t& a, const vector6d_t& d) {
    dh_alpha_ = alpha;
    dh_a_ = a;
    dh_d_ = d;
}

bool KdlKinematicsPlugin::getPositionFK(const vector6d_t& joint_angles, vector6d_t& poses) {
    
}

bool KdlKinematicsPlugin::getPositionIK(const vector6d_t& pose, const vector6d_t& near, vector6d_t& solution,
                                        KinematicsResult& result) {}

bool KdlKinematicsPlugin::getPositionIK(const vector6d_t& pose, const vector6d_t& near, std::vector<vector6d_t>& solutions,
                                        KinematicsResult& result) {}

}  // namespace ELITE
