# 复制一致性模块
file(GLOB_RECURSE RC_SOURCE_FILES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c")

if(RC_SOURCE_FILES)
    add_library(db_consistency STATIC ${RC_SOURCE_FILES})
    target_include_directories(db_consistency PUBLIC
        ${ENGINEERING_SOURCE_DIR}/include
        ${ENGINEERING_SOURCE_DIR}/include/db/consensus
        ${ENGINEERING_SOURCE_DIR}/include/db/replication
        ${ENGINEERING_SOURCE_DIR}/include/db/consistency
    )
    target_link_libraries(db_consistency PUBLIC
        db_consensus
        db_replication
    )
endif()

set_target_properties(db_consistency PROPERTIES FOLDER "Libraries")
