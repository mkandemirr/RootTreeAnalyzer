# tools/plotBeamCluster/CMakeLists.txt
project(plotBeamClusterWithCut)

add_executable(plotBeamClusterWithCut main.cpp)
target_link_libraries(plotBeamClusterWithCut PRIVATE BeamClusterTreeReader ${ROOT_LIBRARIES})


set_target_properties(plotBeamClusterWithCut PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

