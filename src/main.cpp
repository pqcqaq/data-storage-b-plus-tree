#include <chrono>
#include <iomanip>
#include <iostream>
#include <random>
#include <set>
#include <vector>

#include "BPlusTree.h"

class BPlusTreeTester {
   private:
    BPlusTree tree;
    std::mt19937 rng;

    // 测试常量
    static const int TOTAL_RECORDS = 50000;
    static const int QUERY_COUNT = 10000;
    static const int STRESS_RECORDS = 100000;

    std::set<std::string> insertedKeys;
    std::chrono::high_resolution_clock::time_point start, end;

    std::string generateRandomKey(int length = 16) {
        const std::string chars =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
        std::uniform_int_distribution<> dis(0, chars.size() - 1);

        std::string result;
        for (int i = 0; i < length; ++i) {
            result += chars[dis(rng)];
        }
        return result;
    }

    std::string generateRandomValue(int length = 20) {
        return generateRandomKey(length);
    }

   public:
    BPlusTreeTester()
        : rng(std::chrono::steady_clock::now().time_since_epoch().count()) {}

    void basicTest() {
        std::cout << "\n=== 基本功能测试 ===" << std::endl;

        // 创建数据库，设置较小的缓冲池
        if (!tree.create("test.db", PAGE_SIZE, 50)) {
            std::cout << "Failed to create database!" << std::endl;
            return;
        }

        std::cout << "✓ 数据库创建成功" << std::endl;

        // 插入测试数据
        std::vector<std::string> testKeys = {"apple", "banana", "cherry",
                                             "date", "elderberry"};
        std::vector<std::string> testValues = {"red fruit", "yellow fruit",
                                               "red berry", "sweet fruit",
                                               "dark berry"};

        for (size_t i = 0; i < testKeys.size(); i++) {
            std::vector<std::string> value = {testValues[i]};
            std::string rowId = "row" + std::to_string(i);

            if (tree.insert(testKeys[i], value, rowId)) {
                std::cout << "✓ 插入成功: " << testKeys[i] << " -> "
                          << testValues[i] << std::endl;
            } else {
                std::cout << "✗ 插入失败: " << testKeys[i] << std::endl;
            }
        }

        // 查询测试
        std::cout << "\n--- 查询测试 ---" << std::endl;
        for (const auto& key : testKeys) {
            auto results = tree.get(key);
            if (!results.empty()) {
                std::cout << "✓ 查询 " << key << ": " << results[0][0]
                          << std::endl;
            } else {
                std::cout << "✗ 查询失败: " << key << std::endl;
            }
        }

        // 删除测试
        std::cout << "\n--- 删除测试 ---" << std::endl;
        if (tree.remove("banana")) {
            std::cout << "✓ 删除成功: banana" << std::endl;
        } else {
            std::cout << "✗ 删除失败: banana" << std::endl;
        }

        // 再次查询已删除的项
        auto result = tree.get("banana");
        if (result.empty()) {
            std::cout << "✓ 确认删除: banana 不存在" << std::endl;
        } else {
            std::cout << "✗ 删除验证失败: banana 仍然存在" << std::endl;
        }

        // 打印树状态
        TreeStats stats = tree.getStat();
        std::cout << "\n--- 当前树状态 ---" << std::endl;
        std::cout << "高度: " << stats.height << std::endl;
        std::cout << "节点数: " << stats.nodeCount << std::endl;
        std::cout << "分裂次数: " << stats.splitCount << std::endl;
        std::cout << "填充率: " << std::fixed << std::setprecision(2)
                  << stats.fillFactor * 100 << "%" << std::endl;
        std::cout << "文件写入次数: " << stats.fileWriteCount << std::endl;
        std::cout << "缓冲池大小: " << tree.getBufferPoolStats().totalPages
                  << " pages" << std::endl;

        // 打印缓冲池状态
        std::cout << "\n--- 缓冲池状态 ---" << std::endl;
        tree.printBufferPoolStatus();

        tree.printTree();

        tree.close();
    }

    void performanceTest() {
        std::cout << "\n=== 性能测试 ===" << std::endl;

        // 创建新的数据库文件用于性能测试，设置适中的缓冲池
        tree.close();
        if (!tree.create("performance_test.db", PAGE_SIZE, 200)) {
            std::cout << "Failed to create performance test database!"
                      << std::endl;
            return;
        }

        std::cout << "开始插入 " << TOTAL_RECORDS << " 条记录..." << std::endl;

        // 插入性能测试
        start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < TOTAL_RECORDS; i++) {
            std::string key =
                "key_" + std::to_string(i) + "_" + generateRandomKey(8);
            std::string value = generateRandomValue(25);
            std::string rowId = "perf_" + std::to_string(i);

            insertedKeys.insert(key);
            if (!tree.insert(key, {value}, rowId)) {
                std::cout << "插入失败: " << key << std::endl;
                break;
            }

            if ((i + 1) % 5000 == 0) {
                std::cout << "进度: " << (i + 1) << "/" << TOTAL_RECORDS
                          << std::endl;
                // 定期刷新缓冲区
                tree.flushBuffer();
            }
        }

        end = std::chrono::high_resolution_clock::now();
        auto insertDuration =
            std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        std::cout << "✓ 插入完成!" << std::endl;
        std::cout << "插入时间: " << insertDuration.count() << " ms"
                  << std::endl;
        std::cout << "平均插入时间: "
                  << (double)insertDuration.count() / TOTAL_RECORDS
                  << " ms/record" << std::endl;

        // 获取当前树状态
        TreeStats stats = tree.getStat();
        std::cout << "\n--- 当前树状态 ---" << std::endl;
        std::cout << "高度: " << stats.height << std::endl;
        std::cout << "节点数: " << stats.nodeCount << std::endl;
        std::cout << "分裂次数: " << stats.splitCount << std::endl;
        std::cout << "填充率: " << std::fixed << std::setprecision(2)
                  << stats.fillFactor * 100 << "%" << std::endl;
        std::cout << "文件写入次数: " << stats.fileWriteCount << std::endl;
        std::cout << "缓冲池大小: " << tree.getBufferPoolStats().totalPages
                  << " pages" << std::endl;

        // 打印缓冲池统计
        auto bufferStats = tree.getBufferPoolStats();
        std::cout << "缓冲池命中率: " << std::fixed << std::setprecision(2)
                  << bufferStats.hitRatio * 100 << "%" << std::endl;

        // 查询性能测试
        std::cout << "\n开始查询测试..." << std::endl;
        std::vector<std::string> queryKeys;

        // 随机选择一些已插入的键进行查询
        auto it = insertedKeys.begin();
        int step = std::max(1, (int)(insertedKeys.size() / QUERY_COUNT));
        for (int i = 0; i < QUERY_COUNT && it != insertedKeys.end();
             i++, std::advance(it, step)) {
            queryKeys.push_back(*it);
        }

        // 如果键不够，补充一些随机键
        while (queryKeys.size() < QUERY_COUNT) {
            std::string randomKey = "key_" +
                                    std::to_string(rng() % TOTAL_RECORDS) +
                                    "_" + generateRandomKey(8);
            queryKeys.push_back(randomKey);
        }

        int successfulQueries = 0;
        int totalPageAccess = 0;

        start = std::chrono::high_resolution_clock::now();

        for (const auto& key : queryKeys) {
            auto results = tree.get(key);
            if (!results.empty()) {
                successfulQueries++;
            }
            totalPageAccess += stats.height;  // 估算页面访问次数
        }

        end = std::chrono::high_resolution_clock::now();
        auto queryDuration =
            std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        std::cout << "✓ 查询完成!" << std::endl;
        std::cout << "查询数量: " << QUERY_COUNT << std::endl;
        std::cout << "成功查询: " << successfulQueries << std::endl;
        std::cout << "查询时间: " << queryDuration.count() << " μs"
                  << std::endl;
        std::cout << "平均查询时间: "
                  << (double)queryDuration.count() / QUERY_COUNT << " μs/query"
                  << std::endl;
        std::cout << "平均页面访问次数: "
                  << (double)totalPageAccess / QUERY_COUNT << " pages/query"
                  << std::endl;

        // 最终统计
        std::cout << "\n=== 最终性能报告 ===" << std::endl;
        std::cout << "总记录数: " << TOTAL_RECORDS << std::endl;
        std::cout << "B+树高度: " << stats.height << std::endl;
        std::cout << "节点总数: " << stats.nodeCount << std::endl;
        std::cout << "分裂次数: " << stats.splitCount << std::endl;
        std::cout << "平均填充率: " << std::fixed << std::setprecision(2)
                  << stats.fillFactor * 100 << "%" << std::endl;
        std::cout << "平均查询页面访问: "
                  << (double)totalPageAccess / QUERY_COUNT << " pages"
                  << std::endl;

        // 最终缓冲池统计
        bufferStats = tree.getBufferPoolStats();
        std::cout << "最终命中率: " << std::fixed << std::setprecision(2)
                  << bufferStats.hitRatio * 100 << "%" << std::endl;

        tree.close();
    }

    void stressTest() {
        std::cout << "\n=== 压力测试 ===" << std::endl;

        tree.close();
        if (!tree.create("stress_test.db", PAGE_SIZE, 300)) {
            std::cout << "Failed to create stress test database!" << std::endl;
            return;
        }

        std::vector<std::string> keys;
        keys.reserve(STRESS_RECORDS);

        // 插入随机数据
        std::cout << "插入 " << STRESS_RECORDS << " 条随机记录..." << std::endl;
        auto stressStart = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < STRESS_RECORDS; i++) {
            std::string key = generateRandomKey(15);
            std::string value = generateRandomValue(30);
            std::string rowId = "stress_" + std::to_string(i);

            keys.push_back(key);
            if (!tree.insert(key, {value}, rowId)) {
                std::cout << "插入失败: " << key << std::endl;
                break;
            }

            if ((i + 1) % 10000 == 0) {
                std::cout << "进度: " << (i + 1) << "/" << STRESS_RECORDS
                          << std::endl;
                // 定期刷新和检查内存
                tree.flushBuffer();
            }
        }

        auto stressEnd = std::chrono::high_resolution_clock::now();
        auto stressDuration =
            std::chrono::duration_cast<std::chrono::milliseconds>(stressEnd -
                                                                  stressStart);
        std::cout << "插入耗时: " << stressDuration.count() << " ms"
                  << std::endl;
        // 平均
        std::cout << "平均插入时间: "
                  << (double)stressDuration.count() / STRESS_RECORDS
                  << " ms/record" << std::endl;

        // 随机查询测试
        std::cout << "执行随机查询..." << std::endl;
        int queryCount = 10000;
        int found = 0;

        auto queryStart = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < queryCount; i++) {
            if (keys.empty()) break;
            std::string queryKey = keys[rng() % keys.size()];
            auto results = tree.get(queryKey);
            if (!results.empty()) {
                found++;
            }
        }
        auto queryEnd = std::chrono::high_resolution_clock::now();
        auto queryDuration =
            std::chrono::duration_cast<std::chrono::milliseconds>(queryEnd -
                                                                  queryStart);

        std::cout << "查询结果: " << found << "/" << queryCount << " 成功"
                  << std::endl;
        std::cout << "查询耗时: " << queryDuration.count() << " ms"
                  << std::endl;

        // 随机删除测试
        std::cout << "执行随机删除..." << std::endl;
        int deleteCount = 5000;
        int deleted = 0;

        auto deleteStart = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < deleteCount && !keys.empty(); i++) {
            int randomIndex = rng() % keys.size();
            std::string deleteKey = keys[randomIndex];
            if (tree.remove(deleteKey)) {
                deleted++;
                keys.erase(keys.begin() + randomIndex);
            }
        }
        auto deleteEnd = std::chrono::high_resolution_clock::now();
        auto deleteDuration =
            std::chrono::duration_cast<std::chrono::milliseconds>(deleteEnd -
                                                                  deleteStart);

        std::cout << "删除结果: " << deleted << " 条记录" << std::endl;
        std::cout << "删除耗时: " << deleteDuration.count() << " ms"
                  << std::endl;

        // 最终状态
        TreeStats finalStats = tree.getStat();
        std::cout << "\n--- 当前树状态 ---" << std::endl;
        std::cout << "高度: " << finalStats.height << std::endl;
        std::cout << "节点数: " << finalStats.nodeCount << std::endl;
        std::cout << "分裂次数: " << finalStats.splitCount << std::endl;
        std::cout << "填充率: " << std::fixed << std::setprecision(2)
                  << finalStats.fillFactor * 100 << "%" << std::endl;
        std::cout << "文件写入次数: " << finalStats.fileWriteCount << std::endl;
        std::cout << "缓冲池大小: " << tree.getBufferPoolStats().totalPages
                  << " pages" << std::endl;

        // 最终缓冲池统计
        auto bufferStats = tree.getBufferPoolStats();
        std::cout << "- 缓冲池命中率: " << std::fixed << std::setprecision(2)
                  << bufferStats.hitRatio * 100 << "%" << std::endl;

        tree.close();
    }

    void memoryTest() {
        std::cout << "\n=== 内存管理测试 ===" << std::endl;

        tree.close();
        if (!tree.create("memory_test.db", PAGE_SIZE, 20)) {  // 很小的缓冲池
            std::cout << "Failed to create memory test database!" << std::endl;
            return;
        }

        std::cout << "使用小缓冲池(20页)测试大量插入..." << std::endl;

        const int MEMORY_TEST_RECORDS = 100000;
        auto start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < MEMORY_TEST_RECORDS; i++) {
            std::string key =
                "mem_" + std::to_string(i) + "_" + generateRandomKey(10);
            std::string value = generateRandomValue(20);
            std::string rowId = "mem_row_" + std::to_string(i);

            if (!tree.insert(key, {value}, rowId)) {
                std::cout << "插入失败在记录 " << i << std::endl;
                break;
            }

            if ((i + 1) % 1000 == 0) {
                auto bufferStats = tree.getBufferPoolStats();
                std::cout << "进度: " << (i + 1) << "/" << MEMORY_TEST_RECORDS
                          << ", 命中率: " << std::fixed << std::setprecision(2)
                          << bufferStats.hitRatio * 100 << "%" << std::endl;
            }
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration =
            std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        std::cout << "内存测试完成，耗时: " << duration.count() << " ms"
                  << std::endl;

        // 最终缓冲池统计
        auto finalStats = tree.getBufferPoolStats();
        std::cout << "最终缓冲池统计:" << std::endl;
        std::cout << "- 总页面: " << finalStats.totalPages << "/"
                  << finalStats.maxSize << std::endl;
        std::cout << "- 脏页: " << finalStats.dirtyPages << std::endl;
        std::cout << "- 命中次数: " << finalStats.hitCount << std::endl;
        std::cout << "- 未命中次数: " << finalStats.missCount << std::endl;
        std::cout << "- 命中率: " << std::fixed << std::setprecision(2)
                  << finalStats.hitRatio * 100 << "%" << std::endl;

        tree.printBufferPoolStatus();
        tree.close();
    }

    void runAllTests() {
        std::cout << "B+树数据库管理系统测试开始" << std::endl;
        std::cout << "========================================" << std::endl;

        basicTest();
        performanceTest();
        memoryTest();
        stressTest();

        std::cout << "\n========================================" << std::endl;
        std::cout << "所有测试完成!" << std::endl;
    }
};

int main() {
    try {
        BPlusTreeTester tester;
        tester.runAllTests();
    } catch (const std::exception& e) {
        std::cerr << "测试过程中发生异常: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}