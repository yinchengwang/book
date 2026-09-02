# DB 安全管理系统统一入口

file(GLOB SECURITY_MANAGER_SOURCE_FILES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c")

if(SECURITY_MANAGER_SOURCE_FILES)
    add_library(db_security STATIC ${SECURITY_MANAGER_SOURCE_FILES})
    target_include_directories(db_security PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${ENGINEERING_SOURCE_DIR}/include
        ${ENGINEERING_SOURCE_DIR}/include/db
        ${ENGINEERING_SOURCE_DIR}/include/db/security
    )
    target_link_libraries(db_security PRIVATE
        Threads::Threads
    )
endif()
