# SPDX-License-Identifier: Apache-2.0
# Opt-in contract probe, separate from the importer and normal workspace build.
cmake_minimum_required(VERSION 3.25)
project(MmdPhysicsConsumerProbe LANGUAGES CXX)

find_package(physicsUsd 0.1 CONFIG REQUIRED)
add_executable(mmd_physics_consumer_probe main.cpp)
target_compile_features(mmd_physics_consumer_probe PRIVATE cxx_std_17)
target_link_libraries(mmd_physics_consumer_probe PRIVATE physicsUsd::physicsUsd)
