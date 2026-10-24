# netconf 模块：NETCONF 网络配置协议支持
file(GLOB_RECURSE NETCONF_SOURCE_FILES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c")

if(NETCONF_SOURCE_FILES)
    add_library(netconf_server OBJECT ${NETCONF_SOURCE_FILES})
    target_include_directories(netconf_server PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${ENGINEERING_SOURCE_DIR}/include
        ${ENGINEERING_SOURCE_DIR}/include/db
    )
    target_link_libraries(netconf_server PRIVATE storage_yang)
endif()