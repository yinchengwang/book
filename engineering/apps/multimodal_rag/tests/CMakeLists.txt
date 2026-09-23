# multimodal_rag 单元测试（BUILD_TESTING 且 gtest 可用时构建）
if(NOT BUILD_TESTING)
  return()
endif()
if(NOT TARGET gtest)
  return()
endif()

add_executable(mmrag_intent_tests
    test_query_classifier.cpp
    test_intent_gate.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/retrieval/query_classifier.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/retrieval/intent_gate.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/pipeline/pipeline.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/pipeline/pipeline_base.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/types.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/observability/logger.cpp
)

target_include_directories(mmrag_intent_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${ENGINEERING_SOURCE_DIR}/third_part/nlohmann/single_include
    ${ENGINEERING_SOURCE_DIR}/include
)

target_link_libraries(mmrag_intent_tests PRIVATE
    gtest gtest_main project_includes Threads::Threads
)

set_target_properties(mmrag_intent_tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    FOLDER "Apps/multimodal_rag"
)

add_test(NAME mmrag_intent_tests COMMAND mmrag_intent_tests)
