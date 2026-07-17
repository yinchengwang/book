#include <stdio.h>
#include <stdint.h>
int main() {
    uint32_t mt[624];
    uint32_t seed = 789;
    mt[0] = seed;
    for (int i = 1; i < 5; i++) {
        uint32_t prev = mt[i-1];
        mt[i] = 1812433253UL * (prev ^ (prev >> 30)) + (uint32_t)i;
        printf("mt[%d] = %u (0x%08X)\n", i, mt[i], mt[i]);
    }

    // 测试 MT19937 完整输出
    // 梅森旋转
    int MT_N = 624, MT_M = 397;
    uint32_t magic[2] = {0, 0x9908b0dfUL};
    int mti = MT_N;
    for (int kk = 0; kk < MT_N - MT_M; kk++) {
        uint32_t y = (mt[kk] & 0x80000000UL) | (mt[kk + 1] & 0x7fffffffUL);
        mt[kk] = mt[kk + MT_M] ^ (y >> 1) ^ magic[y & 1];
    }
    for (int kk = MT_N - MT_M; kk < MT_N - 1; kk++) {
        uint32_t y = (mt[kk] & 0x80000000UL) | (mt[kk + 1] & 0x7fffffffUL);
        mt[kk] = mt[kk + (MT_M - MT_N)] ^ (y >> 1) ^ magic[y & 1];
    }
    uint32_t y = (mt[MT_N - 1] & 0x80000000UL) | (mt[0] & 0x7fffffffUL);
    mt[MT_N - 1] = mt[MT_M - 1] ^ (y >> 1) ^ magic[y & 1];
    mti = 0;

    // 输出第一个 u32
    y = mt[mti++];
    y ^= (y >> 11);
    y ^= (y << 7) & 0x9d2c5680UL;
    y ^= (y << 15) & 0xefc60000UL;
    y ^= (y >> 18);
    printf("first u32 = %u (0x%08X)\n", y, y);
    printf("u32 %% 1000 = %u\n", y % 1000);

    return 0;
}