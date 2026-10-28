# storage/doc 模块：文档存储引擎
file(GLOB_RECURSE DOC_SOURCE_FILES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c")

if(DOC_SOURCE_FILES)
    # Task 12：独立静态库（模板见 storage/kv）；cjk_tokenizer.c 自 core 迁入
    add_library(mm_doc STATIC ${DOC_SOURCE_FILES})
    target_include_directories(mm_doc PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${ENGINEERING_SOURCE_DIR}/include
        ${ENGINEERING_SOURCE_DIR}/include/db
        ${ENGINEERING_SOURCE_DIR}/include/db/storage
        ${ENGINEERING_SOURCE_DIR}/include/db/core
        ${ENGINEERING_SOURCE_DIR}/third_part/uthash/src
    )
    # doc 白名单依赖 vector（doc_vector.c 使用 hnsw 索引）
    target_link_libraries(mm_doc PUBLIC mm_base mm_vector)
    set_target_properties(mm_doc PROPERTIES FOLDER "ModalityEngines")
endif()
