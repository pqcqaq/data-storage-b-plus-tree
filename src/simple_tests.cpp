#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "BPlusTree.h"

class SimpleBPlusTreeTester {
   private:
    BPlusTree tree;

    // 计算每页最大键数
    static const int MAX_KEYS_PER_PAGE =
        (PAGE_SIZE - sizeof(PageHeader)) / (KEY_SIZE + ROW_ID_SIZE + VALUE_SIZE);

    void printTestHeader(const std::string& testName) {
        std::cout << "\n" << std::string(50, '=') << std::endl;
        std::cout << testName << std::endl;
        std::cout << std::string(50, '=') << std::endl;
    }

    void printTreeStats() {
        TreeStats stats = tree.getStat();
        std::cout << "\n--- 树状态信息 ---" << std::endl;
        std::cout << "树高度: " << stats.height << std::endl;
        std::cout << "节点数: " << stats.nodeCount << std::endl;
        std::cout << "分裂次数: " << stats.splitCount << std::endl;
        std::cout << "填充率: " << std::fixed << std::setprecision(1)
                  << stats.fillFactor * 100 << "%" << std::endl;
        std::cout << "理论每页最大键数: " << MAX_KEYS_PER_PAGE << std::endl;
        std::cout << std::string(30, '-') << std::endl;
    }

    bool validateInsert(const std::string& key, const std::string& value,
                        const std::string& rowId) {
        std::vector<std::string> valueVec = {value};
        bool success = tree.insert(key, valueVec, rowId);
        if (success) {
            std::cout << "✓ 插入成功: " << key << " -> " << value << std::endl;
        } else {
            std::cout << "✗ 插入失败: " << key << std::endl;
        }
        return success;
    }

    bool validateQuery(const std::string& key,
                       const std::string& expectedValue = "") {
        auto results = tree.get(key);
        if (!results.empty() && !results[0].empty()) {
            std::cout << "✓ 查询成功: " << key << " -> " << results[0][0]
                      << std::endl;
            if (!expectedValue.empty() && results[0][0] != expectedValue) {
                std::cout << "✗ 值不匹配! 期望: " << expectedValue
                          << ", 实际: " << results[0][0] << std::endl;
                return false;
            }
            return true;
        } else {
            std::cout << "✗ 查询失败: " << key << " (未找到)" << std::endl;
            return false;
        }
    }

    bool validateDelete(const std::string& key) {
        bool success = tree.remove(key);
        if (success) {
            std::cout << "✓ 删除成功: " << key << std::endl;
            // 验证删除后确实不存在
            auto results = tree.get(key);
            if (results.empty()) {
                std::cout << "✓ 删除验证成功: " << key << " 已不存在"
                          << std::endl;
                return true;
            } else {
                std::cout << "✗ 删除验证失败: " << key << " 仍然存在"
                          << std::endl;
                return false;
            }
        } else {
            std::cout << "✗ 删除失败: " << key << std::endl;
            return false;
        }
    }

   public:
    void test1_BasicOperations() {
        printTestHeader("测试1: 基本增删改查操作");

        // 创建数据库
        if (!tree.create("simple_test.db", PAGE_SIZE, 50)) {
            std::cout << "✗ 数据库创建失败!" << std::endl;
            return;
        }
        std::cout << "✓ 数据库创建成功" << std::endl;

        // 测试插入
        std::cout << "\n-- 插入测试 --" << std::endl;
        validateInsert("key001", "value001", "row001");
        validateInsert("key002", "value002", "row002");
        validateInsert("key003", "value003", "row003");

        printTreeStats();

        // 测试查询
        std::cout << "\n-- 查询测试 --" << std::endl;
        validateQuery("key001", "value001");
        validateQuery("key002", "value002");
        validateQuery("key003", "value003");
        validateQuery("key999");  // 不存在的键

        // 测试删除
        std::cout << "\n-- 删除测试 --" << std::endl;
        validateDelete("key002");

        // 验证删除后的查询
        std::cout << "\n-- 删除后查询验证 --" << std::endl;
        validateQuery("key001", "value001");  // 应该存在
        validateQuery("key002");              // 应该不存在
        validateQuery("key003", "value003");  // 应该存在

        printTreeStats();
        tree.close();
    }

    void test2_TriggerSplit() {
        printTestHeader("测试2: 触发页面分裂");

        if (!tree.create("split_test.db", PAGE_SIZE, 50)) {
            std::cout << "✗ 数据库创建失败!" << std::endl;
            return;
        }
        std::cout << "✓ 数据库创建成功" << std::endl;
        std::cout << "每页最大键数: " << MAX_KEYS_PER_PAGE << std::endl;

        // 插入足够多的数据来触发分裂
        std::cout << "\n-- 插入数据直到触发分裂 --" << std::endl;
        int insertCount = MAX_KEYS_PER_PAGE + 5;  // 比最大值多一些，确保分裂

        for (int i = 1; i <= insertCount; i++) {
            std::string key = "key" +
                              std::string(3 - std::to_string(i).length(), '0') +
                              std::to_string(i);
            std::string value =
                "value" + std::string(3 - std::to_string(i).length(), '0') +
                std::to_string(i);
            std::string rowId =
                "row" + std::string(3 - std::to_string(i).length(), '0') +
                std::to_string(i);

            validateInsert(key, value, rowId);

            // 每插入几个就检查一次状态
            if (i % 5 == 0 || i == insertCount) {
                TreeStats stats = tree.getStat();
                std::cout << "  -> 当前高度: " << stats.height
                          << ", 节点数: " << stats.nodeCount
                          << ", 分裂次数: " << stats.splitCount << std::endl;
            }
        }

        printTreeStats();

        // 验证所有插入的数据都能正确查询
        std::cout << "\n-- 验证所有数据的正确性 --" << std::endl;
        bool allCorrect = true;
        for (int i = 1; i <= insertCount; i++) {
            std::string key = "key" +
                              std::string(3 - std::to_string(i).length(), '0') +
                              std::to_string(i);
            std::string expectedValue =
                "value" + std::string(3 - std::to_string(i).length(), '0') +
                std::to_string(i);

            if (!validateQuery(key, expectedValue)) {
                allCorrect = false;
            }
        }

        if (allCorrect) {
            std::cout << "✓ 所有数据验证通过!" << std::endl;
        } else {
            std::cout << "✗ 数据验证失败!" << std::endl;
        }

        // 打印树结构
        std::cout << "\n-- 树结构 --" << std::endl;
        tree.printTree();

        tree.close();
    }

    void test3_OrderedOperations() {
        printTestHeader("测试3: 有序数据操作");

        if (!tree.create("ordered_test.db", PAGE_SIZE, 50)) {
            std::cout << "✗ 数据库创建失败!" << std::endl;
            return;
        }

        // 插入有序数据
        std::cout << "\n-- 插入有序数据 --" << std::endl;
        std::vector<std::string> keys = {
            "apple", "banana", "cherry",   "date", "elderberry",
            "fig",   "grape",  "honeydew", "kiwi", "lemon"};

        for (size_t i = 0; i < keys.size(); i++) {
            std::string value = "fruit_" + std::to_string(i);
            std::string rowId = "row_" + std::to_string(i);
            validateInsert(keys[i], value, rowId);
        }

        printTreeStats();

        // 测试范围查询的基础 - 按顺序查询
        std::cout << "\n-- 按字典序查询验证 --" << std::endl;
        for (const auto& key : keys) {
            validateQuery(key);
        }

        // 删除中间的一些元素
        std::cout << "\n-- 删除中间元素 --" << std::endl;
        validateDelete("cherry");
        validateDelete("grape");
        validateDelete("kiwi");

        // 再次验证
        std::cout << "\n-- 删除后验证 --" << std::endl;
        std::vector<std::string> remainingKeys = {
            "apple", "banana",   "date", "elderberry",
            "fig",   "honeydew", "lemon"};
        for (const auto& key : remainingKeys) {
            validateQuery(key);
        }

        printTreeStats();
        std::cout << "\n-- 最终树结构 --" << std::endl;
        tree.printTree();

        tree.close();
    }

    void test4_EdgeCases() {
        printTestHeader("测试4: 边界情况");

        if (!tree.create("edge_test.db", PAGE_SIZE, 50)) {
            std::cout << "✗ 数据库创建失败!" << std::endl;
            return;
        }

        // 测试重复键
        std::cout << "\n-- 重复键测试 --" << std::endl;
        validateInsert("duplicate", "value1", "row1");
        bool duplicateResult = tree.insert({"duplicate"}, {"value2"}, "row2");
        if (!duplicateResult) {
            std::cout << "✓ 重复键正确拒绝" << std::endl;
        } else {
            std::cout << "✗ 重复键应该被拒绝" << std::endl;
        }

        // 测试空键和空值的处理
        std::cout << "\n-- 空值测试 --" << std::endl;
        validateInsert("", "empty_key", "row_empty");
        validateInsert("empty_value", "", "row_empty_val");

        // 测试删除不存在的键
        std::cout << "\n-- 删除不存在键测试 --" << std::endl;
        bool deleteResult = tree.remove("nonexistent");
        if (!deleteResult) {
            std::cout << "✓ 删除不存在键正确返回失败" << std::endl;
        } else {
            std::cout << "✗ 删除不存在键应该返回失败" << std::endl;
        }

        printTreeStats();
        tree.close();
    }

    void runAllTests() {
        std::cout << "简单B+树测试开始" << std::endl;
        std::cout << "页面大小: " << PAGE_SIZE << " bytes" << std::endl;
        std::cout << "每页理论最大键数: " << MAX_KEYS_PER_PAGE << std::endl;

        test1_BasicOperations();
        test2_TriggerSplit();
        test3_OrderedOperations();
        test4_EdgeCases();
        debugDuplicateKeyIssue();
        debugSplitDistribution();

        std::cout << "\n" << std::string(50, '=') << std::endl;
        std::cout << "所有测试完成!" << std::endl;
        std::cout << std::string(50, '=') << std::endl;
    }

    void debugDuplicateKeyIssue() {
        std::cout << "=== 调试重复键问题 ===" << std::endl;

        BPlusTree tree;
        tree.create("debug.db", PAGE_SIZE, 50);

        // 第一次插入
        bool result1 = tree.insert("test_key", {"value1"}, "row1");
        std::cout << "第一次插入 test_key: " << (result1 ? "成功" : "失败")
                  << std::endl;

        // 查询确认存在
        auto query1 = tree.get("test_key");
        std::cout << "第一次查询 test_key: "
                  << (query1.empty() ? "未找到" : "找到: " + query1[0][0])
                  << std::endl;

        // 第二次插入相同键
        bool result2 = tree.insert("test_key", {"value2"}, "row2");
        std::cout << "第二次插入 test_key: "
                  << (result2 ? "成功(有问题!)" : "失败(正确)") << std::endl;

        // 再次查询
        auto query2 = tree.get("test_key");
        if (!query2.empty()) {
            std::cout << "第二次查询 test_key: 找到: " << query2[0][0]
                      << std::endl;
            if (query2.size() > 1) {
                std::cout << "警告：找到多个值！" << std::endl;
            }
        }

        tree.close();
    }

    void debugSplitDistribution() {
        std::cout << "\n=== 调试分裂分布问题 ===" << std::endl;

        BPlusTree tree;
        tree.create("split_debug.db", PAGE_SIZE, 50);

        std::cout << "插入18个键（应该不分裂）..." << std::endl;
        for (int i = 1; i <= 18; i++) {
            std::string key = "key" +
                              std::string(3 - std::to_string(i).length(), '0') +
                              std::to_string(i);
            tree.insert(key, {"value" + std::to_string(i)},
                        "row" + std::to_string(i));

            if (i % 6 == 0) {
                TreeStats stats = tree.getStat();
                std::cout << "  插入" << i << "个键后 - 高度:" << stats.height
                          << ", 节点数:" << stats.nodeCount << std::endl;
            }
        }

        TreeStats beforeSplit = tree.getStat();
        std::cout << "分裂前 - 高度:" << beforeSplit.height
                  << ", 节点数:" << beforeSplit.nodeCount << std::endl;

        std::cout << "\n插入第19个键（应该触发分裂）..." << std::endl;
        tree.insert("key019", {"value19"}, "row19");

        TreeStats afterSplit = tree.getStat();
        std::cout << "分裂后 - 高度:" << afterSplit.height
                  << ", 节点数:" << afterSplit.nodeCount
                  << ", 分裂次数:" << afterSplit.splitCount << std::endl;

        std::cout << "\n分裂后树结构:" << std::endl;
        tree.printTree();

        // 检查键分布是否均匀
        std::cout
            << "\n分析：如果看到左右子页面键数相差超过2，则分裂算法需要优化"
            << std::endl;

        tree.close();
    }
};

int main() {
    try {
        SimpleBPlusTreeTester tester;
        tester.runAllTests();
    } catch (const std::exception& e) {
        std::cerr << "测试异常: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}