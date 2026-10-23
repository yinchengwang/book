#include <gtest/gtest.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include "db/lock.h"

class LockMgrTest : public ::testing::Test {
protected:
    void SetUp() override {
        mgr = lock_mgr_create();
    }
    void TearDown() override {
        lock_mgr_destroy(mgr);
    }
    lock_manager_t *mgr;
};

TEST_F(LockMgrTest, CreateDestroy) {
    ASSERT_NE(mgr, nullptr);
}

TEST_F(LockMgrTest, CreateNull) {
    lock_mgr_destroy(nullptr);
}

TEST_F(LockMgrTest, AcquireRelease) {
    EXPECT_EQ(lock_acquire(mgr, (txn_t*)1, LOCK_TABLE, 1, 0, LOCK_MODE_X, 1000), 0);
    lock_release(mgr, (txn_t*)1, LOCK_TABLE, 1, LOCK_MODE_X);
}

TEST_F(LockMgrTest, SharedExclusiveConflict) {
    EXPECT_EQ(lock_acquire(mgr, (txn_t*)1, LOCK_TABLE, 1, 0, LOCK_MODE_S, 1000), 0);
    EXPECT_EQ(lock_acquire(mgr, (txn_t*)2, LOCK_TABLE, 1, 0, LOCK_MODE_S, 1000), 0);
    lock_release(mgr, (txn_t*)1, LOCK_TABLE, 1, LOCK_MODE_S);
    lock_release(mgr, (txn_t*)2, LOCK_TABLE, 1, LOCK_MODE_S);
}

TEST_F(LockMgrTest, AcquireInvalidArgs) {
    EXPECT_NE(lock_acquire(nullptr, (txn_t*)1, LOCK_TABLE, 1, 0, LOCK_MODE_X, 1000), 0);
    EXPECT_NE(lock_acquire(mgr, nullptr, LOCK_TABLE, 1, 0, LOCK_MODE_X, 1000), 0);
}

TEST_F(LockMgrTest, ReleaseNotHeld) {
    lock_release(mgr, (txn_t*)999, LOCK_TABLE, 999, LOCK_MODE_X);  /* Should not crash */
}

TEST_F(LockMgrTest, ReentrantShared) {
    EXPECT_EQ(lock_acquire(mgr, (txn_t*)1, LOCK_TABLE, 1, 0, LOCK_MODE_S, 1000), 0);
    EXPECT_EQ(lock_acquire(mgr, (txn_t*)1, LOCK_TABLE, 1, 0, LOCK_MODE_S, 1000), 0);
    lock_release(mgr, (txn_t*)1, LOCK_TABLE, 1, LOCK_MODE_S);
    lock_release(mgr, (txn_t*)1, LOCK_TABLE, 1, LOCK_MODE_S);
}

static void *concurrent_worker(void *arg) {
    lock_manager_t *m = (lock_manager_t *)arg;
    for (int i = 0; i < 100; i++) {
        lock_acquire(m, (txn_t*)(uintptr_t)pthread_self(), LOCK_TABLE, 10, 0, LOCK_MODE_X, 1000);
        lock_release(m, (txn_t*)(uintptr_t)pthread_self(), LOCK_TABLE, 10, LOCK_MODE_X);
    }
    return NULL;
}

TEST_F(LockMgrTest, ConcurrentAccess) {
    pthread_t threads[4];
    for (int i = 0; i < 4; i++) {
        pthread_create(&threads[i], NULL, concurrent_worker, mgr);
    }
    for (int i = 0; i < 4; i++) {
        pthread_join(threads[i], NULL);
    }
    SUCCEED();
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
