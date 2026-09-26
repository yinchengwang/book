#include "mmrag/version.h"
#include "mmrag/config.h"
#include "mmrag/engine.h"
#include "mmrag/server.h"
#include "mmrag/logger.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <thread>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#else
// POSIX headers for get_exe_dir()'s readlink branch
#include <sys/types.h>
#include <unistd.h>
#endif

namespace {
std::atomic<bool> g_stop{false};
}

static void handle_signal(int /*sig*/) {
    g_stop.store(true);
}

static void print_usage(const char* argv0) {
    std::printf("Usage: %s [--port PORT] [--config PATH]\n", argv0);
}

// 获取可执行文件所在目录
static std::filesystem::path get_exe_dir() {
#ifdef _WIN32
    char buf[MAX_PATH] = {0};
    GetModuleFileNameA(nullptr, buf, MAX_PATH);
    return std::filesystem::path(buf).parent_path();
#else
    char buf[4096] = {0};
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len > 0) {
        buf[len] = '\0';
        return std::filesystem::path(buf).parent_path();
    }
    return std::filesystem::current_path();
#endif
}

int main(int argc, char* argv[]) {
    int port = 8080;
    std::string config_path = "";

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            port = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
            config_path = argv[++i];
        } else if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        } else {
            std::fprintf(stderr, "Unknown arg: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    // 获取可执行文件目录，用于解析相对路径
    std::filesystem::path exe_dir = get_exe_dir();
    // 工程根目录：build/bin → ../..
    std::filesystem::path project_root = std::filesystem::weakly_canonical(exe_dir / "../..");

    // 解析配置文件路径：优先使用用户指定路径，否则在工程根目录下查找
    if (config_path.empty()) {
        // 尝试多个可能的配置文件位置
        std::vector<std::string> candidates = {
            (project_root / "apps/multimodal_rag/config/default.yaml").string(),
            (exe_dir / "config" / "default.yaml").string(),
        };
        for (const auto& candidate : candidates) {
            if (std::ifstream(candidate).good()) {
                config_path = candidate;
                break;
            }
        }
        if (config_path.empty()) {
            config_path = (project_root / "apps/multimodal_rag/config/default.yaml").string();
        }
    }

    std::printf("multimodal_rag_server v%s\n", mmrag::MM_RAG_VERSION);
    std::printf("Port: %d\n", port);
    std::printf("Config: %s\n", config_path.c_str());
    std::printf("Exe dir: %s\n", exe_dir.string().c_str());
    std::fflush(stdout);

    // 加载配置（不存在时用默认值）
    mmrag::Config config;
    try {
        mmrag::ConfigLoader loader;
        if (std::ifstream(config_path).good()) {
            config = loader.load(config_path);
        } else {
            std::printf("Config not found at %s, using defaults\n",
                        config_path.c_str());
            config = mmrag::Config::default_config();
        }
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Failed to load config: %s, using defaults\n", e.what());
        config = mmrag::Config::default_config();
    }
    config.server.port = port;

    // 如果数据目录是相对路径，解析为相对于工程根目录的绝对路径
    if (!config.server.data_dir.empty() &&
        !std::filesystem::path(config.server.data_dir).is_absolute()) {
        config.server.data_dir = std::filesystem::weakly_canonical(project_root / config.server.data_dir).string();
    }
    if (!config.data_dir.empty() &&
        !std::filesystem::path(config.data_dir).is_absolute()) {
        config.data_dir = std::filesystem::weakly_canonical(project_root / config.data_dir).string();
    }
    if (!config.logging.file.empty() &&
        !std::filesystem::path(config.logging.file).is_absolute()) {
        config.logging.file = std::filesystem::weakly_canonical(project_root / config.logging.file).string();
    }
    // 同步 server.data_dir 到 config.data_dir，保持一致
    config.data_dir = config.server.data_dir;

    // 同步 index_dir 和 cache_dir
    config.index_dir = (std::filesystem::path(config.data_dir) / "index").string();
    config.cache_dir = (std::filesystem::path(config.data_dir) / "cache").string();

    // 同步 server.data_dir 到 config.data_dir（再次确保同步）
    config.server.data_dir = config.data_dir;

    std::printf("Project root: %s\n", project_root.string().c_str());
    std::printf("Data dir: %s\n", config.server.data_dir.c_str());

    // 创建并初始化 RAG 引擎
    auto engine_unique = mmrag::create_engine(mmrag::EngineConfig{
        .config = config,
        .load_existing_index = true,
        .auto_create_dirs = true,
    });
    std::shared_ptr<mmrag::RAGEngine> engine = std::move(engine_unique);

    // 启动完整 API 服务器（含 /api/v1/* 路由）
    auto server = mmrag::api::create_server();
    server->set_engine(engine);

    mmrag::ServerConfig server_cfg = config.server;
    server_cfg.port = port;
    if (!server->start(server_cfg)) {
        std::fprintf(stderr, "Failed to start API server on port %d\n", port);
        return 1;
    }

    // 优雅退出
    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);
    while (!g_stop.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    std::printf("\nShutting down...\n");
    server->stop();
    return 0;
}
