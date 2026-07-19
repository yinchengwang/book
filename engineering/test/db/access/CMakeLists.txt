# Access Method 测试（Index AM 框架）
add_executable(test_amapi test_amapi.cpp)
target_link_libraries(test_amapi access_amapi project_includes gtest gtest_main)

add_executable(test_am_btree test_am_btree.cpp)
target_link_libraries(test_am_btree access_amapi project_includes gtest gtest_main)

add_executable(test_am_hnsw test_am_hnsw.cpp)
target_link_libraries(test_am_hnsw access_amapi project_includes gtest gtest_main)

add_executable(test_btbuild_empty test_btbuild_empty.cpp)
target_link_libraries(test_btbuild_empty db_storage project_includes gtest gtest_main)