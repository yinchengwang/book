/**
 * @file schema_manager.cpp
 * @brief 数据库 Schema 管理器实现
 */

#include "mmrag/database.h"

namespace mmrag {

SchemaManager::SchemaManager(Database& db) : db_(db) {}

void SchemaManager::init() {
    // 创建必要的表
    // Note: Full implementation pending Phase 6 (db_storage)
}

bool SchemaManager::exists() {
    // Schema exists check - simplified implementation
    return true;
}

int SchemaManager::get_version() {
    // Default version for schema migration
    return CURRENT_VERSION;
}

void SchemaManager::set_version(int version) {
    // Schema version management
    (void)version;
}

}  // namespace mmrag
