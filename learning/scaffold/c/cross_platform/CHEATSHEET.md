# 跨平台 C 编程速查表

## 平台检测宏

```c
#ifdef _WIN32         // Windows (32 or 64 bit)
#ifdef _WIN64         // Windows 64-bit only
#ifdef __linux__      // Linux (including Android)
#ifdef __APPLE__      // Apple (macOS + iOS)
#ifdef __MACH__       // Mach kernel (macOS)
#ifdef __FreeBSD__    // FreeBSD
#ifdef __ANDROID__    // Android
```

## 类型大小差异

| 类型 | Win32 | Win64 | Linux32 | Linux64 | macOS64 |
|------|-------|-------|---------|---------|---------|
| `int` | 4 | 4 | 4 | 4 | 4 |
| `long` | 4 | 4 | 4 | **8** | **8** |
| `long long` | 8 | 8 | 8 | 8 | 8 |
| `size_t` | 4 | 8 | 4 | 8 | 8 |
| `void*` | 4 | 8 | 4 | 8 | 8 |

> **关键差异**：`long` 在 Windows 是 4 字节，在 Linux/macOS 64-bit 是 8 字节。  
> 跨平台代码用 `int32_t`/`int64_t`（`<stdint.h>`）代替 `long`。

## 路径分隔符

```c
#ifdef _WIN32
  #define PATH_SEP '\\'
  #define PATH_SEP_STR "\\"
#else
  #define PATH_SEP '/'
  #define PATH_SEP_STR "/"
#endif
```

## 动态库加载

```c
/* Windows */
#include <windows.h>
HMODULE h = LoadLibraryA("mylib.dll");
void *func = (void*)GetProcAddress(h, "my_function");
FreeLibrary(h);

/* Linux/macOS (POSIX) */
#include <dlfcn.h>
void *h = dlopen("libmylib.so", RTLD_NOW);
void *func = dlsym(h, "my_function");
dlclose(h);
```

## 线程 API

```c
/* C11 标准（推荐——全平台统一） */
#include <threads.h>
thrd_t t;
thrd_create(&t, my_func, arg);
thrd_join(t, NULL);

/* POSIX */
#include <pthread.h>
pthread_t t;
pthread_create(&t, NULL, my_func, arg);
pthread_join(t, NULL);

/* Windows */
#include <windows.h>
HANDLE t = CreateThread(NULL, 0, my_func, arg, 0, NULL);
WaitForSingleObject(t, INFINITE);
```

## 字节序

```c
/* 运行时检测（最可靠） */
int is_little_endian(void) {
    unsigned int x = 1;
    return (int)(*(unsigned char*)&x);
}

/* 编译期检测 */
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
  /* Little Endian (x86, ARM) */
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
  /* Big Endian (SPARC, PPC) */
#endif
```

## 文件 API

```c
/* C 标准（跨平台） */
FILE *f = fopen("file.txt", "r");   // 全平台统一

/* Windows 专有 */
HANDLE h = CreateFileA("file.txt", GENERIC_READ, ...);  // 异步 I/O

/* POSIX 专有 */
int fd = open("file.txt", O_RDONLY);  // fd-based 操作
```
