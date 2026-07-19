/**
 * @file page_fuzzer.cpp
 * @brief 页面解析模糊测试
 *
 * 生成随机页面数据并调用 page_verify_checksum()，验证不崩溃。
 */
#include <gtest/gtest.h>
#include <vector>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <algorithm>

extern "C" {
#include "db/page.h"
}

/**
 * @brief 页面模糊测试夹具
 */
class PageFuzzTest : public ::testing::Test {
protected:
    void SetUp() override {
        /* 使用固定种子保证可重复 */
        srand(42);
    }
};

/**
 * @brief 随机页面数据，验证不崩溃
 *
 * 生成 10000 个随机页面，每个调用 page_verify_checksum()，
 * 只验证不崩溃，不检查返回值。
 */
TEST_F(PageFuzzTest, RandomPageDataNoCrash) {
    for (int i = 0; i < 10000; i++) {
        /* 分配随机页面数据 */
        std::vector<uint8_t> raw_data(DEFAULT_PAGE_SIZE);
        for (size_t j = 0; j < raw_data.size(); j++) {
            raw_data[j] = (uint8_t)(rand() & 0xFF);
        }

        /* 转换为页面指针并验证不崩溃 */
        const page_t *page = reinterpret_cast<const page_t *>(raw_data.data());
        /* 仅调用校验和函数，不检查返回值 */
        (void)page_verify_checksum(page);
    }
}

/**
 * @brief 部分随机页面数据（保持头部合法）
 *
 * 设置合法头部，但数据区随机。
 */
TEST_F(PageFuzzTest, PartialRandomPageNoCrash) {
    for (int i = 0; i < 10000; i++) {
        std::vector<uint8_t> raw_data(DEFAULT_PAGE_SIZE, 0);

        /* 设置合法头部 */
        page_header_t *hdr = reinterpret_cast<page_header_t *>(raw_data.data());
        hdr->page_id = (uint32_t)(rand() & 0xFFFF);
        hdr->page_type = (uint8_t)(rand() % 5);  /* 0-4 是合法类型 */
        hdr->free_space_offset = (uint16_t)(rand() & 0xFFFF);

        /* 数据区完全随机 */
        for (size_t j = sizeof(page_header_t); j < raw_data.size(); j++) {
            raw_data[j] = (uint8_t)(rand() & 0xFF);
        }

        const page_t *page = reinterpret_cast<const page_t *>(raw_data.data());
        (void)page_verify_checksum(page);
    }
}

/**
 * @brief 空指针/零大小输入，验证不崩溃
 */
TEST_F(PageFuzzTest, NullPtrNoCrash) {
    /* 空指针不应崩溃 */
    /* 如果有 assert，此测试会触发 assert 失败但不会崩溃 */
    if (page_verify_checksum(nullptr)) {
        /* 如果返回 true 或 false 都行，只要不崩溃 */
    }
}

/**
 * @brief 边界大小页面数据
 */
TEST_F(PageFuzzTest, BoundarySizeNoCrash) {
    /* 各种边界大小
     * 注意：page_verify_checksum() 会读取 sizeof(page_t)=65536 字节，
     * 因此 raw_data 必须分配至少 sizeof(page_t)+16 字节以避免 OOB 访问。
     * page_calc_checksum_bytes() 接收显式 size 参数，更安全。
     */
    const size_t min_size = sizeof(page_t) + 16;
    const size_t sizes[] = {min_size, min_size + 1, min_size + 100, min_size + 1024, min_size + 65536};
    for (size_t size : sizes) {
        std::vector<uint8_t> raw_data(size, 0);
        if (size >= sizeof(page_header_t)) {
            const page_t *page = reinterpret_cast<const page_t *>(raw_data.data());
            (void)page_verify_checksum(page);
            (void)page_calc_checksum_bytes(raw_data.data(), size);
        }
    }
}