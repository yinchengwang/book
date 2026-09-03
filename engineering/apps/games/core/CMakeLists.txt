# engineering/apps/games/core/CMakeLists.txt

add_project_library(games_core GAMES_CORE_SOURCES_VAR)

# 注册到父级 games/
target_link_libraries(games_core PUBLIC project_includes)
