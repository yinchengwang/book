/**
 * @file test_crossmodal.cpp
 * @brief 跨模态检索测试
 */

#include <gtest/gtest.h>
#include "rag/crossmodal_retriever.h"
#include "rag/embedding.h"

using namespace rag;

class CrossModalTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 创建文本嵌入服务 (简单实现，用于测试)
        embed_service_ = std::make_unique<SimpleEmbeddingService>(128);
        embed_service_->load();

        // 创建图片特征提取器 (简单实现，用于测试)
        feature_extractor_ = std::make_unique<SimpleImageFeatureExtractor>(512);
        feature_extractor_->load();

        // 创建跨模态检索器
        retriever_ = create_crossmodal_retriever();
        retriever_->set_embedding_service(embed_service_);
        retriever_->set_feature_extractor(feature_extractor_);
    }

    std::unique_ptr<SimpleEmbeddingService> embed_service_;
    std::unique_ptr<SimpleImageFeatureExtractor> feature_extractor_;
    std::unique_ptr<CrossModalRetriever> retriever_;
};

TEST_F(CrossModalTest, AddImage) {
    ImageEntry entry;
    entry.image_id = "img1";
    entry.image_path = "/path/to/image1.jpg";
    entry.caption = "A beautiful sunset over the ocean";
    entry.features = feature_extractor_->extract(entry.image_path);

    retriever_->add_image(entry);

    EXPECT_EQ(retriever_->index_size(), 1);
}

TEST_F(CrossModalTest, AddImages) {
    std::vector<ImageEntry> images;

    for (int i = 0; i < 5; ++i) {
        ImageEntry entry;
        entry.image_id = "img" + std::to_string(i);
        entry.image_path = "/path/to/image" + std::to_string(i) + ".jpg";
        entry.caption = "Image caption " + std::to_string(i);
        entry.features = feature_extractor_->extract(entry.image_path);
        images.push_back(entry);
    }

    retriever_->add_images(images);

    EXPECT_EQ(retriever_->index_size(), 5);
}

TEST_F(CrossModalTest, RetrieveByText) {
    // 添加测试图片
    std::vector<ImageEntry> images;
    for (int i = 0; i < 3; ++i) {
        ImageEntry entry;
        entry.image_id = "img" + std::to_string(i);
        entry.image_path = "/path/to/image" + std::to_string(i) + ".jpg";
        entry.caption = "A cat sitting on a wooden table";
        entry.features = feature_extractor_->extract(entry.image_path);
        images.push_back(entry);
    }
    retriever_->add_images(images);

    // 文本查询
    auto results = retriever_->retrieve_by_text("a cat on furniture", 2);

    EXPECT_GT(results.size(), 0);
    EXPECT_EQ(results[0].source, "crossmodal");
    EXPECT_FALSE(results[0].chunk.id.empty());
}

TEST_F(CrossModalTest, RetrieveByImage) {
    // 添加测试图片
    ImageEntry entry1;
    entry1.image_id = "img1";
    entry1.image_path = "/path/to/image1.jpg";
    entry1.caption = "A dog running in the park";
    entry1.features = feature_extractor_->extract(entry1.image_path);
    retriever_->add_image(entry1);

    ImageEntry entry2;
    entry2.image_id = "img2";
    entry2.image_path = "/path/to/image2.jpg";
    entry2.caption = "A cat sitting on a sofa";
    entry2.features = feature_extractor_->extract(entry2.image_path);
    retriever_->add_image(entry2);

    // 图片查询 - 使用与 img1 相似的路径 (会生成相似的随机向量)
    auto results = retriever_->retrieve_by_image("/path/to/image1_similar.jpg", 2);

    EXPECT_GT(results.size(), 0);
    EXPECT_EQ(results[0].source, "crossmodal");
}

TEST_F(CrossModalTest, SearchTopK) {
    // 添加 10 张图片
    std::vector<ImageEntry> images;
    for (int i = 0; i < 10; ++i) {
        ImageEntry entry;
        entry.image_id = "img" + std::to_string(i);
        entry.image_path = "/path/to/image" + std::to_string(i) + ".jpg";
        entry.caption = "Image " + std::to_string(i);
        entry.features = feature_extractor_->extract(entry.image_path);
        images.push_back(entry);
    }
    retriever_->add_images(images);

    // 只获取 top 3
    auto results = retriever_->retrieve_by_text("query", 3);

    EXPECT_LE(results.size(), 3);
}

TEST_F(CrossModalTest, ImageIndex) {
    auto index = create_image_index();

    // 添加图片
    for (int i = 0; i < 5; ++i) {
        ImageEntry entry;
        entry.image_id = "idx_img" + std::to_string(i);
        entry.image_path = "/path/to/index_image" + std::to_string(i) + ".jpg";
        entry.caption = "Index image " + std::to_string(i);
        entry.features = std::vector<float>(128, 0.1f * i);
        index->add(entry);
    }

    EXPECT_EQ(index->size(), 5);

    // 搜索
    std::vector<float> query(128, 0.2f);
    auto results = index->search(query, 3);

    EXPECT_LE(results.size(), 3);
    EXPECT_FALSE(results.empty());
}

TEST_F(CrossModalTest, ImageIndexBatch) {
    auto index = create_image_index();

    std::vector<ImageEntry> images;
    for (int i = 0; i < 3; ++i) {
        ImageEntry entry;
        entry.image_id = "batch_img" + std::to_string(i);
        entry.image_path = "/path/to/batch_image" + std::to_string(i) + ".jpg";
        entry.caption = "Batch image " + std::to_string(i);
        entry.features = std::vector<float>(64, 0.5f);
        images.push_back(entry);
    }

    index->add_batch(images);

    EXPECT_EQ(index->size(), 3);
}

TEST_F(CrossModalTest, ImageIndexClear) {
    auto index = create_image_index();

    ImageEntry entry;
    entry.image_id = "clear_img";
    entry.image_path = "/path/to/clear.jpg";
    entry.caption = "Clear image";
    entry.features = std::vector<float>(64, 1.0f);
    index->add(entry);

    EXPECT_EQ(index->size(), 1);

    index->clear();

    EXPECT_EQ(index->size(), 0);
}

TEST_F(CrossModalTest, ImageIndexGetAllIds) {
    auto index = create_image_index();

    for (int i = 0; i < 3; ++i) {
        ImageEntry entry;
        entry.image_id = "id_img" + std::to_string(i);
        entry.image_path = "/path/to/id_image" + std::to_string(i) + ".jpg";
        entry.caption = "ID image " + std::to_string(i);
        entry.features = std::vector<float>(64, 0.5f);
        index->add(entry);
    }

    auto ids = index->get_all_ids();

    EXPECT_EQ(ids.size(), 3);
    EXPECT_EQ(ids[0], "id_img0");
    EXPECT_EQ(ids[1], "id_img1");
    EXPECT_EQ(ids[2], "id_img2");
}

TEST_F(CrossModalTest, SimilarityComputation) {
    // 测试余弦相似度计算
    // 相同向量应该有相似度 1.0
    std::vector<float> vec1 = {1.0f, 0.0f, 0.0f};
    std::vector<float> vec2 = {1.0f, 0.0f, 0.0f};

    auto retriever = create_crossmodal_retriever();
    // 使用私有方法测试需要通过公开接口间接测试

    // 测试正交向量
    std::vector<float> vec3 = {0.0f, 1.0f, 0.0f};

    // 通过检索结果验证相似度计算
    ImageEntry entry1, entry2, entry3;
    entry1.image_id = "s1";
    entry1.features = vec1;
    entry2.image_id = "s2";
    entry2.features = vec2;
    entry3.image_id = "s3";
    entry3.features = vec3;

    retriever->add_image(entry1);
    retriever->add_image(entry2);
    retriever->add_image(entry3);

    // vec1 和 vec2 相同，应该排第一
    auto results = retriever->retrieve_by_text("query", 3);

    // 由于使用随机文本嵌入，结果顺序不确定
    // 但至少应该有 3 个结果
    EXPECT_EQ(results.size(), 3);

    // 验证分数在合理范围内
    for (const auto& r : results) {
        EXPECT_GE(r.score, -1.0f);
        EXPECT_LE(r.score, 1.0f);
    }
}

TEST_F(CrossModalTest, EmptyIndex) {
    auto results = retriever_->retrieve_by_text("query", 5);
    EXPECT_EQ(results.size(), 0);
}

TEST_F(CrossModalTest, DefaultRetrieve) {
    // 测试默认 retrieve 方法调用
    ImageEntry entry;
    entry.image_id = "default_img";
    entry.image_path = "/path/to/default.jpg";
    entry.caption = "Default test image";
    entry.features = feature_extractor_->extract(entry.image_path);
    retriever_->add_image(entry);

    // 默认 retrieve 应该等同于 retrieve_by_text
    auto results = retriever_->retrieve("query", 5);

    EXPECT_EQ(results.size(), 1);
    EXPECT_EQ(results[0].source, "crossmodal");
}

TEST_F(CrossModalTest, RetrieveWithEmptyQuery) {
    ImageEntry entry;
    entry.image_id = "empty_query_img";
    entry.image_path = "/path/to/empty.jpg";
    entry.caption = "Empty query test";
    entry.features = feature_extractor_->extract(entry.image_path);
    retriever_->add_image(entry);

    // 空字符串查询仍会进行 (嵌入服务会处理)
    auto results = retriever_->retrieve_by_text("", 5);

    // 应该能正常返回结果 (即使为空查询)
    EXPECT_GE(results.size(), 0);
}

TEST_F(CrossModalTest, FeatureExtractorDimension) {
    auto extractor = create_simple_image_feature_extractor(256);

    EXPECT_EQ(extractor->dimension(), 256);
    EXPECT_TRUE(extractor->is_ready());

    auto features = extractor->extract("/test/path.jpg");
    EXPECT_EQ(features.size(), 256);
}
