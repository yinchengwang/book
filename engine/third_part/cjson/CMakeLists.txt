# cJSON 单文件 JSON 库
# 官方地址: https://github.com/DaveGamble/cJSON

# 如果没有找到系统 cJSON，使用 bundled 源码
if(NOT cJSON_FOUND)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/cJSON.c)
        add_library(cjson STATIC ${CMAKE_CURRENT_SOURCE_DIR}/cJSON.c)
        target_include_directories(cjson PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}
        )
    else()
        # 回退到系统库
        add_library(cjson INTERFACE)
        find_path(CJSON_INCLUDE_DIR "cJSON.h" PATHS /usr/include /usr/local/include)
        if(CJSON_INCLUDE_DIR)
            target_include_directories(cjson INTERFACE ${CJSON_INCLUDE_DIR})
        endif()
        find_library(CJSON_LIBRARY NAMES cjson)
        if(CJSON_LIBRARY)
            target_link_libraries(cjson INTERFACE ${CJSON_LIBRARY})
        endif()
    endif()
endif()
