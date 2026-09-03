#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include "db/common_rwlock.h"
#include <pthread.h>
#include <assert.h>

static void test_rwlock_create_destroy(void** state) {
    (void) state;
    common_rwlock_t* lock = common_rwlock_create("test");
    assert_non_null(lock);
    assert_true(lock->use_lock);
    common_rwlock_destroy(lock);
}

static void test_rwlock_read_lock(void** state) {
    (void) state;
    common_rwlock_t* lock = common_rwlock_create("test");
    common_rwlock_read_lock(lock);
    common_rwlock_read_unlock(lock);
    common_rwlock_destroy(lock);
}

static void test_rwlock_write_lock(void** state) {
    (void) state;
    common_rwlock_t* lock = common_rwlock_create("test");
    common_rwlock_write_lock(lock);
    common_rwlock_write_unlock(lock);
    common_rwlock_destroy(lock);
}

static void* reader_thread(void* arg) {
    common_rwlock_t* lock = arg;
    for (int i = 0; i < 1000; i++) {
        common_rwlock_read_lock(lock);
        common_rwlock_read_unlock(lock);
    }
    return NULL;
}

static void* writer_thread(void* arg) {
    common_rwlock_t* lock = arg;
    for (int i = 0; i < 100; i++) {
        common_rwlock_write_lock(lock);
        common_rwlock_write_unlock(lock);
    }
    return NULL;
}

static void test_rwlock_concurrent(void** state) {
    (void) state;
    common_rwlock_t* lock = common_rwlock_create("test");
    pthread_t readers[5], writers[2];

    for (int i = 0; i < 5; i++) pthread_create(&readers[i], NULL, reader_thread, lock);
    for (int i = 0; i < 2; i++) pthread_create(&writers[i], NULL, writer_thread, lock);

    for (int i = 0; i < 5; i++) pthread_join(readers[i], NULL);
    for (int i = 0; i < 2; i++) pthread_join(writers[i], NULL);

    common_rwlock_destroy(lock);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_rwlock_create_destroy),
        cmocka_unit_test(test_rwlock_read_lock),
        cmocka_unit_test(test_rwlock_write_lock),
        cmocka_unit_test(test_rwlock_concurrent),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
