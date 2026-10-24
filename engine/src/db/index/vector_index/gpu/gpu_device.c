/**
 * @file gpu_device.c
 * @brief GPU 设备管理实现
 *
 * GPU 设备发现、选择、资源管理的存根实现。
 * 实际 CUDA/OpenCL 实现待后续集成。
 */
#include "gpu_vector_index.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ========================================================================
 * 内部状态
 * ======================================================================== */

static bool g_gpu_initialized = false;
static int g_selected_device = -1;
static gpu_device_list_t *g_device_list = NULL;

/* ========================================================================
 * 设备信息初始化（存根实现）
 * ======================================================================== */

static void init_stub_device_info(gpu_device_info_t *info, int id, gpu_backend_t backend)
{
    info->device_id = id;
    info->backend = backend;

    /* 存根设备信息 */
    if (backend == GPU_BACKEND_CUDA) {
        snprintf(info->name, sizeof(info->name), "NVIDIA GPU #%d (Stub)", id);
        info->total_memory = 8UL * 1024 * 1024 * 1024;  /* 8 GB */
        info->free_memory = 6UL * 1024 * 1024 * 1024;   /* 6 GB */
        info->compute_units = 40;
        info->max_threads_per_block = 1024;
        info->max_grid_dim[0] = 65535;
        info->max_grid_dim[1] = 65535;
        info->max_grid_dim[2] = 65535;
        info->max_block_dim[0] = 1024;
        info->max_block_dim[1] = 1024;
        info->max_block_dim[2] = 64;
    } else if (backend == GPU_BACKEND_OPENCL) {
        snprintf(info->name, sizeof(info->name), "OpenCL Device #%d (Stub)", id);
        info->total_memory = 4UL * 1024 * 1024 * 1024;  /* 4 GB */
        info->free_memory = 3UL * 1024 * 1024 * 1024;   /* 3 GB */
        info->compute_units = 32;
        info->max_threads_per_block = 256;
        info->max_grid_dim[0] = 65535;
        info->max_grid_dim[1] = 65535;
        info->max_grid_dim[2] = 65535;
        info->max_block_dim[0] = 256;
        info->max_block_dim[1] = 256;
        info->max_block_dim[2] = 64;
    } else {
        snprintf(info->name, sizeof(info->name), "Unknown Device #%d", id);
        info->total_memory = 0;
        info->free_memory = 0;
        info->compute_units = 0;
        info->max_threads_per_block = 0;
    }
}

/* ========================================================================
 * GPU 设备管理 API 实现
 * ======================================================================== */

int gpu_init(void)
{
    if (g_gpu_initialized) {
        return 0;
    }

    /* TODO: 实际 CUDA/OpenCL 初始化 */
    /* cudaInit() 或 clGetPlatformIDs() */

    g_gpu_initialized = true;
    g_selected_device = -1;

    printf("[GPU-STUB] GPU 子系统已初始化（存根模式）\n");
    return 0;
}

int gpu_shutdown(void)
{
    if (!g_gpu_initialized) {
        return 0;
    }

    /* 释放设备列表 */
    if (g_device_list != NULL) {
        gpu_free_device_list(g_device_list);
        g_device_list = NULL;
    }

    /* TODO: 实际 CUDA/OpenCL 清理 */
    /* cudaDeviceReset() 或 clReleaseContext() */

    g_gpu_initialized = false;
    g_selected_device = -1;

    printf("[GPU-STUB] GPU 子系统已关闭\n");
    return 0;
}

gpu_device_list_t *gpu_get_device_list(gpu_backend_t backend)
{
    gpu_device_list_t *list;
    gpu_backend_t detected_backend;
    int num_devices;

    /* 确定后端类型 */
    if (backend == GPU_BACKEND_NONE) {
        /* TODO: 自动检测可用后端 */
        /* 检查 CUDA first, 然后 OpenCL */
        detected_backend = GPU_BACKEND_CUDA;  /* 存根：默认 CUDA */
    } else {
        detected_backend = backend;
    }

    /* TODO: 实际设备枚举 */
    /* if (detected_backend == GPU_BACKEND_CUDA) { cudaGetDeviceCount(&num_devices); } */
    /* else if (detected_backend == GPU_BACKEND_OPENCL) { clGetDeviceIDs(...); } */

    /* 存根：返回 1 个虚拟设备 */
    num_devices = 1;

    /* 分配设备列表 */
    list = (gpu_device_list_t *)malloc(sizeof(gpu_device_list_t));
    if (list == NULL) {
        return NULL;
    }

    list->devices = (gpu_device_info_t *)malloc(num_devices * sizeof(gpu_device_info_t));
    if (list->devices == NULL) {
        free(list);
        return NULL;
    }

    list->count = num_devices;
    list->selected_device = 0;

    /* 初始化设备信息 */
    for (int i = 0; i < num_devices; i++) {
        init_stub_device_info(&list->devices[i], i, detected_backend);
    }

    printf("[GPU-STUB] 发现 %d 个 GPU 设备 (backend=%d)\n", num_devices, detected_backend);
    return list;
}

void gpu_free_device_list(gpu_device_list_t *list)
{
    if (list == NULL) {
        return;
    }

    if (list->devices != NULL) {
        free(list->devices);
    }

    free(list);
}

int gpu_select_device(int device_id)
{
    /* TODO: 实际设备选择 */
    /* cudaSetDevice(device_id) 或 clSetDefaultDevice() */

    g_selected_device = device_id;
    printf("[GPU-STUB] 已选择设备 %d\n", device_id);
    return 0;
}

const gpu_device_info_t *gpu_get_current_device(void)
{
    if (g_device_list == NULL || g_selected_device < 0 ||
        g_selected_device >= g_device_list->count) {
        return NULL;
    }

    return &g_device_list->devices[g_selected_device];
}

void gpu_sync(void)
{
    /* TODO: 实际同步 */
    /* cudaDeviceSynchronize() 或 clFinish() */
}
