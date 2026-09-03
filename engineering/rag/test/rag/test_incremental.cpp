/**
 * @file test_incremental.cpp
 * @brief 增量索引模块测试
 */

#include <gtest/gtest.h>
#include "rag/incremental_indexer.h"
#include <filesystem>
#include <fstream>

using namespace rag;

class TempDir {
public:
    TempDir() {
        path_ = std::filesystem::temp_directory_path() / "rag_test_XXXXXX";
        std::filesystem::create_directories(path_);
    }

    ~TempDir() {
        std::filesystem::remove_all(path_);
    }

    std::string path() const { return path_.string(); }

    void create_file(const std::string& name, const std::string& content = "test") {
        std::ofstream file(path_ / name);
        file << content;
    }

    void modify_file(const std::string& name, const std::string& content) {
        std::ofstream file(path_ / name, std::ios::trunc);
        file << content;
    }

    void delete_file(const std::string& name) {
        std::filesystem::remove(path_ / name);
    }

private:
    std::filesystem::path path_;
};

TEST(IncrementalIndexerTest, DetectChangesAdded) {
    TempDir dir;
    dir.create_file("file1.txt", "content1");
    dir.create_file("file2.txt", "content2");

    IncrementalIndexer indexer;

    // 第一次检测 - 所有文件都是新增
    IndexDelta delta = indexer.detect_changes(dir.path());

    EXPECT_EQ(delta.added_files.size(), 2);
    EXPECT_TRUE(delta.modified_files.empty());
    EXPECT_TRUE(delta.deleted_files.empty());
}

TEST(IncrementalIndexerTest, DetectChangesModified) {
    TempDir dir;
    dir.create_file("file1.txt", "original");

    IncrementalIndexer indexer;
    indexer.detect_changes(dir.path());

    // 修改文件
    dir.modify_file("file1.txt", "modified");

    // 等待一下确保修改时间不同
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    IndexDelta delta = indexer.detect_changes(dir.path());

    EXPECT_TRUE(delta.added_files.empty());
    EXPECT_EQ(delta.modified_files.size(), 1);
    EXPECT_TRUE(delta.deleted_files.empty());
}

TEST(IncrementalIndexerTest, DetectChangesDeleted) {
    TempDir dir;
    dir.create_file("file1.txt");
    dir.create_file("file2.txt");

    IncrementalIndexer indexer;
    indexer.detect_changes(dir.path());

    // 删除文件
    dir.delete_file("file1.txt");

    IndexDelta delta = indexer.detect_changes(dir.path());

    EXPECT_TRUE(delta.added_files.empty());
    EXPECT_TRUE(delta.modified_files.empty());
    EXPECT_EQ(delta.deleted_files.size(), 1);
}

TEST(IncrementalIndexerTest, ApplyDelta) {
    TempDir dir;
    dir.create_file("new.txt", "new content");
    dir.create_file("modify.txt", "original");

    IncrementalIndexer indexer;

    // 初始检测
    indexer.detect_changes(dir.path());

    // 添加新文件和修改
    dir.create_file("another.txt", "another");
    dir.modify_file("modify.txt", "modified");

    // 再次检测并应用
    IndexDelta delta = indexer.detect_changes(dir.path());
    indexer.apply_delta(delta);

    // 验证
    EXPECT_EQ(delta.added_files.size(), 1);
    EXPECT_EQ(delta.modified_files.size(), 1);
}

TEST(IncrementalIndexerTest, SaveLoadState) {
    TempDir dir;
    dir.create_file("file1.txt", "content1");
    dir.create_file("file2.txt", "content2");

    IncrementalIndexer indexer;
    indexer.detect_changes(dir.path());

    std::string state_file = (dir.path() / "state.json").string();
    indexer.save_state(state_file);

    // 创建新的 indexer 并加载状态
    IncrementalIndexer indexer2;
    bool loaded = indexer2.load_state(state_file);

    EXPECT_TRUE(loaded);

    // 验证状态文件存在
    EXPECT_TRUE(std::filesystem::exists(state_file));
}

TEST(RealTimeIndexerTest, Debounce) {
    auto indexer = std::make_shared<IncrementalIndexer>();
    RealTimeIndexer rt_indexer(indexer);

    rt_indexer.set_debounce_ms(500);
    // Verify set_debounce_ms doesn't throw
}

TEST(RealTimeIndexerTest, StartStopWatch) {
    TempDir dir;
    dir.create_file("file.txt");

    auto indexer = std::make_shared<IncrementalIndexer>();
    RealTimeIndexer rt_indexer(indexer);

    rt_indexer.start_watch(dir.path());
    rt_indexer.stop_watch();

    // 验证多次停止不会崩溃
    rt_indexer.stop_watch();
}

TEST(IncrementalIndexerTest, FactoryFunction) {
    auto indexer = create_incremental_indexer();
    EXPECT_NE(indexer, nullptr);

    auto rt_indexer = create_realtime_indexer(indexer);
    EXPECT_NE(rt_indexer, nullptr);
}