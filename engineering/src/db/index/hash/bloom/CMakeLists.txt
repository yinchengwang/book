# Bloom Filter
file(GLOB_RECURSE BLOOM_SOURCE_FILES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c")

if(BLOOM_SOURCE_FILES)
    add_library(db_bloom STATIC ${BLOOM_SOURCE_FILES})
    target_include_directories(db_bloom PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${ENGINEERING_SOURCE_DIR}/include
    )
endif()