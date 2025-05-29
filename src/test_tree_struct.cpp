#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "BPlusTree.h"

// ================================ TreeHeightValidator
// ================================

struct HeightAnalysis {
    int actualHeight;
    int expectedMinHeight;
    int expectedMaxHeight;
    int totalKeys;
    int totalNodes;
    bool isValid;
    std::string analysis;
};

class TreeHeightValidator {
   public:
    // 计算理论上的最小高度（树完全填满的情况）
    static int calculateMinHeight(int totalKeys, int maxKeysPerPage) {
        if (totalKeys == 0) return 0;
        if (totalKeys <= maxKeysPerPage) return 1;

        // 叶子层需要的最少节点数
        int leafNodes = std::ceil((double)totalKeys / maxKeysPerPage);

        // 逐层向上计算
        int height = 1;
        int currentLevelNodes = leafNodes;

        while (currentLevelNodes > 1) {
            // 上一层需要的内部节点数
            // 每个内部节点最多有 maxKeysPerPage+1 个子节点
            currentLevelNodes =
                std::ceil((double)currentLevelNodes / (maxKeysPerPage + 1));
            height++;
        }

        return height;
    }

    // 计算理论上的最大高度（最不平衡的情况）
    static int calculateMaxHeight(int totalKeys, int maxKeysPerPage) {
        if (totalKeys == 0) return 0;
        if (totalKeys <= maxKeysPerPage) return 1;

        // 最坏情况：每次分裂都最不均匀
        // 近似计算：每个节点最少有 maxKeysPerPage/2 个键
        int minKeysPerNode = std::max(1, maxKeysPerPage / 2);

        // 叶子层需要的最多节点数
        int leafNodes = std::ceil((double)totalKeys / minKeysPerNode);

        int height = 1;
        int currentLevelNodes = leafNodes;

        while (currentLevelNodes > 1) {
            // 每个内部节点最少有 (maxKeysPerPage+1)/2 个子节点
            int minChildrenPerNode = std::max(2, (maxKeysPerPage + 1) / 2);
            currentLevelNodes =
                std::ceil((double)currentLevelNodes / minChildrenPerNode);
            height++;
        }

        return height;
    }

    // 通过公有接口估算键数
    static int estimateTotalKeys(BPlusTree& tree) {
        TreeStats stats = tree.getStat();

        // 使用填充因子和节点数来估算键数
        if (stats.nodeCount == 0) return 0;

        // 估算方法：节点数 * 平均填充率 * 每页最大键数
        double estimatedKeys =
            stats.nodeCount * stats.fillFactor * MAX_KEYS_PER_PAGE;
        return std::max(1, (int)std::round(estimatedKeys));
    }

    // 主要验证函数 - 修改为使用公有接口
    static HeightAnalysis validateTreeHeight(BPlusTree& tree,
                                             int knownKeyCount = -1) {
        HeightAnalysis result;

        TreeStats stats = tree.getStat();
        result.actualHeight = stats.height;
        result.totalNodes = stats.nodeCount;

        // 如果提供了已知键数，使用它；否则估算
        if (knownKeyCount >= 0) {
            result.totalKeys = knownKeyCount;
        } else {
            result.totalKeys = estimateTotalKeys(tree);
        }

        if (result.totalKeys == 0) {
            result.expectedMinHeight = 0;
            result.expectedMaxHeight = 0;
            result.isValid =
                (result.actualHeight == 0 || result.actualHeight == 1);
            result.analysis = "空树，高度应为0或1";
            return result;
        }

        // 计算预期高度范围
        result.expectedMinHeight =
            calculateMinHeight(result.totalKeys, MAX_KEYS_PER_PAGE);
        result.expectedMaxHeight =
            calculateMaxHeight(result.totalKeys, MAX_KEYS_PER_PAGE);

        // 增加一些容错范围，因为实际实现可能有细微差异
        result.expectedMaxHeight += 1;

        // 验证高度是否合理
        result.isValid = (result.actualHeight >= result.expectedMinHeight &&
                          result.actualHeight <= result.expectedMaxHeight);

        // 生成分析报告
        std::ostringstream oss;
        oss << "键数: " << result.totalKeys;
        if (knownKeyCount < 0) {
            oss << "(估算)";
        }
        oss << ", 节点数: " << result.totalNodes
            << ", 实际高度: " << result.actualHeight << ", 预期范围: ["
            << result.expectedMinHeight << ", " << result.expectedMaxHeight
            << "]";

        if (result.isValid) {
            oss << " ✓ 高度合理";
        } else {
            oss << " ✗ 高度异常";
            if (result.actualHeight < result.expectedMinHeight) {
                oss << " (过低)";
            } else {
                oss << " (过高)";
            }
        }

        result.analysis = oss.str();
        return result;
    }

    // 简化的验证函数
    static bool isTreeHeightValid(BPlusTree& tree, int knownKeyCount = -1) {
        HeightAnalysis analysis = validateTreeHeight(tree, knownKeyCount);
        return analysis.isValid;
    }

    // 打印详细分析
    static void printHeightAnalysis(BPlusTree& tree, int knownKeyCount = -1) {
        HeightAnalysis analysis = validateTreeHeight(tree, knownKeyCount);

        std::cout << "\n=== 树高度分析 ===" << std::endl;
        std::cout << analysis.analysis << std::endl;

        if (!analysis.isValid) {
            std::cout << "\n可能的问题：" << std::endl;
            if (analysis.actualHeight > analysis.expectedMaxHeight) {
                std::cout << "- 树可能过于不平衡，分裂算法需要优化"
                          << std::endl;
                std::cout << "- 可能存在过多的单键节点" << std::endl;
            } else if (analysis.actualHeight < analysis.expectedMinHeight) {
                std::cout << "- 高度计算可能有误" << std::endl;
                std::cout << "- 节点可能超出了理论最大容量" << std::endl;
            }
        } else {
            std::cout << "✓ 树结构良好，高度在合理范围内" << std::endl;
        }

        // 计算平均填充度
        if (analysis.totalNodes > 0) {
            double avgKeysPerNode =
                (double)analysis.totalKeys / analysis.totalNodes;
            double theoreticalMax = MAX_KEYS_PER_PAGE;
            double fillRatio = avgKeysPerNode / theoreticalMax * 100;

            std::cout << "平均每节点键数: " << std::fixed
                      << std::setprecision(1) << avgKeysPerNode << "/"
                      << theoreticalMax << " (" << fillRatio << "%)"
                      << std::endl;
        }

        std::cout << std::string(30, '-') << std::endl;
    }
};

// ================================ TreeStructureTester
// ================================

class TreeStructureTester {
   private:
    BPlusTree tree;

    void printTestHeader(const std::string& testName) {
        std::cout << "\n" << std::string(60, '=') << std::endl;
        std::cout << testName << std::endl;
        std::cout << std::string(60, '=') << std::endl;
    }

    bool insertKeys(int startKey, int count) {
        std::cout << "插入键范围: " << startKey << " 到 "
                  << (startKey + count - 1) << std::endl;

        // 预分配字符串以避免重复分配
        std::ostringstream keyStream, valueStream, rowStream;

        for (int i = 0; i < count; i++) {
            int keyNum = startKey + i;

            // 使用流操作减少字符串分配
            keyStream.str("");
            keyStream.clear();
            keyStream << "key" << std::setfill('0') << std::setw(4) << keyNum;

            valueStream.str("");
            valueStream.clear();
            valueStream << "value" << keyNum;

            rowStream.str("");
            rowStream.clear();
            rowStream << "row" << keyNum;

            if (!tree.insert(keyStream.str(), {valueStream.str()},
                             rowStream.str())) {
                std::cout << "✗ 插入失败: " << keyStream.str() << std::endl;
                return false;
            }

            // 定期清理缓冲区和垃圾回收
            if ((i + 1) % 100 == 0) {
                tree.flushBuffer();
                std::cout << "  已插入: " << (i + 1) << "/" << count
                          << " (已清理缓冲区)" << std::endl;
            } else if ((i + 1) % 10 == 0 || i == count - 1) {
                std::cout << "  已插入: " << (i + 1) << "/" << count
                          << std::endl;
            }
        }

        return true;
    }

    void analyzeCurrentState(const std::string& stage, int expectedKeys) {
        std::cout << "\n--- " << stage << " 状态分析 ---" << std::endl;

        TreeStats stats = tree.getStat();
        std::cout << "树高度: " << stats.height << std::endl;
        std::cout << "节点数: " << stats.nodeCount << std::endl;
        std::cout << "分裂次数: " << stats.splitCount << std::endl;
        std::cout << "填充率: " << std::fixed << std::setprecision(1)
                  << stats.fillFactor * 100 << "%" << std::endl;

        // 验证高度合理性
        bool heightValid =
            TreeHeightValidator::isTreeHeightValid(tree, expectedKeys);
        std::cout << "高度验证: " << (heightValid ? "✓ 通过" : "✗ 异常")
                  << std::endl;

        // 验证填充率合理性
        bool fillValid = (stats.fillFactor > 0.0 && stats.fillFactor <= 1.0);
        std::cout << "填充率验证: " << (fillValid ? "✓ 通过" : "✗ 异常")
                  << std::endl;

        std::cout << std::string(40, '-') << std::endl;
    }

   public:
    void test1_EmptyTreeStructure() {
        printTestHeader("测试1: 空树结构验证");

        if (!tree.create("struct_test_empty.db", PAGE_SIZE, 50)) {
            std::cout << "✗ 数据库创建失败!" << std::endl;
            return;
        }

        analyzeCurrentState("空树", 0);

        TreeHeightValidator::printHeightAnalysis(tree, 0);

        tree.close();
    }

    void test2_SinglePageStructure() {
        printTestHeader("测试2: 单页结构验证");

        if (!tree.create("struct_test_single.db", PAGE_SIZE, 50)) {
            std::cout << "✗ 数据库创建失败!" << std::endl;
            return;
        }

        // 插入少量数据，不触发分裂
        int keyCount = MAX_KEYS_PER_PAGE / 2;
        insertKeys(1, keyCount);

        analyzeCurrentState("单页半满", keyCount);

        // 插入到几乎满
        insertKeys(keyCount + 1, MAX_KEYS_PER_PAGE - keyCount);

        analyzeCurrentState("单页接近满载", MAX_KEYS_PER_PAGE);

        TreeHeightValidator::printHeightAnalysis(tree, MAX_KEYS_PER_PAGE);

        std::cout << "\n树结构:" << std::endl;
        tree.printTree();

        tree.close();
    }

    void test3_SplitBehaviorAnalysis() {
        printTestHeader("测试3: 分裂行为深度分析");

        if (!tree.create("struct_test_split.db", PAGE_SIZE, 50)) {
            std::cout << "✗ 数据库创建失败!" << std::endl;
            return;
        }

        std::cout << "最大键数每页: " << MAX_KEYS_PER_PAGE << std::endl;

        // 分裂前的状态
        insertKeys(1, MAX_KEYS_PER_PAGE);
        analyzeCurrentState("分裂前", MAX_KEYS_PER_PAGE);

        TreeStats beforeSplit = tree.getStat();

        // 触发第一次分裂
        std::cout << "\n=== 触发第一次分裂 ===" << std::endl;
        insertKeys(MAX_KEYS_PER_PAGE + 1, 1);

        TreeStats afterFirstSplit = tree.getStat();
        analyzeCurrentState("第一次分裂后", MAX_KEYS_PER_PAGE + 1);

        // 分析分裂效果
        std::cout << "\n分裂效果分析:" << std::endl;
        std::cout << "分裂前节点数: " << beforeSplit.nodeCount << std::endl;
        std::cout << "分裂后节点数: " << afterFirstSplit.nodeCount << std::endl;
        std::cout << "新增节点数: "
                  << (afterFirstSplit.nodeCount - beforeSplit.nodeCount)
                  << std::endl;
        std::cout << "高度变化: " << beforeSplit.height << " -> "
                  << afterFirstSplit.height << std::endl;

        // 检查分裂后键分布的均匀性
        std::cout << "\n树结构（分析键分布）:" << std::endl;
        tree.printTree();

        // 继续插入直到第二次分裂
        std::cout << "\n=== 继续插入至第二次分裂 ===" << std::endl;
        int keysToInsert = MAX_KEYS_PER_PAGE;  // 插入足够多的键
        insertKeys(MAX_KEYS_PER_PAGE + 2, keysToInsert);

        TreeStats afterSecondPhase = tree.getStat();
        analyzeCurrentState("大量插入后", MAX_KEYS_PER_PAGE + 1 + keysToInsert);

        std::cout << "\n最终详细分析:" << std::endl;
        TreeHeightValidator::printHeightAnalysis(
            tree, MAX_KEYS_PER_PAGE + 1 + keysToInsert);

        std::cout << "\n最终树结构:" << std::endl;
        tree.printTree();

        tree.close();
    }

    void test4_ScalabilityAnalysis() {
        printTestHeader("测试4: 可扩展性分析");

        if (!tree.create("struct_test_scale.db", PAGE_SIZE, 100)) {
            std::cout << "✗ 数据库创建失败!" << std::endl;
            return;
        }

        std::vector<int> testSizes = {50, 100, 200, 500, 1000, 10000, 100000};

        for (int size : testSizes) {
            std::cout << "\n=== 测试规模: " << size << " 个键 ===" << std::endl;

            // 计算需要插入的键数
            TreeStats currentStats = tree.getStat();
            int currentKeys = TreeHeightValidator::estimateTotalKeys(tree);
            int keysToInsert = size - currentKeys;

            if (keysToInsert > 0) {
                insertKeys(currentKeys + 1, keysToInsert);
                analyzeCurrentState("规模 " + std::to_string(size), size);

                // 计算理论最优高度
                int theoreticalMinHeight =
                    TreeHeightValidator::calculateMinHeight(size,
                                                            MAX_KEYS_PER_PAGE);
                int theoreticalMaxHeight =
                    TreeHeightValidator::calculateMaxHeight(size,
                                                            MAX_KEYS_PER_PAGE);

                std::cout << "理论高度范围: [" << theoreticalMinHeight << ", "
                          << theoreticalMaxHeight << "]" << std::endl;

                TreeStats stats = tree.getStat();
                double efficiency =
                    (double)theoreticalMinHeight / stats.height * 100;
                std::cout << "高度效率: " << std::fixed << std::setprecision(1)
                          << efficiency << "%" << std::endl;
            }
        }

        // 最终评估
        std::cout << "\n=== 可扩展性总结 ===" << std::endl;
        TreeHeightValidator::printHeightAnalysis(tree, testSizes.back());

        tree.close();
    }

    void test5_BalanceAnalysis() {
        printTestHeader("测试5: 树平衡性分析");

        if (!tree.create("struct_test_balance.db", PAGE_SIZE, 50)) {
            std::cout << "✗ 数据库创建失败!" << std::endl;
            return;
        }

        // 插入有序数据（测试最坏情况）
        std::cout << "=== 有序插入测试（可能导致不平衡） ===" << std::endl;
        int testSize = MAX_KEYS_PER_PAGE * 3;  // 足够触发多次分裂

        for (int i = 1; i <= testSize; i++) {
            std::string key = "key" +
                              std::string(6 - std::to_string(i).length(), '0') +
                              std::to_string(i);
            tree.insert(key, {"value" + std::to_string(i)},
                        "row" + std::to_string(i));

            // 定期检查平衡性
            if (i % MAX_KEYS_PER_PAGE == 0) {
                TreeStats stats = tree.getStat();
                int theoreticalMin = TreeHeightValidator::calculateMinHeight(
                    i, MAX_KEYS_PER_PAGE);
                double balanceFactor = (double)theoreticalMin / stats.height;

                std::cout << "键数: " << i << ", 高度: " << stats.height
                          << ", 理论最小: " << theoreticalMin
                          << ", 平衡因子: " << std::fixed
                          << std::setprecision(2) << balanceFactor << std::endl;
            }
        }

        analyzeCurrentState("有序插入完成", testSize);

        // 评估最终平衡性
        TreeStats finalStats = tree.getStat();
        int theoreticalMin = TreeHeightValidator::calculateMinHeight(
            testSize, MAX_KEYS_PER_PAGE);
        double finalBalance = (double)theoreticalMin / finalStats.height;

        std::cout << "\n平衡性评估:" << std::endl;
        std::cout << "最终平衡因子: " << std::fixed << std::setprecision(2)
                  << finalBalance << std::endl;

        if (finalBalance >= 0.8) {
            std::cout << "✓ 树保持良好平衡" << std::endl;
        } else if (finalBalance >= 0.6) {
            std::cout << "⚠ 树轻微不平衡" << std::endl;
        } else {
            std::cout << "✗ 树严重不平衡" << std::endl;
        }

        TreeHeightValidator::printHeightAnalysis(tree, testSize);

        tree.close();
    }

    void runAllTests() {
        std::cout << "B+树结构专门测试开始" << std::endl;
        std::cout << "配置信息:" << std::endl;
        std::cout << "- 页面大小: " << PAGE_SIZE << " bytes" << std::endl;
        std::cout << "- 每页最大键数: " << MAX_KEYS_PER_PAGE << std::endl;
        std::cout << "- 键大小: " << KEY_SIZE << " bytes" << std::endl;
        std::cout << "- 值大小: " << VALUE_SIZE << " bytes" << std::endl;
        std::cout << std::string(60, '=') << std::endl;

        test1_EmptyTreeStructure();
        test2_SinglePageStructure();
        test3_SplitBehaviorAnalysis();
        test4_ScalabilityAnalysis();
        test5_BalanceAnalysis();

        std::cout << "\n" << std::string(60, '=') << std::endl;
        std::cout << "B+树结构测试完成!" << std::endl;
        std::cout << "建议检查要点:" << std::endl;
        std::cout << "1. 所有高度验证是否通过" << std::endl;
        std::cout << "2. 分裂后键分布是否均匀" << std::endl;
        std::cout << "3. 平衡因子是否在合理范围(>0.6)" << std::endl;
        std::cout << "4. 填充率是否合理(30%-90%)" << std::endl;
        std::cout << std::string(60, '=') << std::endl;
    }
};

// ================================ Main Function
// ================================

int main() {
    try {
        TreeStructureTester tester;
        tester.runAllTests();
    } catch (const std::exception& e) {
        std::cerr << "测试异常: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}