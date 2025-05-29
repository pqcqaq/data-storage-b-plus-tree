#include "BufferPool.h"
#include "BPlusTree.h" 
#include <iostream>
#include <algorithm>

/**
 * @brief BufferPool构造函数
 * @param maxSize 缓冲池最大页面数
 * @param saveCallback 页面保存回调函数
 */
BufferPool::BufferPool(size_t maxSize, std::function<void(std::shared_ptr<BPlusTreeNode>)> saveCallback)
    : maxSize_(maxSize), saveCallback_(saveCallback), hitCount_(0), missCount_(0) {
    if (maxSize_ == 0) {
        maxSize_ = 100;  // 默认最小值
    }
}

/**
 * @brief BufferPool析构函数
 * 确保所有脏页都被写回磁盘
 */
BufferPool::~BufferPool() {
    flushAllPages();
    clear();
}

/**
 * @brief 获取页面
 * @param pageId 页面ID
 * @param loadCallback 页面加载回调函数
 * @return 页面节点
 */
std::shared_ptr<BPlusTreeNode> BufferPool::getPage(int pageId, 
                                                  std::function<std::shared_ptr<BPlusTreeNode>()> loadCallback) {
    auto it = pages_.find(pageId);
    
    if (it != pages_.end()) {
        // 缓存命中
        hitCount_++;
        updateLRU(pageId);
        return it->second.node;
    }
    
    // 缓存未命中
    missCount_++;
    
    // 如果提供了加载回调，尝试加载页面
    std::shared_ptr<BPlusTreeNode> node = nullptr;
    if (loadCallback) {
        node = loadCallback();
    }
    
    if (node) {
        putPage(pageId, node);
        return node;
    }
    
    return nullptr;
}

/**
 * @brief 将页面放入缓冲池
 * @param pageId 页面ID
 * @param node 页面节点
 */
void BufferPool::putPage(int pageId, std::shared_ptr<BPlusTreeNode> node) {
    if (!node) return;
    
    auto it = pages_.find(pageId);
    if (it != pages_.end()) {
        // 页面已存在，更新并移到最前面
        it->second.node = node;
        updateLRU(pageId);
        return;
    }
    
    // 检查是否需要淘汰页面
    while (pages_.size() >= maxSize_) {
        int evictedPageId = evictLRU();
        if (evictedPageId == -1) {
            // 无法淘汰任何页面（所有页面都被固定或都是脏页）
            // 强制淘汰一个脏页
            evictedPageId = forceEvictDirtyPage();
            if (evictedPageId == -1) {
                // 仍然无法淘汰，可能所有页面都被固定
                std::cerr << "Warning: BufferPool is full and cannot evict any page" << std::endl;
                return;
            }
        }
    }
    
    // 添加新页面
    BufferPoolItem item(node, pageId);
    pages_.emplace(pageId, std::move(item));
    
    // 添加到LRU链表前端
    lruList_.push_front(pageId);
    lruMap_[pageId] = lruList_.begin();
}

/**
 * @brief 标记页面为脏页
 * @param pageId 页面ID
 */
void BufferPool::markDirty(int pageId) {
    auto it = pages_.find(pageId);
    if (it != pages_.end()) {
        it->second.dirty = true;
        if (it->second.node) {
            it->second.node->dirty = true;
        }
        updateLRU(pageId);
    }
}

/**
 * @brief 固定页面
 * @param pageId 页面ID
 */
void BufferPool::pinPage(int pageId) {
    auto it = pages_.find(pageId);
    if (it != pages_.end()) {
        it->second.pinned = true;
        updateLRU(pageId);
    }
}

/**
 * @brief 取消固定页面
 * @param pageId 页面ID
 */
void BufferPool::unpinPage(int pageId) {
    auto it = pages_.find(pageId);
    if (it != pages_.end()) {
        it->second.pinned = false;
    }
}

/**
 * @brief 刷新指定页面到磁盘
 * @param pageId 页面ID
 * @return true如果成功
 */
bool BufferPool::flushPage(int pageId) {
    auto it = pages_.find(pageId);
    if (it == pages_.end()) {
        return false;
    }
    
    BufferPoolItem& item = it->second;
    if (item.dirty && item.node && saveCallback_) {
        saveCallback_(item.node);
        item.dirty = false;
        item.node->dirty = false;
        return true;
    }
    
    return true;  // 非脏页也算成功
}

/**
 * @brief 刷新所有脏页到磁盘
 * @return 刷新的页面数量
 */
int BufferPool::flushAllPages() {
    int flushedCount = 0;
    
    for (auto& pair : pages_) {
        BufferPoolItem& item = pair.second;
        if (item.dirty && item.node && saveCallback_) {
            saveCallback_(item.node);
            item.dirty = false;
            item.node->dirty = false;
            flushedCount++;
        }
    }
    
    return flushedCount;
}

/**
 * @brief 从缓冲池中移除页面
 * @param pageId 页面ID
 * @return true如果成功移除
 */
bool BufferPool::removePage(int pageId) {
    return removePageInternal(pageId, false);
}

/**
 * @brief 清空缓冲池
 */
void BufferPool::clear() {
    flushAllPages();
    pages_.clear();
    lruMap_.clear();
    lruList_.clear();
}

/**
 * @brief 设置页面保存回调函数
 * @param callback 保存回调函数
 */
void BufferPool::setSaveCallback(std::function<void(std::shared_ptr<BPlusTreeNode>)> callback) {
    saveCallback_ = callback;
}

/**
 * @brief 获取缓冲池统计信息
 */
BufferPool::Stats BufferPool::getStats() const {
    Stats stats;
    stats.totalPages = pages_.size();
    stats.maxSize = maxSize_;
    stats.hitCount = hitCount_;
    stats.missCount = missCount_;
    
    // 计算命中率
    long long totalAccess = hitCount_ + missCount_;
    if (totalAccess > 0) {
        stats.hitRatio = (double)hitCount_ / totalAccess;
    }
    
    // 统计脏页和固定页
    for (const auto& pair : pages_) {
        const BufferPoolItem& item = pair.second;
        if (item.dirty) {
            stats.dirtyPages++;
        }
        if (item.pinned) {
            stats.pinnedPages++;
        }
    }
    
    return stats;
}

/**
 * @brief 打印缓冲池状态
 */
void BufferPool::printStatus() const {
    Stats stats = getStats();
    
    std::cout << "=== BufferPool Status ===" << std::endl;
    std::cout << "总页面数: " << stats.totalPages << "/" << stats.maxSize << std::endl;
    std::cout << "脏页数量: " << stats.dirtyPages << std::endl;
    std::cout << "固定页面数: " << stats.pinnedPages << std::endl;
    std::cout << "命中次数: " << stats.hitCount << std::endl;
    std::cout << "未命中次数: " << stats.missCount << std::endl;
    std::cout << "命中率: " << stats.hitRatio * 100 << "%" << std::endl;
    std::cout << "========================" << std::endl;
    std::cout << "当前LRU链表大小: " << lruList_.size() << std::endl;
    if (lruList_.empty()) {
        std::cout << "LRU Order: (empty)" << std::endl;
        return;
    }
    // 打印LRU链表中的页面ID
    std::cout << "当前LRU链表: ";
    for (const int pageId : lruList_) {
        std::cout << pageId << " ";
    }
    std::cout << std::endl;
    std::cout << "========================" << std::endl;
}

/**
 * @brief 更新页面的LRU位置
 * @param pageId 页面ID
 */
void BufferPool::updateLRU(int pageId) {
    auto lruIt = lruMap_.find(pageId);
    if (lruIt != lruMap_.end()) {
        // 从当前位置移除
        lruList_.erase(lruIt->second);
    }
    
    // 添加到前端
    lruList_.push_front(pageId);
    lruMap_[pageId] = lruList_.begin();
}

/**
 * @brief 移除LRU链表中最久未使用的页面
 * @return 被移除的页面ID，如果无法移除返回-1
 */
int BufferPool::evictLRU() {
    // 从链表尾部开始查找可以淘汰的页面
    for (auto it = lruList_.rbegin(); it != lruList_.rend(); ++it) {
        int pageId = *it;
        auto pageIt = pages_.find(pageId);
        
        if (pageIt != pages_.end()) {
            const BufferPoolItem& item = pageIt->second;
            
            // 跳过被固定的页面和脏页
            if (!item.pinned && !item.dirty) {
                // 可以安全移除这个页面
                if (removePageInternal(pageId, false)) {
                    return pageId;
                }
            }
        }
    }
    
    return -1;  // 无法找到可淘汰的页面
}

/**
 * @brief 强制刷新并移除最久未使用的脏页
 * @return 被移除的页面ID，如果失败返回-1
 */
int BufferPool::forceEvictDirtyPage() {
    // 从链表尾部开始查找非固定的脏页
    for (auto it = lruList_.rbegin(); it != lruList_.rend(); ++it) {
        int pageId = *it;
        auto pageIt = pages_.find(pageId);
        
        if (pageIt != pages_.end()) {
            const BufferPoolItem& item = pageIt->second;
            
            // 找到非固定的脏页
            if (!item.pinned && item.dirty) {
                // 先刷新到磁盘
                if (flushPage(pageId)) {
                    // 然后移除
                    if (removePageInternal(pageId, false)) {
                        return pageId;
                    }
                }
            }
        }
    }
    
    return -1;  // 无法强制淘汰
}

/**
 * @brief 内部移除页面的实现
 * @param pageId 页面ID
 * @param force 是否强制移除
 * @return true如果成功
 */
bool BufferPool::removePageInternal(int pageId, bool force) {
    auto it = pages_.find(pageId);
    if (it == pages_.end()) {
        return false;
    }
    
    const BufferPoolItem& item = it->second;
    
    // 检查是否可以移除
    if (!force) {
        if (item.pinned) {
            return false;  // 被固定的页面不能移除
        }
        
        if (item.dirty) {
            // 脏页需要先刷新
            if (!flushPage(pageId)) {
                return false;
            }
        }
    }
    
    // 从LRU链表和映射中移除
    auto lruIt = lruMap_.find(pageId);
    if (lruIt != lruMap_.end()) {
        lruList_.erase(lruIt->second);
        lruMap_.erase(lruIt);
    }
    
    // 从页面映射中移除
    pages_.erase(it);
    
    return true;
}