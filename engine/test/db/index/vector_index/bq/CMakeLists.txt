# BQ 二值量化器测试

file(GLOB BQ_TEST_SOURCES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.cpp")

add_executable(bq_test ${BQ_TEST_SOURCES})

target_include_directories(bq_test PRIVATE
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/src
    ${ENGINEERING_SOURCE_DIR}/../vendor/third_part/googletest/googletest/include
)

target_link_libraries(bq_test PRIVATE
    bq
    gtest
    gtest_main
    Threads::Threads
)

include(GoogleTest)
gtest_discover_tests(bq_test PROPERTIES LABELS "bq;quantization" DISCOVERY_TIMEOUT 60)
