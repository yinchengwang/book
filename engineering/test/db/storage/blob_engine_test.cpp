#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>

#include <gtest/gtest.h>

extern "C" {
#include "db/sha256.h"
}

namespace {

std::string to_hex(const uint8_t digest[SHA256_DIGEST_SIZE]) {
    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (size_t i = 0; i < SHA256_DIGEST_SIZE; ++i) {
        output << std::setw(2) << static_cast<unsigned int>(digest[i]);
    }
    return output.str();
}

}  // 匿名命名空间

TEST(Sha256, StandardVectors) {
    uint8_t digest[SHA256_DIGEST_SIZE];

    sha256_compute("", 0, digest);
    EXPECT_EQ(to_hex(digest), "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");

    sha256_compute("abc", 3, digest);
    EXPECT_EQ(to_hex(digest), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");

    std::string million_a(1000000, 'a');
    sha256_compute(million_a.data(), million_a.size(), digest);
    EXPECT_EQ(to_hex(digest), "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
}

TEST(Sha256, StreamingUpdatesMatchOneShot) {
    const std::string input = "流式 SHA-256 测试数据 abc";
    uint8_t expected[SHA256_DIGEST_SIZE];
    uint8_t actual[SHA256_DIGEST_SIZE];
    sha256_compute(input.data(), input.size(), expected);

    sha256_ctx_t context;
    sha256_init(&context);
    for (size_t offset = 0; offset < input.size(); ++offset) {
        sha256_update(&context, input.data() + offset, 1);
    }
    sha256_final(&context, actual);

    EXPECT_EQ(to_hex(actual), to_hex(expected));
}
