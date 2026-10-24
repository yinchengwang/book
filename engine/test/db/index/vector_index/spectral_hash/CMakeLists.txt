# Spectral Hash 测试

add_executable(spectral_hash_test spectral_hash_test.cpp)

target_link_libraries(spectral_hash_test PRIVATE
    spectral_hash
    gtest
    gtest_main
    Threads::Threads
)