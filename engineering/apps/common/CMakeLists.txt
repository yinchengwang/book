# 公共代码库

add_library(apps_common STATIC
    terminal.c
    menu.c
)

target_include_directories(apps_common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(apps_common PRIVATE
    project_includes
)