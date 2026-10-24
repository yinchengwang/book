/**
 * @file thread_local_compat.h
 * @brief 线程本地存储兼容性头文件
 *
 * 为不同平台提供线程本地存储支持。
 */
#ifndef DB_THREAD_LOCAL_COMPAT_H
#define DB_THREAD_LOCAL_COMPAT_H

#include <stdint.h>

#ifdef _WIN32
    #include <windows.h>
    
    /* Windows: 使用 __declspec(thread) */
    #define THREAD_LOCAL __declspec(thread)
    
#else
    /* POSIX: 使用 __thread (GCC) */
    #define THREAD_LOCAL __thread
    
#endif

#endif /* DB_THREAD_LOCAL_COMPAT_H */
