#pragma once

#include <fstream>
#include <functional>
#include <list>
#include <memory>
#include <unordered_map>

// 前向声明
class BPlusTreeNode;

/**
 * @brief BufferPool页面项
 * 包含页面数据、访问信息和状态标记
 */
struct BufferPoolItem {
    std::shared_ptr<BPlusTreeNode> node;  // 页面节点
    bool dirty;                           // 脏页标记
    bool pinned;                          // 是否被固定（不能被淘汰）
    int pageId;                           // 页面ID

    BufferPoolItem(std::shared_ptr<BPlusTreeNode> n, int id)
        : node(n), dirty(false), pinned(false), pageId(id) {}
};

/**
 * @brief 基于LRU策略的缓冲池
 *
 * 功能特性：
 * - 固定大小的页面缓存，防止内存无限增长
 * - LRU淘汰策略，优先淘汰最久未使用的页面
 * - 脏页管理，确保数据一致性
 * - 页面固定机制，防止正在使用的页面被淘汰
 * - 批量刷盘功能，提高I/O效率
 */
class BufferPool {
   public:
    /**
     * @brief 构造函数
     * @param maxSize 缓冲池最大页面数，默认100页
     * @param saveCallback 页面保存回调函数
     */
    explicit BufferPool(size_t maxSize = 100,
                        std::function<void(std::shared_ptr<BPlusTreeNode>)>
                            saveCallback = nullptr);
    /**
     * @brief 析构函数
     * 自动刷新所有脏页到磁盘
     */
    ~BufferPool();

    /**
     * @brief 获取页面
     * @param pageId 页面ID
     * @param loadCallback 页面加载回调函数
     * @return 页面节点，如果失败返回nullptr
     */
    std::shared_ptr<BPlusTreeNode> getPage(
        int pageId,
        std::function<std::shared_ptr<BPlusTreeNode>()> loadCallback = nullptr);

    /**
     * @brief 将页面放入缓冲池
     * @param pageId 页面ID
     * @param node 页面节点
     */
    void putPage(int pageId, std::shared_ptr<BPlusTreeNode> node);

    /**
     * @brief 标记页面为脏页
     * @param pageId 页面ID
     */
    void markDirty(int pageId);

    /**
     * @brief 固定页面（防止被淘汰）
     * @param pageId 页面ID
     */
    void pinPage(int pageId);

    /**
     * @brief 取消固定页面
     * @param pageId 页面ID
     */
    void unpinPage(int pageId);

    /**
     * @brief 刷新指定页面到磁盘
     * @param pageId 页面ID
     * @return true如果成功，false如果页面不存在
     */
    bool flushPage(int pageId);

    /**
     * @brief 刷新所有脏页到磁盘
     * @return 刷新的页面数量
     */
    int flushAllPages();

    /**
     * @brief 从缓冲池中移除页面
     * @param pageId 页面ID
     * @return true如果成功移除，false如果页面不存在或被固定
     */
    bool removePage(int pageId);

    /**
     * @brief 清空缓冲池
     * 会先刷新所有脏页，然后清空缓存
     */
    void clear();

    /**
     * @brief 设置页面保存回调函数
     * @param callback 保存回调函数
     */
    void setSaveCallback(
        std::function<void(std::shared_ptr<BPlusTreeNode>)> callback);

    /**
     * @brief 获取缓冲池统计信息
     */
    struct Stats {
        size_t totalPages;    // 当前缓存的页面数
        size_t dirtyPages;    // 脏页数量
        size_t pinnedPages;   // 被固定的页面数
        size_t maxSize;       // 最大容量
        long long hitCount;   // 命中次数
        long long missCount;  // 未命中次数
        double hitRatio;      // 命中率

        Stats()
            : totalPages(0),
              dirtyPages(0),
              pinnedPages(0),
              maxSize(0),
              hitCount(0),
              missCount(0),
              hitRatio(0.0) {}
    };

    Stats getStats() const;

    /**
     * @brief 打印缓冲池状态（调试用）
     */
    void printStatus() const;

   private:
    // LRU链表：最近使用的在前，最久未使用的在后
    using LRUList = std::list<int>;
    using LRUIterator = LRUList::iterator;

    // 页面ID到缓冲项的映射
    std::unordered_map<int, BufferPoolItem> pages_;

    // 页面ID到LRU链表位置的映射
    std::unordered_map<int, LRUIterator> lruMap_;

    // LRU链表
    LRUList lruList_;

    // 配置参数
    size_t maxSize_;  // 最大页面数
    std::function<void(std::shared_ptr<BPlusTreeNode>)>
        saveCallback_;  // 保存回调

    // 统计信息
    mutable long long hitCount_;   // 命中次数
    mutable long long missCount_;  // 未命中次数

    /**
     * @brief 更新页面的LRU位置
     * @param pageId 页面ID
     */
    void updateLRU(int pageId);

    /**
     * @brief 移除LRU链表中最久未使用的页面
     * @return 被移除的页面ID，如果无法移除返回-1
     */
    int evictLRU();

    /**
     * @brief 强制刷新并移除最久未使用的脏页
     * @return 被移除的页面ID，如果失败返回-1
     */
    int forceEvictDirtyPage();

    /**
     * @brief 内部移除页面的实现
     * @param pageId 页面ID
     * @param force 是否强制移除（忽略固定状态）
     * @return true如果成功，false如果失败
     */
    bool removePageInternal(int pageId, bool force = false);
};