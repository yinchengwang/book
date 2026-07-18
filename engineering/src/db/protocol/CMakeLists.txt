# protocol 模块：非 SQL 协议层
file(GLOB_RECURSE PROTOCOL_SOURCE_FILES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c")

if(PROTOCOL_SOURCE_FILES)
    add_library(db_protocol STATIC ${PROTOCOL_SOURCE_FILES})
    target_include_directories(db_protocol PUBLIC
        ${ENGINEERING_SOURCE_DIR}/include
        ${ENGINEERING_SOURCE_DIR}/include/db
        ${ENGINEERING_SOURCE_DIR}/include/db/protocol
    )
endif()