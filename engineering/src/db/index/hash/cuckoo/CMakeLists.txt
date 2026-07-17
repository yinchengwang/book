# Cuckoo Filter
file(GLOB_RECURSE CUCKOO_SOURCE_FILES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c")

if(CUCKOO_SOURCE_FILES)
    add_library(db_cuckoo STATIC ${CUCKOO_SOURCE_FILES})
    target_include_directories(db_cuckoo PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${ENGINEERING_SOURCE_DIR}/include
        ${ENGINEERING_SOURCE_DIR}/include/db
        ${ENGINEERING_SOURCE_DIR}/include/db/index
    )
endif()