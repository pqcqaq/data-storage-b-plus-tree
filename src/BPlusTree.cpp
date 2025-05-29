#include "BPlusTree.h"

#include <cassert>
#include <limits>
#include <queue>
#include <sstream>

// ================================ BPlusTreeNode 实现================================

/**
 * @brief BPlusTreeNode 构造函数
 * @param pageId 页面ID，用于唯一标识此节点
 * @param isLeaf 是否为叶子节点的标志
 * 
 * 初始化B+树节点的基本属性，包括页面头信息和容器预分配
 */
BPlusTreeNode::BPlusTreeNode(int pageId, bool isLeaf) : dirty(false) {
    // 设置页面头信息
    header.pageId = pageId;           // 设置页面唯一标识符
    header.isLeaf = isLeaf;           // 标记节点类型（叶子或内部节点）
    header.keyCount = 0;              // 初始化键数量为0
    header.parentId = -1;             // 初始化父节点ID为-1（表示无父节点）
    header.nextLeafId = -1;           // 初始化下一个叶子节点ID为-1

    // 预分配键向量容量，提高插入性能
    keys.reserve(MAX_KEYS_PER_PAGE);

    // 如果不是叶子节点，预分配子节点指针向量容量
    if (!isLeaf) {
        children.reserve(MAX_KEYS_PER_PAGE + 1);  // 内部节点的子节点数 = 键数 + 1
    }
}

/**
 * @brief 将节点数据序列化到缓冲区
 * @param buffer 目标缓冲区，大小必须为PAGE_SIZE
 * 
 * 将节点的所有数据（页面头、键值、子节点指针）按顺序写入缓冲区，
 * 用于持久化存储到磁盘文件中
 */
void BPlusTreeNode::serialize(char* buffer) const {
    // 清零整个缓冲区，确保未使用区域为0
    memset(buffer, 0, PAGE_SIZE);

    // 复制页面头信息到缓冲区开始位置
    memcpy(buffer, &header, sizeof(PageHeader));
    int offset = sizeof(PageHeader);          // 记录当前写入位置

    // 依次复制所有键值对到缓冲区
    for (int i = 0; i < header.keyCount; i++) {
        memcpy(buffer + offset, &keys[i], sizeof(KeyValue));
        offset += sizeof(KeyValue);          // 更新写入位置
    }

    // 对于内部节点，需要序列化子节点指针
    if (!header.isLeaf) {
        for (int i = 0; i <= header.keyCount; i++) {
            if (i < children.size()) {
                // 复制有效的子节点ID
                memcpy(buffer + offset, &children[i], sizeof(int));
            } else {
                // 对于不存在的子节点，写入-1作为无效标记
                int invalidId = -1;
                memcpy(buffer + offset, &invalidId, sizeof(int));
            }
            offset += sizeof(int);           // 更新写入位置
        }
    }
}

/**
 * @brief 从缓冲区反序列化节点数据
 * @param buffer 源缓冲区，包含序列化的节点数据
 * 
 * 从缓冲区中读取节点数据并重建节点状态，
 * 用于从磁盘文件中加载节点数据
 */
void BPlusTreeNode::deserialize(const char* buffer) {
    // 从缓冲区复制页面头信息
    memcpy(&header, buffer, sizeof(PageHeader));
    int offset = sizeof(PageHeader);          // 记录当前读取位置

    // 清空并重建键向量
    keys.clear();
    keys.resize(header.keyCount);            // 根据键数量调整向量大小
    for (int i = 0; i < header.keyCount; i++) {
        // 依次读取每个键值对
        memcpy(&keys[i], buffer + offset, sizeof(KeyValue));
        offset += sizeof(KeyValue);          // 更新读取位置
    }

    // 清空子节点向量
    children.clear();
    // 对于内部节点，需要反序列化子节点指针
    if (!header.isLeaf) {
        children.resize(header.keyCount + 1); // 内部节点的子节点数 = 键数 + 1
        for (int i = 0; i <= header.keyCount; i++) {
            // 依次读取每个子节点ID
            memcpy(&children[i], buffer + offset, sizeof(int));
            offset += sizeof(int);           // 更新读取位置
        }
    }

    // 标记节点为干净状态（未修改）
    dirty = false;
}

/**
 * @brief 检查节点是否已满
 * @return true 如果节点已满，false 否则
 * 
 * 判断节点中的键数量是否达到最大限制，
 * 用于决定是否需要进行节点分裂操作
 */
bool BPlusTreeNode::isFull() const {
    // 当键数量达到最大值时认为已满
    return header.keyCount >= MAX_KEYS_PER_PAGE;
}

/**
 * @brief 在节点中查找键的插入位置
 * @param key 要查找的键值
 * @return 键应该插入的位置索引
 * 
 * 使用二分查找算法在有序键数组中找到指定键的位置，
 * 如果键存在则返回其位置，否则返回应该插入的位置
 */
int BPlusTreeNode::findKey(const std::string& key) const {
    int left = 0;                            // 搜索范围左边界
    int right = header.keyCount;             // 搜索范围右边界

    // 二分查找循环
    while (left < right) {
        int mid = left + (right - left) / 2; // 计算中点，防止溢出

        // 比较中点键值与目标键值
        if (keys[mid].getKey() >= key) {
            right = mid;                     // 目标在左半部分
        } else {
            left = mid + 1;                  // 目标在右半部分
        }
    }

    return left;                             // 返回插入位置
}

/**
 * @brief 在节点中插入键值对
 * @param kv 要插入的键值对
 * @param childId 相关的子节点ID（仅对内部节点有效，默认为-1）
 * 
 * 在节点的正确位置插入新的键值对，保持键的有序性，
 * 对于内部节点还需要插入对应的子节点指针
 */
void BPlusTreeNode::insertKey(const KeyValue& kv, int childId) {
    // 检查是否还有空间进行插入
    if (header.keyCount >= MAX_KEYS_PER_PAGE) {
        std::cerr << "Warning: Attempting to insert into full node"
                  << std::endl;
        return;                              // 节点已满，无法插入
    }

    // 找到键值对应该插入的位置
    int pos = findKey(kv.getKey());

    // 在指定位置插入键值对，保持有序性
    keys.insert(keys.begin() + pos, kv);
    header.keyCount++;                       // 增加键计数

    // 对于内部节点，需要插入对应的子节点指针
    if (!header.isLeaf && childId != -1) {
        // 子节点指针插入在键的右侧
        children.insert(children.begin() + pos + 1, childId);
    }

    // 标记节点为脏状态（已修改）
    dirty = true;
}

/**
 * @brief 从节点中删除指定索引的键
 * @param index 要删除的键的索引位置
 * 
 * 删除指定位置的键值对，对于内部节点还需要删除对应的子节点指针，
 * 删除后保持数组的连续性
 */
void BPlusTreeNode::removeKey(int index) {
    // 检查索引有效性
    if (index >= 0 && index < header.keyCount) {
        // 删除指定位置的键
        keys.erase(keys.begin() + index);
        header.keyCount--;                   // 减少键计数

        // 对于内部节点，删除对应的子节点指针
        if (!header.isLeaf && index < (int)children.size()) {
            // 删除键右侧的子节点指针
            children.erase(children.begin() + index + 1);
        }

        // 标记节点为脏状态（已修改）
        dirty = true;
    }
}

/**
 * @brief 分裂节点
 * @param newNode 新创建的节点，用于存放分裂后的右半部分数据
 * @param promotedKey 输出参数，返回需要上提到父节点的键
 * 
 * 将当前节点分裂为两个节点，当前节点保留左半部分，
 * 新节点存储右半部分，并确定需要上提到父节点的键
 */
void BPlusTreeNode::split(std::shared_ptr<BPlusTreeNode> newNode,
                          KeyValue& promotedKey) {
    // 使用更优化的分裂策略
    int totalKeys = header.keyCount;         // 总键数
    int mid;                                 // 分裂点

    if (header.isLeaf) {
        // 叶子节点：尽可能均匀分裂
        mid = (totalKeys + 1) / 2;          // 向上取整，右边可能多1个
    } else {
        // 内部节点：中间键要上提，所以分裂点不同
        mid = totalKeys / 2;                // 向下取整
    }

    if (header.isLeaf) {
        // 叶子节点分裂处理
        // 将右半部分的键复制到新节点
        for (int i = mid; i < header.keyCount; i++) {
            newNode->keys.push_back(keys[i]);
        }
        newNode->header.keyCount = header.keyCount - mid;

        // 维护叶子节点链表的连接关系
        newNode->header.nextLeafId = header.nextLeafId;  // 新节点指向原节点的下一个
        header.nextLeafId = newNode->header.pageId;      // 原节点指向新节点

        // 叶子节点分裂时，上提的键是右边第一个键的副本
        if (newNode->header.keyCount > 0) {
            promotedKey = newNode->keys[0];
        }
    } else {
        // 内部节点分裂处理
        if (mid < header.keyCount) {
            promotedKey = keys[mid];         // 中间键上提到父节点
        }

        // 右边节点获得mid+1到end的键（跳过上提的中间键）
        for (int i = mid + 1; i < header.keyCount; i++) {
            newNode->keys.push_back(keys[i]);
        }
        newNode->header.keyCount = header.keyCount - mid - 1;

        // 移动对应的子指针到新节点
        for (int i = mid + 1; i <= header.keyCount && i < (int)children.size();
             i++) {
            newNode->children.push_back(children[i]);
        }
    }

    // 调整当前节点大小，保留左半部分
    keys.resize(mid);
    header.keyCount = mid;
    if (!header.isLeaf) {
        children.resize(mid + 1);            // 内部节点保留mid+1个子指针
    }

    // 标记两个节点都为脏状态
    dirty = true;
    newNode->dirty = true;
}

// ================================ BPlusTree 实现
// ================================

/**
 * @brief BPlusTree 构造函数
 * 
 * 初始化B+树对象，设置初始状态
 */
BPlusTree::BPlusTree() {}

/**
 * @brief BPlusTree 析构函数
 * 
 * 清理资源，关闭文件和缓冲池
 */
BPlusTree::~BPlusTree() { 
    close();                                 // 确保资源被正确释放
}

/**
 * @brief 创建或打开B+树文件
 * @param fname 文件名
 * @param pageSize 页面大小（当前实现中未使用）
 * @param bufferPoolSize 缓冲池大小
 * @return true 成功，false 失败
 * 
 * 创建新的B+树文件或打开existing文件，初始化缓冲池和元数据
 */
bool BPlusTree::create(const std::string& fname, int pageSize,
                       size_t bufferPoolSize) {
    filename = fname;                        // 保存文件名

    // 初始化BufferPool，限制缓冲池大小以避免内存问题
    size_t maxBufferSize = std::min(bufferPoolSize, static_cast<size_t>(1000));
    bufferPool = std::make_unique<BufferPool>(maxBufferSize);

    // 设置缓冲池的保存回调函数，当页面需要写回磁盘时调用
    bufferPool->setSaveCallback(
        [this](std::shared_ptr<BPlusTreeNode> node) { this->savePage(node); });

    // 检查文件是否已存在
    std::ifstream testFile(filename);
    bool fileExists = testFile.good();      // 测试文件是否可读
    testFile.close();

    if (fileExists) {
        // 文件存在，以读写模式打开
        file.open(filename, std::ios::in | std::ios::out | std::ios::binary);
        if (file.is_open()) {
            loadMetadata();                  // 加载已有的元数据
            return true;
        }
        return false;                        // 文件打开失败
    } else {
        // 文件不存在，创建新文件
        file.open(filename, std::ios::out | std::ios::binary);
        if (!file.is_open()) {
            return false;                    // 文件创建失败
        }
        file.close();

        // 重新以读写模式打开
        file.open(filename, std::ios::in | std::ios::out | std::ios::binary);
        if (!file.is_open()) {
            return false;                    // 文件打开失败
        }

        // 初始化新的元数据
        metadata = Metadata();
        saveMetadata();                      // 保存初始元数据到文件
        return true;
    }
}

/**
 * @brief 关闭B+树，释放资源
 * 
 * 刷新所有缓冲页面到磁盘，保存元数据，关闭文件
 */
void BPlusTree::close() {
    if (bufferPool) {
        bufferPool->flushAllPages();         // 将所有脏页写回磁盘
        bufferPool.reset();                  // 释放缓冲池
    }
    if (file.is_open()) {
        saveMetadata();                      // 保存最新的元数据
        file.close();                        // 关闭文件
    }
}

/**
 * @brief 从磁盘加载页面
 * @param pageId 要加载的页面ID
 * @return 加载的节点指针，失败时返回nullptr
 * 
 * 通过缓冲池管理页面加载，如果页面不在缓冲池中则从磁盘读取
 */
std::shared_ptr<BPlusTreeNode> BPlusTree::loadPage(int pageId) {
    if (!bufferPool) {
        return nullptr;                      // 缓冲池未初始化
    }

    // 通过缓冲池获取页面，如果不存在则使用lambda函数加载
    auto node = bufferPool->getPage(
        pageId, [this, pageId]() -> std::shared_ptr<BPlusTreeNode> {
            // 创建新的节点对象
            auto newNode = std::make_shared<BPlusTreeNode>(pageId);

            // 计算文件位置，使用更安全的计算方式
            std::streampos filePos =
                METADATA_SIZE + static_cast<std::streampos>(pageId) * PAGE_SIZE;

            // 检查文件位置是否合理
            if (filePos < 0 || pageId < 0) {
                std::cerr << "Invalid page position: pageId=" << pageId
                          << ", pos=" << filePos << std::endl;
                return nullptr;
            }

            // 定位到文件中的页面位置
            this->file.seekg(filePos);
            if (!this->file.good()) {
                std::cerr << "Failed to seek to position: " << filePos
                          << std::endl;
                this->file.clear();          // 清除错误状态
                return newNode;              // 返回空节点而不是nullptr
            }

            // 读取页面数据
            char buffer[PAGE_SIZE];
            this->file.read(buffer, PAGE_SIZE);

            if (this->file.gcount() == PAGE_SIZE) {
                // 完整读取，反序列化数据
                newNode->deserialize(buffer);
            } else if (this->file.gcount() > 0) {
                // 部分读取，可能是文件末尾
                std::cout << "Partial read: " << this->file.gcount() << " bytes"
                          << std::endl;
            }

            return newNode;
        });

    return node;
}

/**
 * @brief 保存页面到磁盘
 * @param node 要保存的节点
 * 
 * 将节点数据序列化并写入磁盘文件的对应位置
 */
void BPlusTree::savePage(std::shared_ptr<BPlusTreeNode> node) {
    // 检查节点有效性和是否需要保存
    if (!node || !node->dirty) return;

    // 序列化节点数据到缓冲区
    char buffer[PAGE_SIZE];
    node->serialize(buffer);

    // 使用更安全的位置计算
    std::streampos filePos =
        METADATA_SIZE +
        static_cast<std::streampos>(node->header.pageId) * PAGE_SIZE;

    // 检查位置有效性
    if (filePos < 0 || node->header.pageId < 0) {
        std::cerr << "Invalid save position: pageId=" << node->header.pageId
                  << ", pos=" << filePos << std::endl;
        return;
    }

    // 定位到文件写入位置
    file.seekp(filePos);
    if (!file.good()) {
        std::cerr << "Failed to seek to save position: " << filePos
                  << std::endl;
        file.clear();                        // 清除错误状态
        return;
    }

    // 写入页面数据
    file.write(buffer, PAGE_SIZE);
    if (!file.good()) {
        std::cerr << "Failed to write page " << node->header.pageId
                  << " to disk" << std::endl;
        file.clear();                        // 清除错误状态
        return;
    }
    fileWriteCount++;                        // 增加写入计数

    file.flush();                            // 强制刷新到磁盘
    node->dirty = false;                     // 标记为干净状态
}

/**
 * @brief 创建新页面
 * @param isLeaf 是否为叶子节点
 * @return 新创建的节点指针，失败时返回nullptr
 * 
 * 分配新的页面ID，创建新节点，并加入缓冲池管理
 */
std::shared_ptr<BPlusTreeNode> BPlusTree::createNewPage(bool isLeaf) {
    // 检查页面ID是否会溢出
    if (metadata.nextPageId < 0 || metadata.nextPageId > 10000000) {
        std::cerr << "Page ID overflow or invalid: " << metadata.nextPageId
                  << std::endl;
        return nullptr;
    }

    // 分配新的页面ID
    int pageId = metadata.nextPageId++;
    // 创建新节点
    auto node = std::make_shared<BPlusTreeNode>(pageId, isLeaf);
    node->dirty = true;                      // 新节点需要保存

    // 将新节点加入缓冲池
    if (bufferPool) {
        bufferPool->putPage(pageId, node);
        bufferPool->markDirty(pageId);       // 标记为脏页
    }
    metadata.pageCount++;                    // 增加页面计数

    return node;
}

/**
 * @brief 保存元数据到文件
 * 
 * 将B+树的元数据（根页面ID、页面计数等）写入文件头部
 */
void BPlusTree::saveMetadata() {
    // 定位到文件开始位置
    file.seekp(0);
    if (!file.good()) {
        std::cerr << "Failed to seek to metadata position" << std::endl;
        file.clear();                        // 清除错误状态
        return;
    }

    // 写入元数据结构
    file.write(reinterpret_cast<const char*>(&metadata), sizeof(Metadata));
    if (!file.good()) {
        std::cerr << "Failed to write metadata" << std::endl;
        file.clear();                        // 清除错误状态
        return;
    }

    file.flush();                            // 强制刷新到磁盘
}

/**
 * @brief 从文件加载元数据
 * 
 * 从文件头部读取B+树的元数据信息
 */
void BPlusTree::loadMetadata() {
    // 定位到文件开始位置
    file.seekg(0);
    // 读取元数据结构
    file.read(reinterpret_cast<char*>(&metadata), sizeof(Metadata));

    // 验证元数据的合理性
    if (metadata.nextPageId < 0 || metadata.pageCount < 0) {
        std::cout << "Invalid metadata detected, reinitializing..."
                  << std::endl;
        metadata = Metadata();              // 重新初始化元数据
    }
}

/**
 * @brief 查找包含指定键的叶子节点
 * @param key 要查找的键
 * @return 包含该键的叶子节点指针，未找到时返回nullptr
 * 
 * 从根节点开始，沿着B+树向下查找，直到找到包含指定键的叶子节点
 */
std::shared_ptr<BPlusTreeNode> BPlusTree::findLeafNode(const std::string& key) {
    // 检查树是否为空
    if (metadata.rootPageId == -1) {
        return nullptr;                      // 空树
    }

    // 从根节点开始查找
    auto current = loadPage(metadata.rootPageId);

    // 向下遍历直到叶子节点
    while (current && !current->header.isLeaf) {
        // 在当前内部节点中查找键的位置
        int pos = current->findKey(key);

        // 如果找到相等的键，选择右子树
        if (pos < current->header.keyCount &&
            current->keys[pos].getKey() == key) {
            pos++;                           // 移动到右子树
        }

        // 检查子节点数组的有效性
        if (current->children.empty() || pos >= (int)current->children.size()) {
            return nullptr;                  // 子节点数组无效
        }
        if (pos < 0) {
            pos = 0;                         // 确保索引非负
        }

        // 检查子节点ID的有效性
        if (pos < 0 || current->children[pos] == -1) {
            return nullptr;                  // 无效的子节点ID
        }

        // 加载子节点并继续向下查找
        current = loadPage(current->children[pos]);
    }

    return current;                          // 返回找到的叶子节点
}

/**
 * @brief 插入键值对到B+树
 * @param key 键
 * @param value 值数组
 * @param rowId 行ID
 * @return true 插入成功，false 插入失败
 * 
 * 在B+树中插入新的键值对，如果键已存在则更新值，
 * 如果节点满了则进行分裂操作
 */
bool BPlusTree::insert(const std::string& key,
                       const std::vector<std::string>& value,
                       const std::string& rowId) {
    // 构造键值对象
    std::string val = value.empty() ? "" : value[0];
    KeyValue kv(key, rowId, val);

    // 处理空树的情况
    if (metadata.rootPageId == -1) {
        // 创建第一个节点作为根节点（叶子节点）
        auto root = createNewPage(true);
        if (!root) return false;             // 创建失败

        // 插入第一个键值对
        root->insertKey(kv);
        metadata.rootPageId = root->header.pageId;  // 设置根节点ID
        saveMetadata();                      // 保存元数据
        return true;
    }

    // 查找目标叶子节点
    auto leaf = findLeafNode(key);
    if (!leaf) return false;                 // 查找失败

    // 在叶子节点中查找键的位置
    int pos = leaf->findKey(key);
    if (pos < leaf->header.keyCount && leaf->keys[pos].getKey() == key) {
        // 键已存在，更新值（这是原来的正确行为）
        leaf->keys[pos] = kv;
        leaf->dirty = true;                  // 标记为脏页
        if (bufferPool) {
            bufferPool->markDirty(leaf->header.pageId);
        }
        return true;
    }

    // 插入新键
    leaf->insertKey(kv);
    if (bufferPool) {
        bufferPool->markDirty(leaf->header.pageId);  // 标记为脏页
    }

    // 每插入100个键清理一次缓冲池，控制内存使用
    // 先不管这个，不知道为什么变快了，按理说不应该
    // static int insertCount = 0;
    // if (++insertCount % 100 == 0 && bufferPool) {
    //     bufferPool->flushAllPages();
    // }

    // 检查是否需要分裂
    if (leaf->isFull()) {
        handleOverflow(leaf);                // 处理节点溢出
    }

    return true;
}

/**
 * @brief 处理节点溢出（分裂）
 * @param node 发生溢出的节点
 * 
 * 处理节点分裂的连锁反应，使用迭代方式避免递归调用栈溢出
 */
void BPlusTree::handleOverflow(std::shared_ptr<BPlusTreeNode> node) {
    // 使用队列存储需要处理的溢出节点
    std::vector<std::shared_ptr<BPlusTreeNode>> overflowNodes;
    overflowNodes.push_back(node);

    // 使用循环而不是递归处理溢出，避免栈溢出
    while (!overflowNodes.empty()) {
        auto currentNode = overflowNodes.back();
        overflowNodes.pop_back();

        // 检查节点是否真的需要分裂
        if (!currentNode || !currentNode->isFull()) continue;

        // 创建新节点用于存储分裂后的右半部分
        auto newNode = createNewPage(currentNode->header.isLeaf);
        if (!newNode) continue;              // 创建失败，跳过

        // 执行节点分裂
        KeyValue promotedKey;
        currentNode->split(newNode, promotedKey);
        metadata.splitCount++;               // 增加分裂计数

        // 标记相关页面为脏页
        if (bufferPool) {
            bufferPool->markDirty(currentNode->header.pageId);
            bufferPool->markDirty(newNode->header.pageId);
        }

        // 处理根节点分裂的特殊情况
        if (currentNode->header.pageId == metadata.rootPageId) {
            // 创建新根节点
            auto newRoot = createNewPage(false);  // 内部节点
            if (!newRoot) continue;          // 创建失败，跳过

            // 设置新根节点的子节点和键
            newRoot->children.push_back(currentNode->header.pageId);
            newRoot->children.push_back(newNode->header.pageId);
            newRoot->keys.push_back(promotedKey);
            newRoot->header.keyCount = 1;

            // 更新子节点的父节点引用
            currentNode->header.parentId = newRoot->header.pageId;
            newNode->header.parentId = newRoot->header.pageId;

            // 更新根节点ID
            metadata.rootPageId = newRoot->header.pageId;
            saveMetadata();

            if (bufferPool) {
                bufferPool->markDirty(newRoot->header.pageId);
            }
        } else {
            // 非根节点分裂，需要向父节点插入上提的键
            auto parent = loadPage(currentNode->header.parentId);
            if (parent) {
                // 设置新节点的父节点
                newNode->header.parentId = parent->header.pageId;
                // 向父节点插入上提的键
                insertInternal(parent, promotedKey, newNode->header.pageId);

                // 如果父节点也满了，加入队列处理
                if (parent->isFull()) {
                    overflowNodes.push_back(parent);
                }
            }
        }
    }
}

/**
 * @brief 向内部节点插入键值对
 * @param node 目标内部节点
 * @param kv 要插入的键值对
 * @param rightChildId 键右侧的子节点ID
 * 
 * 在内部节点中插入键值对和对应的子节点指针
 */
void BPlusTree::insertInternal(std::shared_ptr<BPlusTreeNode> node,
                               const KeyValue& kv, int rightChildId) {
    // 检查节点有效性
    if (!node || node->header.isLeaf) return;

    // 找到插入位置
    int pos = node->findKey(kv.getKey());

    // 插入键值对
    node->keys.insert(node->keys.begin() + pos, kv);
    node->header.keyCount++;

    // 插入右子节点指针
    if (rightChildId != -1) {
        node->children.insert(node->children.begin() + pos + 1, rightChildId);
    }

    // 标记为脏页
    node->dirty = true;
    if (bufferPool) {
        bufferPool->markDirty(node->header.pageId);
    }
}

/**
 * @brief 获取指定键的所有值
 * @param key 要查找的键
 * @return 包含所有匹配值的二维向量
 * 
 * 在B+树中查找指定键的所有值，返回匹配的结果集
 */
std::vector<std::vector<std::string>> BPlusTree::get(const std::string& key) {
    std::vector<std::vector<std::string>> result;

    // 查找包含该键的叶子节点
    auto leaf = findLeafNode(key);
    if (!leaf) return result;                // 未找到

    // 在叶子节点中查找所有匹配的键值对
    for (int i = 0; i < leaf->header.keyCount; i++) {
        if (leaf->keys[i].getKey() == key) {
            std::vector<std::string> values;
            values.push_back(leaf->keys[i].getValue());
            result.push_back(values);        // 添加到结果集
        }
    }

    return result;
}

/**
 * @brief 从B+树中删除指定键
 * @param key 要删除的键
 * @return true 删除成功，false 键不存在
 * 
 * 删除指定键，如果删除后节点过小则进行合并或重分布操作
 */
bool BPlusTree::remove(const std::string& key) {
    // 查找包含该键的叶子节点
    auto leaf = findLeafNode(key);
    if (!leaf) return false;                 // 键不存在

    // 在叶子节点中查找键的位置
    int pos = leaf->findKey(key);
    if (pos >= leaf->header.keyCount || leaf->keys[pos].getKey() != key) {
        return false;                        // 键不存在
    }

    // 删除键
    leaf->removeKey(pos);
    if (bufferPool) {
        bufferPool->markDirty(leaf->header.pageId);  // 标记为脏页
    }

    // 检查是否需要处理下溢（节点过小）
    int minKeys = MAX_KEYS_PER_PAGE / 2;
    if (leaf->header.keyCount < minKeys &&
        leaf->header.pageId != metadata.rootPageId) {
        handleUnderflow(leaf);               // 处理节点下溢
    }

    return true;
}

/**
 * @brief 处理节点下溢（合并或重分布）
 * @param node 发生下溢的节点
 * 
 * 当节点中的键数量少于最小要求时，通过合并或重分布来维护B+树的性质
 */
void BPlusTree::handleUnderflow(std::shared_ptr<BPlusTreeNode> node) {
    if (!node) return;

    int minKeys = MAX_KEYS_PER_PAGE / 2;

    // 检查节点是否真的需要处理下溢
    if (node->header.keyCount >= minKeys) {
        return;                              // 键数量足够，无需处理
    }

    // 处理根节点的特殊情况
    if (node->header.pageId == metadata.rootPageId) {
        // 如果根节点是内部节点且为空，则将其唯一子节点提升为新根
        if (!node->header.isLeaf && node->header.keyCount == 0 &&
            !node->children.empty() && node->children[0] != -1) {
            metadata.rootPageId = node->children[0];
            auto newRoot = loadPage(metadata.rootPageId);
            if (newRoot) {
                newRoot->header.parentId = -1;  // 新根无父节点
                if (bufferPool) {
                    bufferPool->markDirty(newRoot->header.pageId);
                }
            }
            saveMetadata();
            metadata.pageCount--;            // 减少页面计数
        }
        return;
    }

    // 加载父节点
    auto parent = loadPage(node->header.parentId);
    if (!parent) return;

    // 在父节点中找到当前节点的位置
    int nodeIndex = -1;
    for (int i = 0; i < (int)parent->children.size(); i++) {
        if (parent->children[i] == node->header.pageId) {
            nodeIndex = i;
            break;
        }
    }
    if (nodeIndex == -1) return;             // 未找到节点位置

    // 尝试从左兄弟节点借键
    if (nodeIndex > 0) {
        auto leftSibling = loadPage(parent->children[nodeIndex - 1]);
        if (leftSibling && leftSibling->header.keyCount > minKeys) {
            redistributeFromLeft(node, leftSibling, parent, nodeIndex - 1);
            return;
        }
    }

    // 尝试从右兄弟节点借键
    if (nodeIndex < (int)parent->children.size() - 1) {
        auto rightSibling = loadPage(parent->children[nodeIndex + 1]);
        if (rightSibling && rightSibling->header.keyCount > minKeys) {
            redistributeFromRight(node, rightSibling, parent, nodeIndex);
            return;
        }
    }

    // 无法借键，尝试与左兄弟合并
    if (nodeIndex > 0) {
        auto leftSibling = loadPage(parent->children[nodeIndex - 1]);
        if (leftSibling) {
            mergeNodes(leftSibling, node, parent, nodeIndex - 1);
            return;
        }
    }

    // 尝试与右兄弟合并
    if (nodeIndex < (int)parent->children.size() - 1) {
        auto rightSibling = loadPage(parent->children[nodeIndex + 1]);
        if (rightSibling) {
            mergeNodes(node, rightSibling, parent, nodeIndex);
            return;
        }
    }
}

/**
 * @brief 从左兄弟节点重分布键
 * @param node 目标节点
 * @param leftSibling 左兄弟节点
 * @param parent 父节点
 * @param parentKeyIndex 父节点中的键索引
 * 
 * 从左兄弟节点借一个键来增加目标节点的键数量
 */
void BPlusTree::redistributeFromLeft(std::shared_ptr<BPlusTreeNode> node,
                                     std::shared_ptr<BPlusTreeNode> leftSibling,
                                     std::shared_ptr<BPlusTreeNode> parent,
                                     int parentKeyIndex) {
    if (node->header.isLeaf) {
        // 叶子节点重分布
        // 将左兄弟的最后一个键移动到当前节点的开头
        node->keys.insert(node->keys.begin(), leftSibling->keys.back());
        leftSibling->keys.pop_back();
        leftSibling->header.keyCount--;
        node->header.keyCount++;

        // 更新父节点中的分隔键为当前节点的第一个键
        parent->keys[parentKeyIndex] = node->keys[0];
    } else {
        // 内部节点重分布
        // 将父节点的键下降到当前节点
        node->keys.insert(node->keys.begin(), parent->keys[parentKeyIndex]);
        node->header.keyCount++;

        // 将左兄弟的最后一个键上升到父节点
        parent->keys[parentKeyIndex] = leftSibling->keys.back();
        leftSibling->keys.pop_back();
        leftSibling->header.keyCount--;

        // 移动对应的子节点指针
        node->children.insert(node->children.begin(),
                              leftSibling->children.back());
        leftSibling->children.pop_back();

        // 更新移动的子节点的父节点引用
        if (node->children[0] != -1) {
            auto child = loadPage(node->children[0]);
            if (child) {
                child->header.parentId = node->header.pageId;
                if (bufferPool) {
                    bufferPool->markDirty(child->header.pageId);
                }
            }
        }
    }

    // 标记相关节点为脏页
    if (bufferPool) {
        bufferPool->markDirty(node->header.pageId);
        bufferPool->markDirty(leftSibling->header.pageId);
        bufferPool->markDirty(parent->header.pageId);
    }
}

/**
 * @brief 从右兄弟节点重分布键
 * @param node 目标节点
 * @param rightSibling 右兄弟节点
 * @param parent 父节点
 * @param parentKeyIndex 父节点中的键索引
 * 
 * 从右兄弟节点借一个键来增加目标节点的键数量
 */
void BPlusTree::redistributeFromRight(
    std::shared_ptr<BPlusTreeNode> node,
    std::shared_ptr<BPlusTreeNode> rightSibling,
    std::shared_ptr<BPlusTreeNode> parent, int parentKeyIndex) {
    if (node->header.isLeaf) {
        // 叶子节点重分布
        // 将右兄弟的第一个键移动到当前节点的末尾
        node->keys.push_back(rightSibling->keys[0]);
        rightSibling->keys.erase(rightSibling->keys.begin());
        rightSibling->header.keyCount--;
        node->header.keyCount++;

        // 更新父节点中的分隔键为右兄弟的第一个键
        parent->keys[parentKeyIndex] = rightSibling->keys[0];
    } else {
        // 内部节点重分布
        // 将父节点的键下降到当前节点
        node->keys.push_back(parent->keys[parentKeyIndex]);
        node->header.keyCount++;

        // 将右兄弟的第一个键上升到父节点
        parent->keys[parentKeyIndex] = rightSibling->keys[0];
        rightSibling->keys.erase(rightSibling->keys.begin());
        rightSibling->header.keyCount--;

        // 移动对应的子节点指针
        node->children.push_back(rightSibling->children[0]);
        rightSibling->children.erase(rightSibling->children.begin());

        // 更新移动的子节点的父节点引用
        if (node->children.back() != -1) {
            auto child = loadPage(node->children.back());
            if (child) {
                child->header.parentId = node->header.pageId;
                if (bufferPool) {
                    bufferPool->markDirty(child->header.pageId);
                }
            }
        }
    }

    // 标记相关节点为脏页
    if (bufferPool) {
        bufferPool->markDirty(node->header.pageId);
        bufferPool->markDirty(rightSibling->header.pageId);
        bufferPool->markDirty(parent->header.pageId);
    }
}

/**
 * @brief 合并两个节点
 * @param leftNode 左节点
 * @param rightNode 右节点
 * @param parent 父节点
 * @param parentKeyIndex 父节点中要删除的键索引
 * 
 * 将右节点的所有键和子节点合并到左节点中，然后删除右节点
 */
void BPlusTree::mergeNodes(std::shared_ptr<BPlusTreeNode> leftNode,
                           std::shared_ptr<BPlusTreeNode> rightNode,
                           std::shared_ptr<BPlusTreeNode> parent,
                           int parentKeyIndex) {
    if (leftNode->header.isLeaf) {
        // 叶子节点合并
        // 将右节点的所有键复制到左节点
        for (const auto& key : rightNode->keys) {
            leftNode->keys.push_back(key);
        }
        leftNode->header.keyCount += rightNode->header.keyCount;

        // 维护叶子节点链表的连接关系
        leftNode->header.nextLeafId = rightNode->header.nextLeafId;
    } else {
        // 内部节点合并
        // 将父节点的键下降到左节点
        leftNode->keys.push_back(parent->keys[parentKeyIndex]);
        leftNode->header.keyCount++;

        // 将右节点的所有键复制到左节点
        for (const auto& key : rightNode->keys) {
            leftNode->keys.push_back(key);
        }
        leftNode->header.keyCount += rightNode->header.keyCount;

        // 将右节点的所有子节点指针复制到左节点
        for (int childId : rightNode->children) {
            leftNode->children.push_back(childId);
            // 更新子节点的父节点引用
            if (childId != -1) {
                auto child = loadPage(childId);
                if (child) {
                    child->header.parentId = leftNode->header.pageId;
                    if (bufferPool) {
                        bufferPool->markDirty(child->header.pageId);
                    }
                }
            }
        }
    }

    // 从父节点删除对应的键
    parent->removeKey(parentKeyIndex);

    // 标记相关节点为脏页
    if (bufferPool) {
        bufferPool->markDirty(leftNode->header.pageId);
        bufferPool->markDirty(parent->header.pageId);
    }

    // 更新统计信息
    metadata.pageCount--;                    // 减少页面计数
    metadata.mergeCount++;                   // 增加合并计数

    // 检查父节点是否需要处理下溢
    if (parent->header.keyCount < MAX_KEYS_PER_PAGE / 2) {
        handleUnderflow(parent);
    }
}

/**
 * @brief 获取B+树统计信息
 * @return TreeStats 结构，包含树的各种统计信息
 * 
 * 计算并返回B+树的高度、节点数、填充因子等统计信息
 */
TreeStats BPlusTree::getStat() {
    TreeStats stats;

    // 检查树是否为空
    if (metadata.rootPageId == -1) {
        return stats;                        // 返回默认统计信息
    }

    // 加载根节点并计算统计信息
    auto root = loadPage(metadata.rootPageId);
    if (root) {
        stats.height = calculateHeight(root);         // 计算树高度
        stats.nodeCount = metadata.pageCount;        // 节点总数
        stats.splitCount = metadata.splitCount;      // 分裂次数
        stats.mergeCount = metadata.mergeCount;      // 合并次数
        stats.fillFactor = calculateFillFactor();    // 填充因子
        stats.fileWriteCount = fileWriteCount;       // 文件写入次数
    }

    return stats;
}

/**
 * @brief 计算B+树的高度
 * @param node 起始节点（通常是根节点）
 * @return 树的高度
 * 
 * 从给定节点开始，沿着最左路径计算到叶子节点的高度
 */
int BPlusTree::calculateHeight(std::shared_ptr<BPlusTreeNode> node) {
    if (!node) return 0;                     // 空节点高度为0

    int height = 0;
    auto current = node;

    // 使用迭代而不是递归，沿着最左路径向下
    while (current && !current->header.isLeaf) {
        height++;                            // 增加高度计数
        // 检查子节点的有效性
        if (current->children.empty() || current->children[0] == -1) {
            break;                           // 无有效子节点，停止
        }
        current = loadPage(current->children[0]);  // 移动到第一个子节点
    }

    return height + 1;                       // +1 for leaf level
}

/**
 * @brief 计算B+树的填充因子
 * @return 填充因子（0.0到1.0之间的值）
 * 
 * 遍历整个树，计算所有节点的平均填充程度
 */
double BPlusTree::calculateFillFactor() {
    // 检查树是否为空
    if (metadata.pageCount == 0 || metadata.rootPageId == -1) {
        return 0.0;                          // 空树的填充因子为0
    }

    int totalKeys = 0;                       // 总键数
    int totalCapacity = 0;                   // 总容量

    // 使用队列进行层次遍历
    std::queue<int> nodeQueue;
    nodeQueue.push(metadata.rootPageId);

    while (!nodeQueue.empty()) {
        int pageId = nodeQueue.front();
        nodeQueue.pop();

        // 加载当前节点
        auto node = loadPage(pageId);
        if (!node) continue;                 // 加载失败，跳过

        // 累计键数和容量
        totalKeys += node->header.keyCount;
        totalCapacity += MAX_KEYS_PER_PAGE;

        // 对于内部节点，将子节点加入队列
        if (!node->header.isLeaf) {
            for (int i = 0;
                 i <= node->header.keyCount && i < (int)node->children.size();
                 i++) {
                if (node->children[i] != -1) {
                    nodeQueue.push(node->children[i]);
                }
            }
        }
    }

    // 计算填充因子
    return totalCapacity > 0 ? (double)totalKeys / totalCapacity : 0.0;
}

/**
 * @brief 设置缓冲池大小
 * @param size 新的缓冲池大小
 * 
 * 创建新的缓冲池并迁移数据，刷新旧缓冲池中的所有页面
 */
void BPlusTree::setBufferPoolSize(size_t size) {
    if (bufferPool) {
        // 创建新的缓冲池
        auto newBufferPool = std::make_unique<BufferPool>(size);
        // 设置保存回调函数
        newBufferPool->setSaveCallback(
            [this](std::shared_ptr<BPlusTreeNode> node) {
                this->savePage(node);
            });

        // 刷新旧缓冲池中的所有页面
        bufferPool->flushAllPages();
        // 切换到新缓冲池
        bufferPool = std::move(newBufferPool);
    }
}

/**
 * @brief 获取缓冲池统计信息
 * @return BufferPool::Stats 缓冲池统计信息
 * 
 * 返回缓冲池的命中率、页面数等统计信息
 */
BufferPool::Stats BPlusTree::getBufferPoolStats() const {
    if (bufferPool) {
        return bufferPool->getStats();       // 返回缓冲池统计信息
    }
    return BufferPool::Stats();              // 返回默认统计信息
}

/**
 * @brief 刷新缓冲池中的所有页面
 * @return 刷新的页面数量
 * 
 * 强制将缓冲池中的所有脏页写回磁盘
 */
int BPlusTree::flushBuffer() {
    if (bufferPool) {
        return bufferPool->flushAllPages();  // 刷新所有页面
    }
    return 0;                               // 无缓冲池，返回0
}

/**
 * @brief 打印缓冲池状态信息
 * 
 * 输出缓冲池的当前状态，用于调试和监控
 */
void BPlusTree::printBufferPoolStatus() const {
    if (bufferPool) {
        bufferPool->printStatus();           // 打印缓冲池状态
    }
}

/**
 * @brief 打印整个B+树结构
 * 
 * 以层次结构方式打印整个B+树，用于调试和可视化
 */
void BPlusTree::printTree() {
    // 检查树是否为空
    if (metadata.rootPageId == -1) {
        std::cout << "Empty tree" << std::endl;
        return;
    }

    // 从根节点开始打印
    auto root = loadPage(metadata.rootPageId);
    printNode(root, 0);                      // 从第0层开始打印
}

/**
 * @brief 递归打印节点及其子树
 * @param node 要打印的节点
 * @param level 当前层次（用于缩进）
 * 
 * 递归打印节点信息和其所有子节点，形成树状结构显示
 */
void BPlusTree::printNode(std::shared_ptr<BPlusTreeNode> node, int level) {
    if (!node) return;                       // 空节点，直接返回

    // 根据层次打印缩进
    for (int i = 0; i < level; i++) {
        std::cout << "  ";                   // 每层缩进2个空格
    }

    // 打印节点基本信息
    std::cout << "Page " << node->header.pageId << " ("
              << (node->header.isLeaf ? "Leaf" : "Internal")
              << ", Keys: " << node->header.keyCount << "): ";

    // 打印节点中的所有键
    for (int i = 0; i < node->header.keyCount; i++) {
        std::cout << node->keys[i].getKey() << " ";
    }
    std::cout << std::endl;

    // 对于内部节点，递归打印所有子节点
    if (!node->header.isLeaf) {
        for (int i = 0;
             i <= node->header.keyCount && i < (int)node->children.size();
             i++) {
            if (node->children[i] != -1) {
                auto child = loadPage(node->children[i]);
                printNode(child, level + 1); // 递归打印子节点，层次+1
            }
        }
    }
}