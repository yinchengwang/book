# pthread scaffold

POSIX 线程 + mutex + cond 三件套最小演示。2 worker 并发累加同一计数器，验证 mutex 原子性。

## 复现命令

需要 gcc 与 pthread 库（MinGW-w64 / Linux / macOS 均可）。

```bash
cd learning/scaffold/pthread
gcc -Wall -Wextra -O2 -std=c11 -o pthread_demo main.c -lpthread
./pthread_demo
```

或 `make run`（需要 make）。

## 预期输出

```
worker 0 done
worker 1 done
counter = 200000 (expected 200000)
OK
```

如出现 `counter = <expected>` 但 worker 完成数 <2 或 counter != 200000，说明竞态未消除。

## 关键点

- `pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;` 静态初始化
- `pthread_create` 返回 0 成功；错误码在 `errno.h` 之外
- `pthread_join` 必须等待，否则主线程退出时子线程被强杀
- 锁粒度：每次 `lock / unlock` 包裹单个 `g_counter++`，过粗影响并发，过细失去保护

## 工程对照（核心）

详见 NOTES.md "工程对照" 段。
