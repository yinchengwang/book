/**
 * @file wal_fuzzer.cpp
 * @brief WAL 记录解析模糊测试
 *
 * 生成随机 WAL 数据并调用 wal_redo() 解析，验证不崩溃。
 * 随机字节可能被解析为各种日志记录类型，测试解析器的健壮性。
 */
#include <gtest/gtest.h>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <vector>

extern "C" {
#include "db/wal.h"
#include "db/disk.h"
#include "db/wal_buf.h"
}

/**
 * @brief WAL 模糊测试夹具
 */
class WALFuzzTest : public ::testing::Test {
protected:
    char wal_path[512];

    void SetUp() override {
        /* 使用固定种子保证可重复 */
        srand(42);

        /* 在临时目录中创建 WAL 文件 */
        snprintf(wal_path, sizeof(wal_path), "./test-results/fuzz_wal_%d.wal", rand());
    }

    void TearDown() override {
        /* 清理测试文件 */
        remove(wal_path);
    }

    /**
     * @brief 写入随机 WAL 数据到文件
     */
    void write_random_wal_data(const uint8_t *data, size_t size) {
        FILE *f = fopen(wal_path, "wb");
        if (!f) return;

        /* 写入 WAL 头部（固定格式确保 wal_redo 能打开） */
        uint8_t header[32] = {0};
        memcpy(header, "WAL", 3);      /* magic */
        header[3] = 1;                  /* version */
        fwrite(header, 1, sizeof(header), f);

        /* 写入随机数据作为日志记录 */
        if (data && size > 0) {
            fwrite(data, 1, size, f);
        }

        fclose(f);
    }

    /**
     * @brief 生成随机 WAL 记录数据
     */
    std::vector<uint8_t> generate_random_wal_records(size_t num_records) {
        std::vector<uint8_t> data;

        for (size_t i = 0; i < num_records; i++) {
            /* 记录头部：type(1) + txid(4) + lsn(8) + size(3) + checksum(2) = 18 字节 */
            uint8_t header[WAL_RECORD_HEADER_SIZE];
            for (size_t j = 0; j < sizeof(header); j++) {
                header[j] = (uint8_t)(rand() & 0xFF);
            }
            data.insert(data.end(), header, header + sizeof(header));

            /* 随机数据部分 */
            size_t data_size = rand() % 256;
            for (size_t j = 0; j < data_size; j++) {
                data.push_back((uint8_t)(rand() & 0xFF));
            }
        }

        return data;
    }
};

/**
 * @brief 应用回调（只验证不崩溃，不修改数据）
 */
static int fuzz_wal_apply(void *ctx, wal_log_type_t type,
                          const void *key, size_t key_len,
                          const void *value, size_t value_len) {
    (void)ctx;
    (void)type;
    (void)key;
    (void)key_len;
    (void)value;
    (void)value_len;
    return 0;
}

/**
 * @brief 随机 WAL 数据，验证不崩溃
 */
TEST_F(WALFuzzTest, RandomWALDataNoCrash) {
    for (int i = 0; i < 1000; i++) {
        /* 生成随机 WAL 记录 */
        auto data = generate_random_wal_records(rand() % 10 + 1);
        write_random_wal_data(data.data(), data.size());

        /* 调用 wal_redo，只验证不崩溃 */
        int ret = wal_redo(wal_path, 0, fuzz_wal_apply, NULL);
        /* 返回 -1 或 0 都正常，不崩溃即可 */
        (void)ret;
    }
}

/**
 * @brief 空 WAL 文件
 */
TEST_F(WALFuzzTest, EmptyWALDataNoCrash) {
    write_random_wal_data(NULL, 0);
    int ret = wal_redo(wal_path, 0, fuzz_wal_apply, NULL);
    (void)ret;
}

/**
 * @brief 超大 WAL 记录
 */
TEST_F(WALFuzzTest, LargeWALDataNoCrash) {
    /* 生成一个 1MB 的随机 WAL 数据 */
    size_t large_size = 1024 * 1024;
    std::vector<uint8_t> large_data(large_size);
    for (size_t i = 0; i < large_size; i++) {
        large_data[i] = (uint8_t)(rand() & 0xFF);
    }
    write_random_wal_data(large_data.data(), large_data.size());

    int ret = wal_redo(wal_path, 0, fuzz_wal_apply, NULL);
    (void)ret;
}