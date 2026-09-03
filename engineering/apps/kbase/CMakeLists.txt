# kbase CLI 应用
file(GLOB KBASE_CLI_SOURCES
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
)

if(KBASE_CLI_SOURCES)
    add_executable(kbase_cli ${KBASE_CLI_SOURCES})

    target_include_directories(kbase_cli
        PRIVATE
            ${ENGINEERING_SOURCE_DIR}/include
            ${ENGINEERING_SOURCE_DIR}/include/kbase
    )

    target_link_libraries(kbase_cli
        PRIVATE
            kbase
            project_includes
            vector_index
            db_storage
    )

    set_target_properties(kbase_cli PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        FOLDER "Apps/kbase"
    )

    message(STATUS "Added kbase_cli app: ${KBASE_CLI_SOURCES}")
else()
    message(WARNING "No sources found for kbase_cli")
endif()