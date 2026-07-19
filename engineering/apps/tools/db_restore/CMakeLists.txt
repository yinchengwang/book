file(GLOB DB_RESTORE_SOURCES CONFIGURE_DEPENDS "*.c")
add_executable(db_restore ${DB_RESTORE_SOURCES})
target_link_libraries(db_restore PRIVATE db_core project_includes)