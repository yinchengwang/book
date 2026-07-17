# validator 模块：校验层
file(GLOB_RECURSE VALIDATOR_SOURCE_FILES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c")

if(VALIDATOR_SOURCE_FILES)
    add_library(db_validator STATIC ${VALIDATOR_SOURCE_FILES})
    target_include_directories(db_validator PUBLIC
        ${ENGINEERING_SOURCE_DIR}/include
        ${ENGINEERING_SOURCE_DIR}/include/db
        ${ENGINEERING_SOURCE_DIR}/include/db/validator
        ${ENGINEERING_SOURCE_DIR}/include/db/parser
    )
    target_link_libraries(db_validator PRIVATE db_parser_sql_simplified db_storage)
    target_compile_options(db_validator PRIVATE -Wno-unused-parameter)
endif()
