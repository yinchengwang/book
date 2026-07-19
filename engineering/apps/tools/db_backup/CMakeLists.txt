file(GLOB DB_BACKUP_SOURCES CONFIGURE_DEPENDS "*.c")
add_executable(db_backup ${DB_BACKUP_SOURCES})
target_link_libraries(db_backup PRIVATE db_core project_includes)