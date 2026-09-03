# SQ 量化器测试

file(GLOB SQ_TEST_SOURCES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.cpp")

add_executable(sq_test ${SQ_TEST_SOURCES})

target_include_directories(sq_test PRIVATE
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/third_part/googletest/googletest/include
)

target_link_libraries(sq_test PRIVATE
    sq
    gtest
    gtest_main
    Threads::Threads
)

include(GoogleTest)
gtest_discover_tests(sq_test
    PROPERTIES LABELS "sq;quantization"
)