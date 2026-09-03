# WASM 胶水层 - 空占位 CMakeLists
# Emscripten 构建不通过 CMake 调度，由 build-games-wasm.sh 直接调用 emcc
# 本文件仅用于 IDE 语法解析和路径索引

cmake_minimum_required(VERSION 3.20)
project(GamesWASM)

# 警告：不要通过 CMake 构建此目录
# 使用 bash engineering/scripts/build-games-wasm.sh 替代
