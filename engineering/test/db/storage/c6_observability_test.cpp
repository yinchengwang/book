#include <gtest/gtest.h>
#include <cstdio>

extern "C" {
#include "db/log_engine.h"
#include "db/storage/log/log_label_index.h"
#include "db/storage/log/log_engine_ext.h"
}

namespace {
const char *kLogDir = "/tmp/c6_log_test";
}  // namespace

TEST(LogLabelIndex, PutQuery) {
    remove(kLogDir);
    log_engine_t *e = log_engine_create((char *)kLogDir);
    ASSERT_NE(e, nullptr);

    log_label_index_t *idx = log_label_index_create((char *)kLogDir);
    ASSERT_NE(idx, nullptr);

    EXPECT_EQ(log_label_index_put(idx, "service", "api", 1), 0);
    EXPECT_EQ(log_label_index_put(idx, "service", "web", 2), 0);
    EXPECT_EQ(log_label_index_put(idx, "level", "info", 1), 0);

    /* 查询 service=api */
    struct {
        int n;
        uint64_t ids[8];
    } ctx = {0};
    int n = log_label_index_query(idx,
        (const char *[]){"service"}, (const char *[]){"api"}, 1,
        [](uint64_t sid, void *c) -> int {
            auto *ctx = (decltype(&ctx))c;
            ctx->ids[ctx->n++] = sid;
            return 0;
        }, &ctx);
    EXPECT_EQ(n, 1);
    EXPECT_EQ(ctx.ids[0], 1u);

    log_label_index_destroy(idx);
    log_engine_close(e);
    rmdir(kLogDir);
}

TEST(LogQL, Parser) {
    char **labels = NULL, **values = NULL;
    int n = 0;
    int rc = log_parse_logql_selector("{service=\"api\",level=\"info\"}",
                                      &labels, &values, &n);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(n, 2);
    if (n == 2) {
        EXPECT_STREQ(labels[0], "service");
        EXPECT_STREQ(values[0], "api");
        EXPECT_STREQ(labels[1], "level");
        EXPECT_STREQ(values[1], "info");
    }
    log_free_logql_parsed(labels, values, n);
}

TEST(LogEngine, PushAndQuery) {
    remove(kLogDir);
    log_engine_t *e = log_engine_create((char *)kLogDir);
    ASSERT_NE(e, nullptr);

    const char *keys[] = {"service", "level"};
    const char *vals[] = {"api", "info"};
    log_labels_t labels = { keys, vals, 2 };
    log_line_t lines[] = {
        {1000, "hello", 5},
        {2000, "world", 5},
    };
    EXPECT_EQ(log_push(e, &labels, lines, 2), 0);

    log_line_t out[10];
    size_t count = 0;
    int rc = log_query(e, NULL, "hello", 0, 9999, out, 10, &count);
    EXPECT_EQ(rc, 0);
    EXPECT_GE(count, 1u);

    log_engine_close(e);
    rmdir(kLogDir);
}
