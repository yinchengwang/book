# kbase 库：轻量级推理引擎 + Obsidian 知识库
file(GLOB KBASE_SOURCES
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/infra/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/infra/ops/*.c"
)

if(KBASE_SOURCES)
    add_library(kbase STATIC ${KBASE_SOURCES})

    target_include_directories(kbase
        PUBLIC
            ${ENGINEERING_SOURCE_DIR}/include
            ${ENGINEERING_SOURCE_DIR}/include/kbase
    )

    target_link_libraries(kbase
        PUBLIC
            project_includes
            vector_index
            db_storage
            vector_persist
    )

    set_target_properties(kbase PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
        FOLDER "Libraries/kbase"
    )

    message(STATUS "Added kbase library: ${KBASE_SOURCES}")
else()
    message(WARNING "No sources found for kbase library")
endif()
