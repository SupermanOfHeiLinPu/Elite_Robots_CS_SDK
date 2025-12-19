# TODO: Modularization.
add_library(
    elite_kdl_kin_plugin
    SHARED
    KdlKinematicsPlugin/source/KdlKinematicsPlugin.cpp
)
target_include_directories(
    elite_kdl_kin_plugin
    PRIVATE
    KdlKinematicsPlugin/include
)
target_link_libraries(
    elite_kdl_kin_plugin
    PRIVATE
    elite-cs-series-sdk::shared
)