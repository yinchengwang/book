/* platform.c — 平台特定实现 */

#include "platform.h"

#ifdef PLATFORM_WINDOWS
  #ifndef _WIN32_WINNT
    #define _WIN32_WINNT 0x0600  /* Windows Vista+ */
  #endif
  #include <windows.h>
#elif defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS)
  #include <unistd.h>
  #include <dlfcn.h>
  #ifdef PLATFORM_LINUX
    #include <sys/sysinfo.h>
  #endif
#endif

#include <stdio.h>

/* ========== CPU 核心数 ========== */

int platform_cpu_count(void) {
#ifdef PLATFORM_WINDOWS
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return (int)si.dwNumberOfProcessors;
#elif defined(PLATFORM_LINUX)
    return (int)sysconf(_SC_NPROCESSORS_ONLN);
#elif defined(PLATFORM_MACOS)
    return (int)sysconf(_SC_NPROCESSORS_ONLN);
#else
    return -1;
#endif
}

/* ========== 页大小 ========== */

unsigned long platform_page_size(void) {
#ifdef PLATFORM_WINDOWS
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return (unsigned long)si.dwPageSize;
#elif defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS)
    long sz = sysconf(_SC_PAGESIZE);
    return (unsigned long)(sz > 0 ? sz : 4096);
#else
    return 4096;
#endif
}

/* ========== 字节序检测（运行时） ========== */

int is_little_endian(void) {
    unsigned int x = 1;
    return (int)(*(unsigned char *)&x);
}

/* ========== 跨平台 sleep ========== */

void platform_sleep_ms(int ms) {
#ifdef PLATFORM_WINDOWS
    Sleep((DWORD)ms);
#elif defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS)
    /* usleep 已被 POSIX 标记为过时，用 nanosleep 替代 */
    struct timespec ts;
    ts.tv_sec  = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
#else
    /* fallback: busy-wait（不推荐但普适） */
    (void)ms;
#endif
}

/* ========== 动态库加载测试 ========== */

int platform_dlopen_test(void) {
#ifdef PLATFORM_WINDOWS
    HMODULE h = LoadLibraryA("kernel32.dll");
    if (h) {
        FreeLibrary(h);
        return 1;
    }
    return 0;
#elif defined(PLATFORM_LINUX)
    void *h = dlopen("libc.so.6", RTLD_NOW);
    if (h) { dlclose(h); return 1; }
    return 0;
#elif defined(PLATFORM_MACOS)
    void *h = dlopen("libc.dylib", RTLD_NOW);
    if (h) { dlclose(h); return 1; }
    return 0;
#else
    return 0;
#endif
}
