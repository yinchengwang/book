#!/usr/bin/env bash
# test_profiles.sh — MMDB_MODALITY profile 机制的配置期负向/正向测试
# 用法: bash scripts/test_profiles.sh   （在 engine/ 目录下执行）

set -u
cd "$(dirname "$0")/.."

expect_fail() {
  rm -rf build/probe.$$ build/probe.log
  if cmake -B build/probe.$$ -S . -G Ninja "$@" >build/probe.log 2>&1; then
    echo "FAIL: 本应报错却配置成功: $*"
    tail -5 build/probe.log
    exit 1
  fi
  if ! grep -q "CMake Error" build/probe.log; then
    echo "FAIL: 配置失败但不是 CMake Error（可能是别的工具炸了）: $*"
    tail -5 build/probe.log
    exit 1
  fi
  rm -rf build/probe.$$ build/probe.log
}

expect_ok() {
  if ! cmake -B "build/p_$1" -S . -G Ninja -DMMDB_MODALITY="$1" >"build/p_$1.log" 2>&1; then
    echo "FAIL: profile $1 配置失败"
    tail -8 "build/p_$1.log"
    exit 1
  fi
  rm -rf "build/p_$1" "build/p_$1.log"
}

# ---- 负向：非法组合必须 FATAL_ERROR ----
expect_fail -DMMDB_MODALITY=kv -DMMDB_ENABLE_VECTOR=ON          # 手动开关未放行
expect_fail -DMMDB_MODALITY=nonexistent                          # 未知 profile
expect_fail -DMMDB_MODALITY=graph -DMMDB_ENABLE_KV=OFF           # 手动开关未放行
# 闭包校验（放行手动开关后仍须拒绝不封闭组合）
expect_fail -DMMDB_MODALITY=graph -DMMDB_ENABLE_KV=OFF -DMMDB_ALLOW_MANUAL_SWITCHES=ON
expect_fail -DMMDB_MODALITY=sparse -DMMDB_ENABLE_VECTOR=OFF -DMMDB_ALLOW_MANUAL_SWITCHES=ON
echo "OK: 5 个非法组合都被拒绝"

# ---- 幂等：同一目录重复配置同一 profile 必须成功 ----
rm -rf build/probe.$$ build/probe.log
cmake -B build/probe.$$ -S . -G Ninja -DMMDB_MODALITY=kv >build/probe.log 2>&1 || {
  echo "FAIL: 首次配置 kv 失败"; tail -5 build/probe.log; exit 1; }
cmake -B build/probe.$$ -S . -G Ninja -DMMDB_MODALITY=kv >>build/probe.log 2>&1 || {
  echo "FAIL: 重复配置 kv 应幂等成功"; tail -5 build/probe.log; exit 1; }
rm -rf build/probe.$$ build/probe.log
echo "OK: 同 profile 重复配置幂等"

# ---- 换 profile 必须拒绝 ----
rm -rf build/probe.$$ build/probe.log
cmake -B build/probe.$$ -S . -G Ninja -DMMDB_MODALITY=kv >build/probe.log 2>&1 || {
  echo "FAIL: 配置 kv 失败"; exit 1; }
if cmake -B build/probe.$$ -S . -G Ninja -DMMDB_MODALITY=vector >>build/probe.log 2>&1; then
  echo "FAIL: 同一构建树换 profile 本应拒绝"; exit 1
fi
rm -rf build/probe.$$ build/probe.log
echo "OK: 换 profile 要求换构建目录"

# ---- 正向：全部 profile 都能配置 ----
for p in kv vector graph spatial tree sparse doc ts stream columnar st rdf relational legacy full; do
  expect_ok "$p"
done
echo "OK: 全部 15 个 profile 可配置"
