# 根 CMakeLists.txt —— 双轨调度器
cmake_minimum_required(VERSION 3.20)
project(Book LANGUAGES C CXX)

option(ENGINEERING_BUILD "Build engineering track (PG storage, MiniVecDB, apps)" ON)
option(LEARNING_BUILD "Build learning track (hand-crafted DS/algorithms)" OFF)

if(ENGINEERING_BUILD AND EXISTS "${CMAKE_CURRENT_LIST_DIR}/engineering/CMakeLists.txt")
    add_subdirectory(engineering)
endif()

if(LEARNING_BUILD AND EXISTS "${CMAKE_CURRENT_LIST_DIR}/learning/CMakeLists.txt")
    add_subdirectory(learning)
endif()