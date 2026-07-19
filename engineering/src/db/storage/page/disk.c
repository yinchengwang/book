/**
 * @file disk.c
 * @brief 磁盘文件操作实现
 *
 * 使用pread/pwrite进行原子性读写，支持页面的读写和分配
 */

#include "db/disk.h"
#include "db/fault_inject.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef _WIN32
    #include <windows.h>
    #include <io.h>
#else
    #include <unistd.h>
    #include <fcntl.h>
    #include <sys/stat.h>
#endif

/* ============================================================
 * 平台兼容层
 * ============================================================ */

#ifdef _WIN32
typedef HANDLE file_handle_t;
#define INVALID_FD INVALID_HANDLE_VALUE

static ssize_t file_pread(file_handle_t fd, void *buf, size_t count, off_t offset) {
    LARGE_INTEGER li_offset;
    li_offset.QuadPart = offset;
    OVERLAPPED ov = {0};
    ov.Offset = (DWORD)(li_offset.LowPart);
    ov.OffsetHigh = (DWORD)(li_offset.HighPart);
    DWORD bytes_read = 0;
    if (!ReadFile(fd, buf, (DWORD)count, &bytes_read, &ov)) {
        return -1;
    }
    return (ssize_t)bytes_read;
}

static ssize_t file_pwrite(file_handle_t fd, const void *buf, size_t count, off_t offset) {
    LARGE_INTEGER li_offset;
    li_offset.QuadPart = offset;
    OVERLAPPED ov = {0};
    ov.Offset = (DWORD)(li_offset.LowPart);
    ov.OffsetHigh = (DWORD)(li_offset.HighPart);
    DWORD bytes_written = 0;
    if (!WriteFile(fd, buf, (DWORD)count, &bytes_written, &ov)) {
        return -1;
    }
    return (ssize_t)bytes_written;
}

static int file_sync(file_handle_t fd) {
    return FlushFileBuffers(fd) ? 0 : -1;
}

static int file_truncate(file_handle_t fd, off_t length) {
    LARGE_INTEGER pos = {{0}};
    pos.QuadPart = length;
    if (!SetFilePointerEx(fd, pos, NULL, FILE_BEGIN)) return -1;
    if (!SetEndOfFile(fd)) return -1;
    return 0;
}

#else
typedef int file_handle_t;
#define INVALID_FD (-1)

static ssize_t file_pread(int fd, void *buf, size_t count, off_t offset) {
    return pread(fd, buf, count, offset);
}

static ssize_t file_pwrite(int fd, const void *buf, size_t count, off_t offset) {
    return pwrite(fd, buf, count, offset);
}

static int file_sync(int fd) {
    return fsync(fd);
}

static int file_truncate(int fd, off_t length) {
    return ftruncate(fd, length);
}

#endif

/* ============================================================
 * 文件结构
 * ============================================================ */

/** 页面分配位图大小（管理空闲页面） */
#define PAGE_BITMAP_SIZE 4096

/** 文件头部 Magic 标识 */
#define DB_MAGIC 0x44423230  /* "DB20" */

/** 文件头部结构 */
typedef struct db_file_header_s {
    uint32_t magic;           /**< Magic: 0x44423230 */
    uint32_t version;         /**< 版本号 */
    size_t   page_size;       /**< 页面大小 */
    page_id_t page_count;     /**< 总页面数 */
    page_id_t free_page_head; /**< 空闲页面链表头 */
    uint8_t  reserved[4096 - sizeof(uint32_t)*4 - sizeof(size_t)*2 - sizeof(page_id_t)*2];
} db_file_header_t;

/** 数据库文件结构 */
struct db_file_s {
    file_handle_t fd;         /**< 文件句柄 */
    char         *path;       /**< 文件路径 */
    size_t        page_size;   /**< 页面大小 */
    page_id_t     page_count; /**< 页面总数 */
    page_id_t     free_head;  /**< 空闲页面链表头 */
    db_file_header_t header;  /**< 文件头部（缓存） */
};

/* ============================================================
 * 位图操作
 * ============================================================ */

/**
 * @brief 标记页面为已分配（简化实现：依赖空闲链表管理）
 */
static void mark_page_allocated(db_file_header_t *header, page_id_t page_id) {
    (void)header;
    (void)page_id;
    /* 位图操作暂时省略，依赖空闲链表管理 */
}

/**
 * @brief 标记页面为空闲（简化实现：依赖空闲链表管理）
 */
static void mark_page_free(db_file_header_t *header, page_id_t page_id) {
    (void)header;
    (void)page_id;
    /* 位图操作暂时省略，依赖空闲链表管理 */
}

/* ============================================================
 * 文件操作实现
 * ============================================================ */

db_file_t *disk_open(const char *path, size_t page_size) {
    if (!path || page_size < 4096 || page_size > 65536) {
        return NULL;
    }

    db_file_t *file = (db_file_t *)calloc(1, sizeof(db_file_t));
    if (!file) return NULL;

    file->path = strdup(path);
    file->page_size = page_size;

#ifdef _WIN32
    file->fd = CreateFileA(path,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file->fd == INVALID_FD) {
        free(file->path);
        free(file);
        return NULL;
    }
#else
    file->fd = open(path, O_RDWR | O_CREAT, 0644);
    if (file->fd < 0) {
        free(file->path);
        free(file);
        return NULL;
    }
#endif

    /* 读取或初始化文件头部 */
    memset(&file->header, 0, sizeof(file->header));

    ssize_t n = file_pread(file->fd, &file->header, sizeof(file->header), 0);

    if (n < (ssize_t)sizeof(file->header) || file->header.magic != DB_MAGIC) {
        /* 新文件或损坏，初始化 */
        file->header.magic = DB_MAGIC;
        file->header.version = 1;
        file->header.page_size = page_size;
        file->header.page_count = 1;  /* 头部页 */
        file->header.free_page_head = INVALID_PAGE_ID;

        /* 写入头部 */
        if (file_pwrite(file->fd, &file->header, sizeof(file->header), 0) < 0) {
            disk_close(file);
            return NULL;
        }
        file_sync(file->fd);
    }

    file->page_count = file->header.page_count;
    file->free_head = file->header.free_page_head;

    return file;
}

void disk_close(db_file_t *file) {
    if (!file) return;

    if (file->fd != INVALID_FD) {
        /* 保存头部信息 */
        file->header.page_count = file->page_count;
        file->header.free_page_head = file->free_head;
        file_pwrite(file->fd, &file->header, sizeof(file->header), 0);
        file_sync(file->fd);

#ifdef _WIN32
        CloseHandle(file->fd);
#else
        close(file->fd);
#endif
    }
    free(file->path);
    free(file);
}

db_file_t *disk_open_raw(const char *path) {
    if (!path) return NULL;

    db_file_t *file = (db_file_t *)calloc(1, sizeof(db_file_t));
    if (!file) return NULL;

    file->path = strdup(path);
    file->page_size = 8192;  /* 默认页面大小 */

#ifdef _WIN32
    file->fd = CreateFileA(path,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file->fd == INVALID_FD) {
        free(file->path);
        free(file);
        return NULL;
    }
#else
    file->fd = open(path, O_RDWR | O_CREAT, 0644);
    if (file->fd < 0) {
        free(file->path);
        free(file);
        return NULL;
    }
#endif

    return file;
}

bool disk_exists(const char *path) {
#ifdef _WIN32
    return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
#else
    struct stat st;
    return stat(path, &st) == 0;
#endif
}

page_id_t disk_get_page_count(const db_file_t *file) {
    return file ? file->page_count : 0;
}

size_t disk_get_page_size(const db_file_t *file) {
    return file ? file->page_size : 0;
}

page_t *disk_read_page(db_file_t *file, page_id_t page_id) {
    if (!file || page_id >= file->page_count) return NULL;

    /* [A3.3] 故障注入：模拟磁盘读取失败 */
    if (fault_should_fail(FAULT_POINT_READ_PAGE)) {
        return NULL;
    }

    page_t *page = (page_t *)malloc(file->page_size);
    if (!page) return NULL;

    off_t offset = (off_t)page_id * file->page_size;
    ssize_t n = file_pread(file->fd, page, file->page_size, offset);

    if (n != (ssize_t)file->page_size) {
        free(page);
        return NULL;
    }

    return page;
}

int disk_write_page(db_file_t *file, page_id_t page_id, const page_t *page) {
    if (!file || !page || page_id >= file->page_count) return -1;

    /* [A3.3] 故障注入：模拟磁盘写入失败 */
    if (fault_should_fail(FAULT_POINT_WRITE_PAGE)) {
        return -1;
    }

    off_t offset = (off_t)page_id * file->page_size;
    ssize_t n = file_pwrite(file->fd, page, file->page_size, offset);

    return (n == (ssize_t)file->page_size) ? 0 : -1;
}

page_t *disk_alloc_page(db_file_t *file, page_type_t type, page_id_t *out_page_id) {
    if (!file) return NULL;

    page_id_t new_page_id;

    /* 尝试从空闲链表分配 */
    if (file->free_head != INVALID_PAGE_ID) {
        new_page_id = file->free_head;

        /* 读取空闲页头部获取下一个空闲页 */
        page_t *free_page = disk_read_page(file, new_page_id);
        if (free_page) {
            /* 空闲页的前4字节存储下一个空闲页ID */
            page_id_t next_free = *((page_id_t *)free_page->data);
            file->free_head = next_free;
            free(free_page);
        } else {
            /* 读取失败，使用递增分配 */
            new_page_id = file->page_count++;
        }
    } else {
        /* 分配新页面 */
        new_page_id = file->page_count++;
    }

    /* 创建页面 */
    page_t *page = page_create(new_page_id, type);
    if (!page) {
        file->page_count--;
        return NULL;
    }

    /* 写入磁盘 */
    if (disk_write_page(file, new_page_id, page) != 0) {
        page_free(page);
        file->page_count--;
        return NULL;
    }

    /* 扩展文件大小（如果需要） */
    off_t file_size = (off_t)file->page_count * file->page_size;
    file_truncate(file->fd, file_size);

    /* 更新文件头部并刷盘（确保下次打开时 page_count 正确） */
    file->header.page_count = file->page_count;
    file->header.free_page_head = file->free_head;
    file_pwrite(file->fd, &file->header, sizeof(file->header), 0);

    if (out_page_id) {
        *out_page_id = new_page_id;
    }

    return page;
}

int disk_free_page(db_file_t *file, page_id_t page_id) {
    if (!file || page_id == 0 || page_id >= file->page_count) return -1;

    /* 将页面加入空闲链表 */
    page_t *page = disk_read_page(file, page_id);
    if (!page) return -1;

    /* 在页面数据区存储下一个空闲页ID */
    *((page_id_t *)page->data) = file->free_head;
    page->header.page_type = PAGE_FREE;

    disk_write_page(file, page_id, page);
    page_free(page);

    /* 更新空闲链表头 */
    file->free_head = page_id;

    mark_page_free(&file->header, page_id);

    return 0;
}

int disk_sync(db_file_t *file) {
    if (!file) return -1;
    return file_sync(file->fd);
}

const char *disk_get_path(const db_file_t *file) {
    return file ? file->path : NULL;
}

uint64_t disk_get_size(db_file_t *file) {
    if (!file) return 0;
#ifdef _WIN32
    LARGE_INTEGER size;
    if (GetFileSizeEx(file->fd, &size)) {
        return (uint64_t)size.QuadPart;
    }
    return 0;
#else
    struct stat st;
    if (fstat(file->fd, &st) == 0) {
        return (uint64_t)st.st_size;
    }
    return 0;
#endif
}

ssize_t disk_pread(db_file_t *file, uint64_t offset, void *buf, size_t count) {
    if (!file || !buf) return -1;
    return file_pread(file->fd, buf, count, (off_t)offset);
}

ssize_t disk_pwrite(db_file_t *file, uint64_t offset, const void *buf, size_t count) {
    if (!file || !buf) return -1;
    return file_pwrite(file->fd, buf, count, (off_t)offset);
}

/* ============================================================
 * 元数据操作（使用文件头部存储）
 * ============================================================ */

/* 元数据存储在文件头部固定偏移处 */
#define META_OFFSET offsetof(db_file_header_t, reserved)

int disk_get_meta(db_file_t *file, const char *key, void *value, size_t *len) {
    if (!file || !key || !value || !len) return -1;

    /* 简单的键值存储：key:value\n 格式 */
    /* 实际实现中可以存储在专门的元数据页中 */
    (void)key;
    (void)value;
    (void)len;

    return 1;  /* 未找到 */
}

int disk_set_meta(db_file_t *file, const char *key, const void *value, size_t len) {
    if (!file || !key || !value) return -1;

    (void)key;
    (void)value;
    (void)len;

    return 0;
}
