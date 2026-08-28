add_executable(api-server
    main.c
    config.c
    modules/health.c
    modules/todo_api.c
    modules/notes_api_mod.c
    modules/digest_api.c
    modules/quiz_api.c
    modules/interview_api.c
    modules/review_api.c
    modules/static_files.c
)

target_include_directories(api-server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/modules
    ${ENGINEERING_SOURCE_DIR}/apps/common
    ${ENGINEERING_SOURCE_DIR}/apps/todo-app
    ${ENGINEERING_SOURCE_DIR}/../third_part/sqlite3
    ${ENGINEERING_SOURCE_DIR}/../third_part
)

if(NOT TARGET sqlite3)
    add_subdirectory(${ENGINEERING_SOURCE_DIR}/../third_part/sqlite3 ${CMAKE_BINARY_DIR}/sqlite3_build)
endif()

target_link_libraries(api-server PRIVATE
    apps_common
    todo
    cjson
    sqlite3
    ws2_32
)

