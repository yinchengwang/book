# PostgreSQL 风格 Hash 索引
file(GLOB_RECURSE PG_HASH_SOURCE_FILES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c")

if(PG_HASH_SOURCE_FILES)
    add_library(db_pg_hash STATIC ${PG_HASH_SOURCE_FILES})
    target_include_directories(db_pg_hash PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${ENGINEERING_SOURCE_DIR}/include
        ${ENGINEERING_SOURCE_DIR}/include/db
        ${ENGINEERING_SOURCE_DIR}/include/db/index
    )
    target_link_libraries(db_pg_hash PRIVATE db_storage)
endif()