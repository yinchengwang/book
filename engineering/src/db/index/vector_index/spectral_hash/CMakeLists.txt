# Spectral Hash 库

add_library(spectral_hash STATIC spectral_hash.c)

target_include_directories(spectral_hash PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${ENGINEERING_SOURCE_DIR}/include
)

target_link_libraries(spectral_hash PRIVATE project_includes Threads::Threads)