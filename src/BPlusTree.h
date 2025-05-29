#pragma once

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "BufferPool.h"  // 引入BufferPool

// 常量定义
const int PAGE_SIZE = 4096;       // 页面大小
const int METADATA_SIZE = 16384;  // 元数据大小 16KB
const int KEY_SIZE = 64;          // 键的固定长度
const int ROW_ID_SIZE = 32;       // rowId的固定长度
const int VALUE_SIZE = 128;       // 值的固定长度
const int MAX_KEYS_PER_PAGE =
    (PAGE_SIZE - 64) / (KEY_SIZE + ROW_ID_SIZE + VALUE_SIZE);  // 每页最大键数

// 前向声明
class BPlusTreeNode;
class BPlusTree;

// 键值对结构
struct KeyValue {
    char key[KEY_SIZE];
    char rowId[ROW_ID_SIZE];
    char value[VALUE_SIZE];

    KeyValue() {
        memset(key, 0, KEY_SIZE);
        memset(rowId, 0, ROW_ID_SIZE);
        memset(value, 0, VALUE_SIZE);
    }

    KeyValue(const std::string& k, const std::string& rid,
             const std::string& v) {
        memset(key, 0, KEY_SIZE);
        memset(rowId, 0, ROW_ID_SIZE);
        memset(value, 0, VALUE_SIZE);

        strncpy(key, k.c_str(), std::min(k.length(), (size_t)KEY_SIZE - 1));
        strncpy(rowId, rid.c_str(),
                std::min(rid.length(), (size_t)ROW_ID_SIZE - 1));
        strncpy(value, v.c_str(), std::min(v.length(), (size_t)VALUE_SIZE - 1));
    }

    std::string getKey() const { return std::string(key); }
    std::string getRowId() const { return std::string(rowId); }
    std::string getValue() const { return std::string(value); }
};

// 页面头部信息
struct PageHeader {
    int pageId;
    int parentId;
    bool isLeaf;
    int keyCount;
    int nextLeafId;  // 叶子节点链表

    PageHeader()
        : pageId(-1), parentId(-1), isLeaf(true), keyCount(0), nextLeafId(-1) {}
};

// B+树节点
class BPlusTreeNode {
   public:
    PageHeader header;
    std::vector<KeyValue> keys;
    std::vector<int> children;  // 子节点页面ID
    bool dirty;                 // 脏页标记

    BPlusTreeNode(int pageId = -1, bool isLeaf = true);
    ~BPlusTreeNode() = default;

    // 序列化和反序列化
    void serialize(char* buffer) const;
    void deserialize(const char* buffer);

    // 节点操作
    bool isFull() const;
    int findKey(const std::string& key) const;
    void insertKey(const KeyValue& kv, int childId = -1);
    void removeKey(int index);
    void split(std::shared_ptr<BPlusTreeNode> newNode, KeyValue& promotedKey);

   private:
    static const int MAX_KEYS = MAX_KEYS_PER_PAGE;
    
};

// 统计信息
struct TreeStats {
    int height;
    int nodeCount;
    int splitCount;
    int mergeCount;
    double fillFactor;
    size_t fileWriteCount;  // 文件写入计数

    TreeStats()
        : height(0),
          nodeCount(0),
          splitCount(0),
          mergeCount(0),
          fillFactor(0.0),
          fileWriteCount(0)  // 初始化文件写入计数
    {}
};

// 元数据结构
struct Metadata {
    int rootPageId;
    int nextPageId;
    int pageCount;
    int splitCount;
    int mergeCount;
    char padding[METADATA_SIZE - 5 * sizeof(int)];

    Metadata()
        : rootPageId(-1),
          nextPageId(1),
          pageCount(0),
          splitCount(0),
          mergeCount(0) {
        memset(padding, 0, sizeof(padding));
    }
};

// B+树主类
class BPlusTree {
   private:
    std::string filename;
    std::fstream file;
    Metadata metadata;
    std::unique_ptr<BufferPool> bufferPool;  // 使用BufferPool替代简单的map缓存
    size_t fileWriteCount;                   // 文件写入计数, 用于调试和性能分析

    // 页面管理
    std::shared_ptr<BPlusTreeNode> loadPage(int pageId);
    void savePage(std::shared_ptr<BPlusTreeNode> node);
    std::shared_ptr<BPlusTreeNode> createNewPage(bool isLeaf = true);
    void saveMetadata();
    void loadMetadata();

    // B+树操作辅助函数
    std::shared_ptr<BPlusTreeNode> findLeafNode(const std::string& key);
    void insertInternal(std::shared_ptr<BPlusTreeNode> node, const KeyValue& kv,
                        int rightChildId = -1);
    void handleOverflow(std::shared_ptr<BPlusTreeNode> node);
    void handleUnderflow(std::shared_ptr<BPlusTreeNode> node);

    // 统计辅助函数
    int calculateHeight(std::shared_ptr<BPlusTreeNode> node);
    double calculateFillFactor();
    void mergeNodes(std::shared_ptr<BPlusTreeNode> leftNode,
                    std::shared_ptr<BPlusTreeNode> rightNode,
                    std::shared_ptr<BPlusTreeNode> parent, int parentKeyIndex);

    void redistributeFromLeft(std::shared_ptr<BPlusTreeNode> node,
                              std::shared_ptr<BPlusTreeNode> leftSibling,
                              std::shared_ptr<BPlusTreeNode> parent,
                              int parentKeyIndex);
    void redistributeFromRight(std::shared_ptr<BPlusTreeNode> node,
                               std::shared_ptr<BPlusTreeNode> rightSibling,
                               std::shared_ptr<BPlusTreeNode> parent,
                               int parentKeyIndex);

   public:
    BPlusTree();
    ~BPlusTree();

    // 主要接口
    bool create(const std::string& filename, int pageSize = PAGE_SIZE,
                size_t bufferPoolSize = 100);
    void close();
    bool insert(const std::string& key, const std::vector<std::string>& value,
                const std::string& rowId);
    std::vector<std::vector<std::string>> get(const std::string& key);
    bool remove(const std::string& key);
    TreeStats getStat();

    // BufferPool相关接口
    /**
     * @brief 设置缓冲池大小
     * @param size 缓冲池大小（页面数）
     */
    void setBufferPoolSize(size_t size);

    /**
     * @brief 获取缓冲池统计信息
     * @return BufferPool统计信息
     */
    BufferPool::Stats getBufferPoolStats() const;

    /**
     * @brief 刷新所有脏页到磁盘
     * @return 刷新的页面数量
     */
    int flushBuffer();

    /**
     * @brief 打印缓冲池状态
     */
    void printBufferPoolStatus() const;

    // 调试和测试
    void printTree();
    void printNode(std::shared_ptr<BPlusTreeNode> node, int level = 0);
};