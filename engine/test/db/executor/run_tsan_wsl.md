# WSL2 下 ThreadSanitizer 验证（gap04）

MinGW 不支持 TSan，须在 WSL2 Ubuntu 构建。前置参照记忆 engineering-linux-build-knowledge：
NTFS 下 configure_file 需防护、显式 time/stddef/errno include、mkdir(path,0755) 调用点改写。

```bash
# WSL2 Ubuntu 内：
cd /mnt/d/code/book/engineering
cmake -B build-tsan -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_C_FLAGS="-fsanitize=thread -g" \
  -DCMAKE_CXX_FLAGS="-fsanitize=thread -g" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=thread"
ninja -C build-tsan px_queue_test px_scheduler_test exchange_exec_test hashjoin_px_test
for t in px_queue_test px_scheduler_test exchange_exec_test hashjoin_px_test; do
  ./build-tsan/test/db/executor/$t || exit 1
done
echo "TSAN CLEAN"
```

验收：`TSAN CLEAN` 且输出无任何 "WARNING: ThreadSanitizer"。
