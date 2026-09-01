/**
 * @file simd_test.cpp
 * @brief Gap#2 向量化执行引擎 Task2 单元测试：真实 SIMD 比较内核 + 运行时分派 + 诚实检测
 *
 * 覆盖：
 * - 四种类型（int32 / int64 / float / double）× 六种 CompareOp × 边界长度
 *   （0,1,7,15,16,17,31,63,64,65,257）的过滤位图，与测试内独立标量参考逐位一致
 * - 随机数据大批量（1000 元素）对拍
 * - 位图缓冲的「先清零后写入」契约与越界哨兵检查
 * - NaN 语义与 C 标量比较一致（float / double）
 * - SIMD 检测函数自洽性（detect / best / has_extension / 类型字符串）
 *
 * 关键纪律：本文件只调用**经运行时分派的公开函数**（vector_filter_*_simd），
 * 绝不直接调用 filter_*_avx2 等内部内核，因此在不支持 AVX2 的主机上不会触发 #UD。
 */
#include <gtest/gtest.h>

extern "C" {
#include "db/core/vector_exec.h"
#include "db/vectorized/vectorized.h"
}

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

namespace {

/* 位图哨兵：写在有效字之后，用于检测内核越界写 */
constexpr uint64_t kGuard = 0xA5A5A5A5DEADBEEFULL;
constexpr int kGuardWords = 2;

/* 覆盖标量尾部、SSE2（4/2 通道）、AVX2（8/4 通道）与跨 64 位字的边界长度 */
const std::vector<int> &BoundaryLengths() {
    static const std::vector<int> v = {0, 1, 7, 15, 16, 17, 31, 63, 64, 65, 257};
    return v;
}

const std::vector<CompareOp> &AllOps() {
    static const std::vector<CompareOp> v = {CMP_EQ, CMP_NE, CMP_LT, CMP_LE, CMP_GT, CMP_GE};
    return v;
}

const char *OpName(CompareOp op) {
    switch (op) {
        case CMP_EQ: return "EQ";
        case CMP_NE: return "NE";
        case CMP_LT: return "LT";
        case CMP_LE: return "LE";
        case CMP_GT: return "GT";
        case CMP_GE: return "GE";
        default:     return "??";
    }
}

/* 测试内独立标量参考实现——刻意与被测代码分开写，避免共用同一处 bug */
template <typename T>
std::vector<uint64_t> RefBitmap(const std::vector<T> &a, T b, CompareOp op) {
    int n = static_cast<int>(a.size());
    int nwords = (n + 63) / 64;
    std::vector<uint64_t> bm(static_cast<size_t>(nwords), 0);
    for (int i = 0; i < n; i++) {
        bool m = false;
        switch (op) {
            case CMP_EQ: m = (a[i] == b); break;
            case CMP_NE: m = (a[i] != b); break;
            case CMP_LT: m = (a[i] <  b); break;
            case CMP_LE: m = (a[i] <= b); break;
            case CMP_GT: m = (a[i] >  b); break;
            case CMP_GE: m = (a[i] >= b); break;
            default: m = false; break;
        }
        if (m) bm[static_cast<size_t>(i / 64)] |= (1ULL << (i % 64));
    }
    return bm;
}

/* 简单确定性 PRNG（xorshift64*），保证跨平台可复现 */
struct Rng {
    uint64_t s;
    explicit Rng(uint64_t seed) : s(seed ? seed : 0x9E3779B97F4A7C15ULL) {}
    uint64_t next() {
        s ^= s >> 12;
        s ^= s << 25;
        s ^= s >> 27;
        return s * 0x2545F4914F6CDD1DULL;
    }
    /* 取小值域，保证 EQ/NE/LT/GT 都有非平凡的命中分布 */
    int32_t nextSmall() { return static_cast<int32_t>(next() % 21) - 10; }
};

/* 分配「nwords 有效字 + 哨兵字」的缓冲 */
std::vector<uint64_t> MakeBuf(int n, uint64_t fill) {
    int nwords = (n + 63) / 64;
    std::vector<uint64_t> buf(static_cast<size_t>(nwords + kGuardWords), fill);
    for (int g = 0; g < kGuardWords; g++) {
        buf[static_cast<size_t>(nwords + g)] = kGuard;
    }
    return buf;
}

void CheckGuard(const std::vector<uint64_t> &buf, int n, const std::string &ctx) {
    int nwords = (n + 63) / 64;
    for (int g = 0; g < kGuardWords; g++) {
        EXPECT_EQ(buf[static_cast<size_t>(nwords + g)], kGuard)
            << ctx << " 越界写入哨兵字 " << g;
    }
}

/* 逐字比较实际位图与参考位图 */
void ExpectBitmapEq(const std::vector<uint64_t> &actual,
                    const std::vector<uint64_t> &expect,
                    int n, const std::string &ctx) {
    int nwords = (n + 63) / 64;
    for (int w = 0; w < nwords; w++) {
        EXPECT_EQ(actual[static_cast<size_t>(w)], expect[static_cast<size_t>(w)])
            << ctx << " 第 " << w << " 字不一致（n=" << n << "）";
    }
}

/* ------------------------------------------------------------------
 * 通用驱动：对给定长度/操作符跑一次公开分派函数并与参考对拍
 * ------------------------------------------------------------------ */

template <typename T, typename Fn>
void RunCase(Fn filter_fn, const std::vector<T> &a, T b, CompareOp op,
             const char *type_name, uint64_t prefill) {
    int n = static_cast<int>(a.size());
    std::vector<uint64_t> buf = MakeBuf(n, prefill);
    filter_fn(a.empty() ? nullptr : a.data(), b, n, op, buf.data());

    std::string ctx = std::string(type_name) + "/" + OpName(op) + "/n=" + std::to_string(n);

    if (n == 0) {
        /* n<=0 直接返回，不写任何字；此处只要求不崩溃、不动哨兵 */
        CheckGuard(buf, n, ctx);
        return;
    }
    ExpectBitmapEq(buf, RefBitmap(a, b, op), n, ctx);
    CheckGuard(buf, n, ctx);
}

}  // namespace

/* ========================================================================
 * 1. int32：全操作符 × 全边界长度
 * ======================================================================== */
TEST(SimdFilterInt32, AllOpsAllLengths) {
    Rng rng(12345);
    for (int n : BoundaryLengths()) {
        std::vector<int32_t> a(static_cast<size_t>(n));
        for (int i = 0; i < n; i++) a[static_cast<size_t>(i)] = rng.nextSmall();
        /* 保证枢轴值确实出现在数据中，EQ/NE 非平凡 */
        if (n > 0) a[static_cast<size_t>(n / 2)] = 0;
        for (CompareOp op : AllOps()) {
            RunCase<int32_t>(vector_filter_int_simd, a, 0, op, "int32", 0);
        }
    }
}

TEST(SimdFilterInt32, ClearsResultBufferFirst) {
    /* 分派函数必须先清零 ceil(n/64) 个字，脏缓冲不得污染结果 */
    Rng rng(999);
    for (int n : BoundaryLengths()) {
        if (n == 0) continue;
        std::vector<int32_t> a(static_cast<size_t>(n));
        for (int i = 0; i < n; i++) a[static_cast<size_t>(i)] = rng.nextSmall();
        for (CompareOp op : AllOps()) {
            RunCase<int32_t>(vector_filter_int_simd, a, 3, op, "int32-dirty",
                             0xFFFFFFFFFFFFFFFFULL);
        }
    }
}

TEST(SimdFilterInt32, Extremes) {
    /* 边界值：INT32_MIN/MAX 附近，验证有符号比较（cmpgt_epi32）正确 */
    std::vector<int32_t> a = {INT32_MIN, INT32_MIN + 1, -1, 0, 1,
                              INT32_MAX - 1, INT32_MAX, -12345, 12345, 0};
    const int32_t pivots[] = {INT32_MIN, -1, 0, 1, INT32_MAX};
    for (int32_t b : pivots) {
        for (CompareOp op : AllOps()) {
            RunCase<int32_t>(vector_filter_int_simd, a, b, op, "int32-extreme", 0);
        }
    }
}

TEST(SimdFilterInt32, RandomLargeBatch) {
    Rng rng(0xC0FFEE);
    std::vector<int32_t> a(1000);
    for (size_t i = 0; i < a.size(); i++) a[i] = static_cast<int32_t>(rng.next() % 200) - 100;
    for (int t = 0; t < 8; t++) {
        int32_t b = static_cast<int32_t>(rng.next() % 200) - 100;
        for (CompareOp op : AllOps()) {
            RunCase<int32_t>(vector_filter_int_simd, a, b, op, "int32-rand", 0);
        }
    }
}

/* ========================================================================
 * 2. int64
 * ======================================================================== */
TEST(SimdFilterInt64, AllOpsAllLengths) {
    Rng rng(54321);
    for (int n : BoundaryLengths()) {
        std::vector<int64_t> a(static_cast<size_t>(n));
        for (int i = 0; i < n; i++) a[static_cast<size_t>(i)] = rng.nextSmall();
        if (n > 0) a[static_cast<size_t>(n / 2)] = 0;
        for (CompareOp op : AllOps()) {
            RunCase<int64_t>(vector_filter_int64_simd, a, 0, op, "int64", 0);
        }
    }
}

TEST(SimdFilterInt64, ClearsResultBufferFirst) {
    Rng rng(1357);
    for (int n : BoundaryLengths()) {
        if (n == 0) continue;
        std::vector<int64_t> a(static_cast<size_t>(n));
        for (int i = 0; i < n; i++) a[static_cast<size_t>(i)] = rng.nextSmall();
        for (CompareOp op : AllOps()) {
            RunCase<int64_t>(vector_filter_int64_simd, a, -2, op, "int64-dirty",
                             0xFFFFFFFFFFFFFFFFULL);
        }
    }
}

TEST(SimdFilterInt64, Extremes) {
    /* 关键：跨越 32 位边界的值，验证 64 位有符号比较（不是拆成 32 位比较的错误实现） */
    std::vector<int64_t> a = {INT64_MIN, INT64_MIN + 1, -4294967297LL, -4294967296LL,
                              -1, 0, 1, 4294967295LL, 4294967296LL, 4294967297LL,
                              INT64_MAX - 1, INT64_MAX};
    const int64_t pivots[] = {INT64_MIN, -4294967296LL, -1, 0, 1, 4294967296LL, INT64_MAX};
    for (int64_t b : pivots) {
        for (CompareOp op : AllOps()) {
            RunCase<int64_t>(vector_filter_int64_simd, a, b, op, "int64-extreme", 0);
        }
    }
}

TEST(SimdFilterInt64, RandomLargeBatch) {
    Rng rng(0xBEEF64);
    std::vector<int64_t> a(1000);
    for (size_t i = 0; i < a.size(); i++) {
        /* 混合小值与跨 32 位大值 */
        a[i] = (i % 3 == 0) ? static_cast<int64_t>(rng.next())
                            : (static_cast<int64_t>(rng.next() % 200) - 100);
    }
    for (int t = 0; t < 8; t++) {
        int64_t b = (t % 2) ? static_cast<int64_t>(rng.next())
                            : (static_cast<int64_t>(rng.next() % 200) - 100);
        for (CompareOp op : AllOps()) {
            RunCase<int64_t>(vector_filter_int64_simd, a, b, op, "int64-rand", 0);
        }
    }
}

/* ========================================================================
 * 3. float
 * ======================================================================== */
TEST(SimdFilterFloat, AllOpsAllLengths) {
    Rng rng(2468);
    for (int n : BoundaryLengths()) {
        std::vector<float> a(static_cast<size_t>(n));
        for (int i = 0; i < n; i++) {
            a[static_cast<size_t>(i)] = static_cast<float>(rng.nextSmall()) * 0.5f;
        }
        if (n > 0) a[static_cast<size_t>(n / 2)] = 0.0f;
        for (CompareOp op : AllOps()) {
            RunCase<float>(vector_filter_float_simd, a, 0.0f, op, "float", 0);
        }
    }
}

TEST(SimdFilterFloat, ClearsResultBufferFirst) {
    Rng rng(3690);
    for (int n : BoundaryLengths()) {
        if (n == 0) continue;
        std::vector<float> a(static_cast<size_t>(n));
        for (int i = 0; i < n; i++) {
            a[static_cast<size_t>(i)] = static_cast<float>(rng.nextSmall()) * 0.25f;
        }
        for (CompareOp op : AllOps()) {
            RunCase<float>(vector_filter_float_simd, a, 1.25f, op, "float-dirty",
                           0xFFFFFFFFFFFFFFFFULL);
        }
    }
}

TEST(SimdFilterFloat, SpecialValues) {
    /* NaN / ±Inf / ±0：SIMD 内核必须与 C 标量比较语义逐位一致
       （EQ/LT/LE/GT/GE 遇 NaN 为假，NE 遇 NaN 为真；-0.0 == +0.0） */
    const float nan_v = std::numeric_limits<float>::quiet_NaN();
    const float inf_v = std::numeric_limits<float>::infinity();
    std::vector<float> a = {nan_v, -nan_v, inf_v, -inf_v, 0.0f, -0.0f,
                            1.0f, -1.0f, 3.5f, -3.5f,
                            std::numeric_limits<float>::min(),
                            std::numeric_limits<float>::max()};
    /* 补到跨 SSE2/AVX2 通道与跨字长度 */
    while (a.size() < 70) a.push_back(a[a.size() % 12]);

    const float pivots[] = {0.0f, -0.0f, 1.0f, inf_v, -inf_v, nan_v};
    for (float b : pivots) {
        for (CompareOp op : AllOps()) {
            RunCase<float>(vector_filter_float_simd, a, b, op, "float-special", 0);
        }
    }
}

TEST(SimdFilterFloat, RandomLargeBatch) {
    Rng rng(0xF10A7);
    std::vector<float> a(1000);
    for (size_t i = 0; i < a.size(); i++) {
        a[i] = static_cast<float>(static_cast<int32_t>(rng.next() % 2000) - 1000) * 0.125f;
    }
    for (int t = 0; t < 8; t++) {
        float b = static_cast<float>(static_cast<int32_t>(rng.next() % 2000) - 1000) * 0.125f;
        for (CompareOp op : AllOps()) {
            RunCase<float>(vector_filter_float_simd, a, b, op, "float-rand", 0);
        }
    }
}

/* ========================================================================
 * 4. double
 * ======================================================================== */
TEST(SimdFilterDouble, AllOpsAllLengths) {
    Rng rng(13579);
    for (int n : BoundaryLengths()) {
        std::vector<double> a(static_cast<size_t>(n));
        for (int i = 0; i < n; i++) {
            a[static_cast<size_t>(i)] = static_cast<double>(rng.nextSmall()) * 0.5;
        }
        if (n > 0) a[static_cast<size_t>(n / 2)] = 0.0;
        for (CompareOp op : AllOps()) {
            RunCase<double>(vector_filter_double_simd, a, 0.0, op, "double", 0);
        }
    }
}

TEST(SimdFilterDouble, ClearsResultBufferFirst) {
    Rng rng(24680);
    for (int n : BoundaryLengths()) {
        if (n == 0) continue;
        std::vector<double> a(static_cast<size_t>(n));
        for (int i = 0; i < n; i++) {
            a[static_cast<size_t>(i)] = static_cast<double>(rng.nextSmall()) * 0.25;
        }
        for (CompareOp op : AllOps()) {
            RunCase<double>(vector_filter_double_simd, a, -1.75, op, "double-dirty",
                            0xFFFFFFFFFFFFFFFFULL);
        }
    }
}

TEST(SimdFilterDouble, SpecialValues) {
    const double nan_v = std::numeric_limits<double>::quiet_NaN();
    const double inf_v = std::numeric_limits<double>::infinity();
    std::vector<double> a = {nan_v, -nan_v, inf_v, -inf_v, 0.0, -0.0,
                             1.0, -1.0, 3.5, -3.5,
                             std::numeric_limits<double>::min(),
                             std::numeric_limits<double>::max()};
    while (a.size() < 70) a.push_back(a[a.size() % 12]);

    const double pivots[] = {0.0, -0.0, 1.0, inf_v, -inf_v, nan_v};
    for (double b : pivots) {
        for (CompareOp op : AllOps()) {
            RunCase<double>(vector_filter_double_simd, a, b, op, "double-special", 0);
        }
    }
}

TEST(SimdFilterDouble, RandomLargeBatch) {
    Rng rng(0xD0AB1E);
    std::vector<double> a(1000);
    for (size_t i = 0; i < a.size(); i++) {
        a[i] = static_cast<double>(static_cast<int32_t>(rng.next() % 2000) - 1000) * 0.125;
    }
    for (int t = 0; t < 8; t++) {
        double b = static_cast<double>(static_cast<int32_t>(rng.next() % 2000) - 1000) * 0.125;
        for (CompareOp op : AllOps()) {
            RunCase<double>(vector_filter_double_simd, a, b, op, "double-rand", 0);
        }
    }
}

/* ========================================================================
 * 5. 空指针 / 非法入参
 * ======================================================================== */
TEST(SimdFilterNullArgs, DoesNotCrash) {
    uint64_t bm[8];
    memset(bm, 0, sizeof(bm));
    int32_t ai[4] = {1, 2, 3, 4};
    int64_t al[4] = {1, 2, 3, 4};
    float   af[4] = {1, 2, 3, 4};
    double  ad[4] = {1, 2, 3, 4};

    vector_filter_int_simd(nullptr, 0, 4, CMP_EQ, bm);
    vector_filter_int_simd(ai, 0, 4, CMP_EQ, nullptr);
    vector_filter_int_simd(ai, 0, 0, CMP_EQ, bm);
    vector_filter_int_simd(ai, 0, -5, CMP_EQ, bm);

    vector_filter_int64_simd(nullptr, 0, 4, CMP_EQ, bm);
    vector_filter_int64_simd(al, 0, 4, CMP_EQ, nullptr);
    vector_filter_int64_simd(al, 0, -1, CMP_EQ, bm);

    vector_filter_float_simd(nullptr, 0, 4, CMP_EQ, bm);
    vector_filter_float_simd(af, 0, 4, CMP_EQ, nullptr);
    vector_filter_float_simd(af, 0, 0, CMP_EQ, bm);

    vector_filter_double_simd(nullptr, 0, 4, CMP_EQ, bm);
    vector_filter_double_simd(ad, 0, 4, CMP_EQ, nullptr);
    vector_filter_double_simd(ad, 0, 0, CMP_EQ, bm);

    /* 非法入参不得写入位图 */
    for (int i = 0; i < 8; i++) EXPECT_EQ(bm[i], 0ULL);
}

/* ========================================================================
 * 6. 字符串过滤（保持标量，本 gap 不做 SIMD 字符串比较）
 * ======================================================================== */
TEST(SimdFilterString, ScalarSemanticsUnchanged) {
    const char *vals[] = {"abc", "abd", "abc", "zzz", "", "abc"};
    const int n = 6;
    uint64_t bm[1 + kGuardWords];

    memset(bm, 0, sizeof(bm));
    bm[1] = kGuard;
    vector_filter_string_simd(vals, "abc", n, CMP_EQ, bm);
    EXPECT_EQ(bm[0], (1ULL << 0) | (1ULL << 2) | (1ULL << 5));
    EXPECT_EQ(bm[1], kGuard);

    memset(bm, 0, sizeof(bm));
    bm[1] = kGuard;
    vector_filter_string_simd(vals, "abc", n, CMP_NE, bm);
    EXPECT_EQ(bm[0], (1ULL << 1) | (1ULL << 3) | (1ULL << 4));
    EXPECT_EQ(bm[1], kGuard);

    /* 非法入参不崩溃 */
    vector_filter_string_simd(nullptr, "abc", n, CMP_EQ, bm);
    vector_filter_string_simd(vals, "abc", n, CMP_EQ, nullptr);
    vector_filter_string_simd(vals, "abc", 0, CMP_EQ, bm);
}

/* ========================================================================
 * 7. SIMD 检测函数如实报告
 * ======================================================================== */
TEST(SimdDetect, SelfConsistent) {
    SimdExtension best = simd_get_best_extension();
    EXPECT_EQ(best, simd_detect_extension()) << "best 必须与 detect 一致";

    /* 多次调用结果稳定 */
    for (int i = 0; i < 4; i++) {
        EXPECT_EQ(simd_detect_extension(), best);
        EXPECT_EQ(simd_get_best_extension(), best);
    }

    /* SIMD_NONE 恒为真（任何机器都「支持」无 SIMD） */
    EXPECT_TRUE(simd_has_extension(SIMD_NONE));
    /* 自身必然支持 */
    EXPECT_TRUE(simd_has_extension(best));

    /* 低于 best 的 x86 扩展都应支持；高于 best 的都不支持 */
    const SimdExtension x86_chain[] = {SIMD_SSE, SIMD_SSE2, SIMD_SSE4,
                                       SIMD_AVX, SIMD_AVX2, SIMD_AVX512};
    if (best != SIMD_NEON && best != SIMD_NONE) {
        for (SimdExtension e : x86_chain) {
            EXPECT_EQ(simd_has_extension(e), best >= e)
                << "扩展枚举值 " << static_cast<int>(e) << " 的报告与 best 不一致";
        }
    }

    /* has_extension 与 has_simd_support 一致 */
    EXPECT_EQ(vector_has_simd_support(), best != SIMD_NONE);
}

TEST(SimdDetect, ReportsHonestlyOnX86) {
#if defined(__x86_64__) || defined(_M_X64)
    const char *forced = std::getenv("MMDB_SIMD");
    if (forced && *forced) {
        GTEST_SKIP() << "MMDB_SIMD=" << forced << " 强制降级中，跳过硬件基线断言";
    }
    /* x86_64 基线保证 SSE2，检测函数不得再硬编码 SIMD_NONE */
    EXPECT_NE(simd_get_best_extension(), SIMD_NONE)
        << "x86_64 至少应报告 SSE2";
    EXPECT_GE(static_cast<int>(simd_get_best_extension()), static_cast<int>(SIMD_SSE2));
    EXPECT_TRUE(vector_has_simd_support());
    EXPECT_STRNE(vector_get_simd_type(), "scalar");
#else
    GTEST_SKIP() << "非 x86_64 平台，跳过基线断言";
#endif
}

TEST(SimdDetect, EnvDowngradeRespected) {
    /* MMDB_SIMD 只降不升：设了什么级别，分派就不得高于该级别 */
    const char *forced = std::getenv("MMDB_SIMD");
    if (!forced || !*forced) {
        GTEST_SKIP() << "未设置 MMDB_SIMD，本用例只在降级运行中生效";
    }
    SimdExtension cap = SIMD_NONE;
    if      (strcmp(forced, "scalar") == 0) cap = SIMD_NONE;
    else if (strcmp(forced, "sse2")   == 0) cap = SIMD_SSE2;
    else if (strcmp(forced, "sse4.2") == 0) cap = SIMD_SSE4;
    else if (strcmp(forced, "avx2")   == 0) cap = SIMD_AVX2;
    else GTEST_SKIP() << "MMDB_SIMD 取值无法识别：" << forced;

    EXPECT_LE(static_cast<int>(simd_get_best_extension()), static_cast<int>(cap))
        << "分派级别高于 MMDB_SIMD 指定的上限";
}

TEST(SimdDetect, TypeStrings) {
    const char *t = vector_get_simd_type();
    ASSERT_NE(t, nullptr);
    EXPECT_GT(strlen(t), 0u);

    const char *v = vecx_active_simd();
    ASSERT_NE(v, nullptr);
    EXPECT_GT(strlen(v), 0u);

    /* db_core 与 db_vectorized 两层对外报告必须一致 */
    EXPECT_STREQ(t, v);

    /* 字符串必须落在已知集合内，并与枚举对应 */
    SimdExtension best = simd_get_best_extension();
    const char *want = "scalar";
    switch (best) {
        case SIMD_AVX2: want = "avx2";   break;
        case SIMD_SSE4: want = "sse4.2"; break;
        case SIMD_SSE2: want = "sse2";   break;
        default:        want = "scalar"; break;
    }
    EXPECT_STREQ(t, want);

    /* 打印一次，便于在 CI 日志确认实际走的内核 */
    RecordProperty("active_simd", t);
    std::printf("[ SIMD     ] 运行时分派内核 = %s (enum=%d)\n", t, static_cast<int>(best));
}
