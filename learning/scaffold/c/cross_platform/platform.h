/* platform.h — 跨平台抽象层头文件 */

#ifndef PLATFORM_H
#define PLATFORM_H

/* ========== 平台检测 ========== */

#if defined(_WIN32) || defined(_WIN64)
  #define PLATFORM_WINDOWS 1
  #define PLATFORM_OS_NAME "Windows"
  #define PLATFORM_PATH_SEP '\\'
  #define PLATFORM_SHARED_LIB_EXT ".dll"
#elif defined(__linux__)
  #define PLATFORM_LINUX 1
  #define PLATFORM_OS_NAME "Linux"
  #define PLATFORM_PATH_SEP '/'
  #define PLATFORM_SHARED_LIB_EXT ".so"
#elif defined(__APPLE__) && defined(__MACH__)
  #define PLATFORM_MACOS 1
  #define PLATFORM_OS_NAME "macOS"
  #define PLATFORM_PATH_SEP '/'
  #define PLATFORM_SHARED_LIB_EXT ".dylib"
#else
  #error "Unsupported platform: define _WIN32, __linux__, or __APPLE__"
#endif

/* ========== C 标准兼容 ========== */

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
  /* C11 或更高 */
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
  /* C99 */
#else
  #warning "Compiler does not support C99 or later"
#endif

/* ========== 辅助宏 ========== */

#define PLATFORM_STRINGIFY(x) PLATFORM_STRINGIFY_INNER(x)
#define PLATFORM_STRINGIFY_INNER(x) #x

/* ========== 平台 API 声明 ========== */

/* 获取 CPU 核心数 */
int platform_cpu_count(void);

/* 获取系统页大小（字节） */
unsigned long platform_page_size(void);

/* 检测字节序（1=小端，0=大端） */
int is_little_endian(void);

/* 跨平台 sleep（毫秒） */
void platform_sleep_ms(int ms);

/* 动态库加载测试 */
int platform_dlopen_test(void);

#endif /* PLATFORM_H */
