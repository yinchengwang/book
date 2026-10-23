# 根 CMakeLists.txt —— 双轨调度器
# learning/ 已切出为独立仓（yinchengwang/learning），本仓不再构建学习轨道，
# 故移除 LEARNING_BUILD option 与 add_subdirectory(learning) 分支。
cmake_minimum_required(VERSION 3.20)
project(Book LANGUAGES C CXX)

option(ENGINEERING_BUILD "Build engineering track (PG storage, MiniVecDB)" ON)

if(ENGINEERING_BUILD AND EXISTS "${CMAKE_CURRENT_LIST_DIR}/engineering/CMakeLists.txt")
    add_subdirectory(engineering)
endif()