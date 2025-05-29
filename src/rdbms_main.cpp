#include "SimpleRDBMS.h"
#include <iostream>
#include <string>
#include <vector>
#include <chrono>

class RDBMSTester {
private:
    SimpleRDBMS rdbms;
    
    void executeAndPrint(const std::string& sql) {
        std::cout << "\n执行SQL: " << sql << std::endl;
        std::cout << std::string(50, '-') << std::endl;
        
        auto result = rdbms.executeSQL(sql);
        rdbms.printQueryResult(result);
        std::cout << std::endl;
    }
    
public:
    void runBasicTests() {
        std::cout << "=== 简易RDBMS系统测试 ===" << std::endl;
        
        // 初始化数据库
        if (!rdbms.initialize("./test_db")) {
            std::cout << "Failed to initialize database!" << std::endl;
            return;
        }
        
        std::cout << "✓ 数据库初始化成功" << std::endl;
        
        // 测试CREATE TABLE
        std::cout << "\n--- 创建表测试 ---" << std::endl;
        
        executeAndPrint("CREATE TABLE users (id INT PRIMARY KEY, name VARCHAR(50) NOT NULL, age INT, email VARCHAR(100))");
        
        executeAndPrint("CREATE TABLE products (id INT PRIMARY KEY, name VARCHAR(100) NOT NULL, price INT, category VARCHAR(50))");
        
        // 显示所有表
        std::cout << "\n--- 显示所有表 ---" << std::endl;
        rdbms.showTables();
        
        // 描述表结构
        std::cout << "\n--- 表结构描述 ---" << std::endl;
        rdbms.describeTable("users");
        std::cout << std::endl;
        rdbms.describeTable("products");
        
        // 测试INSERT
        std::cout << "\n--- 插入数据测试 ---" << std::endl;
        
        executeAndPrint("INSERT INTO users (id, name, age, email) VALUES (1, 'Alice', 25, 'alice@example.com')");
        
        executeAndPrint("INSERT INTO users (id, name, age, email) VALUES (2, 'Bob', 30, 'bob@example.com')");
        
        executeAndPrint("INSERT INTO users (id, name, age, email) VALUES (3, 'Charlie', 35, 'charlie@example.com')");
        
        executeAndPrint("INSERT INTO products (id, name, price, category) VALUES (3, 'Phone', 800, 'Electronics')");
        
        // 测试SELECT
        std::cout << "\n--- 查询数据测试 ---" << std::endl;
        
        executeAndPrint("SELECT * FROM users");
        
        executeAndPrint("SELECT name, age FROM users");
        
        executeAndPrint("SELECT * FROM products");
        
        executeAndPrint("SELECT name, price FROM products WHERE category = 'Electronics'");
        
        // 测试UPDATE
        std::cout << "\n--- 更新数据测试 ---" << std::endl;
        
        executeAndPrint("UPDATE users SET age = 26 WHERE id = 1");
        
        executeAndPrint("UPDATE products SET price = 900 WHERE name = 'Phone'");
        
        // 测试DELETE
        std::cout << "\n--- 删除数据测试 ---" << std::endl;
        
        executeAndPrint("DELETE FROM users WHERE id = 3");
        
        executeAndPrint("DELETE FROM products WHERE price < 50");
        
        // 验证删除结果
        std::cout << "\n--- 验证删除结果 ---" << std::endl;
        executeAndPrint("SELECT * FROM users");
        executeAndPrint("SELECT * FROM products");
        
        // 测试错误处理
        std::cout << "\n--- 错误处理测试 ---" << std::endl;
        
        executeAndPrint("CREATE TABLE users (id INT PRIMARY KEY)"); // 表已存在
        
        executeAndPrint("INSERT INTO nonexistent (id) VALUES (1)"); // 表不存在
        
        executeAndPrint("SELECT * FROM nonexistent"); // 表不存在
        
        executeAndPrint("INSERT INTO users (id, name, invalid_column) VALUES (4, 'David', 'test')"); // 列不存在
        
        // 清理测试
        std::cout << "\n--- 清理测试 ---" << std::endl;
        
        executeAndPrint("DROP TABLE users");
        executeAndPrint("DROP TABLE products");
        
        std::cout << "\n--- 最终表列表 ---" << std::endl;
        rdbms.showTables();
        
        rdbms.shutdown();
        std::cout << "\n✓ RDBMS系统测试完成" << std::endl;
    }
    
    void runInteractiveMode() {
        std::cout << "\n=== 交互式SQL模式 ===" << std::endl;
        std::cout << "输入SQL语句（输入 'quit' 或 'exit' 退出）:" << std::endl;
        
        if (!rdbms.initialize("./interactive_db")) {
            std::cout << "Failed to initialize database!" << std::endl;
            return;
        }
        
        std::string input;
        while (true) {
            std::cout << "SQL> ";
            std::getline(std::cin, input);
            
            if (input.empty()) continue;
            
            std::string lowerInput = input;
            std::transform(lowerInput.begin(), lowerInput.end(), lowerInput.begin(), ::tolower);
            
            if (lowerInput == "quit" || lowerInput == "exit") {
                break;
            } else if (lowerInput == "show tables" || lowerInput == "\\dt") {
                rdbms.showTables();
                continue;
            } else if (lowerInput.substr(0, 4) == "desc" || lowerInput.substr(0, 8) == "describe") {
                // 解析表名
                size_t spacePos = lowerInput.find(' ');
                if (spacePos != std::string::npos) {
                    std::string tableName = input.substr(spacePos + 1);
                    // 去除分号
                    if (!tableName.empty() && tableName.back() == ';') {
                        tableName.pop_back();
                    }
                    rdbms.describeTable(tableName);
                }
                continue;
            } else if (lowerInput == "help" || lowerInput == "\\h") {
                printHelp();
                continue;
            }
            
            try {
                auto result = rdbms.executeSQL(input);
                rdbms.printQueryResult(result);
            } catch (const std::exception& e) {
                std::cout << "Error: " << e.what() << std::endl;
            }
        }
        
        rdbms.shutdown();
        std::cout << "再见！" << std::endl;
    }
    
    void printHelp() {
        std::cout << "\n=== 帮助信息 ===" << std::endl;
        std::cout << "支持的SQL语句:" << std::endl;
        std::cout << "  CREATE TABLE table_name (column_name data_type [constraints], ...);" << std::endl;
        std::cout << "  DROP TABLE table_name;" << std::endl;
        std::cout << "  INSERT INTO table_name [(columns)] VALUES (values);" << std::endl;
        std::cout << "  SELECT columns FROM table_name [WHERE conditions];" << std::endl;
        std::cout << "  UPDATE table_name SET column=value [WHERE conditions];" << std::endl;
        std::cout << "  DELETE FROM table_name [WHERE conditions];" << std::endl;
        std::cout << "\n支持的数据类型:" << std::endl;
        std::cout << "  INT, INTEGER - 整数类型" << std::endl;
        std::cout << "  VARCHAR(size) - 变长字符串" << std::endl;
        std::cout << "  BOOLEAN, BOOL - 布尔类型" << std::endl;
        std::cout << "\n支持的约束:" << std::endl;
        std::cout << "  PRIMARY KEY - 主键" << std::endl;
        std::cout << "  NOT NULL - 非空" << std::endl;
        std::cout << "\n特殊命令:" << std::endl;
        std::cout << "  SHOW TABLES 或 \\dt - 显示所有表" << std::endl;
        std::cout << "  DESC table_name 或 DESCRIBE table_name - 描述表结构" << std::endl;
        std::cout << "  HELP 或 \\h - 显示帮助" << std::endl;
        std::cout << "  QUIT 或 EXIT - 退出程序" << std::endl;
        std::cout << std::endl;
    }
    
    void runPerformanceTest() {
        std::cout << "\n=== RDBMS性能测试 ===" << std::endl;
        
        if (!rdbms.initialize("./perf_db")) {
            std::cout << "Failed to initialize database!" << std::endl;
            return;
        }
        
        // 创建测试表
        executeAndPrint("CREATE TABLE test_table (id INT PRIMARY KEY, name VARCHAR(50), value INT)");
        
        // 批量插入测试
        std::cout << "\n--- 批量插入性能测试 ---" << std::endl;
        auto start = std::chrono::high_resolution_clock::now();

        const int TEST_RECORDS = 1000;
        for (int i = 1; i <= TEST_RECORDS; i++) {
            std::string sql = "INSERT INTO test_table (id, name, value) VALUES (" + 
                             std::to_string(i) + ", 'name" + std::to_string(i) + "', " + 
                             std::to_string(i * 10) + ")";
            
            auto result = rdbms.executeSQL(sql);
            if (!result.success) {
                std::cout << "插入失败: " << result.message << std::endl;
                break;
            }
            
            if (i % 100 == 0) {
                std::cout << "已插入 " << i << " 条记录..." << std::endl;
            }
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        std::cout << "插入 " << TEST_RECORDS << " 条记录耗时: " << duration.count() << " ms" << std::endl;
        std::cout << "平均插入时间: " << (double)duration.count() / TEST_RECORDS << " ms/record" << std::endl;
        
        // 查询性能测试
        std::cout << "\n--- 查询性能测试 ---" << std::endl;
        start = std::chrono::high_resolution_clock::now();
        
        const int QUERY_COUNT = 100;
        for (int i = 0; i < QUERY_COUNT; i++) {
            int randomId = 1 + (rand() % TEST_RECORDS);
            std::string sql = "SELECT * FROM test_table WHERE id = " + std::to_string(randomId);
            rdbms.executeSQL(sql);
        }
        
        end = std::chrono::high_resolution_clock::now();
        duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        std::cout << "执行 " << QUERY_COUNT << " 次查询耗时: " << duration.count() << " ms" << std::endl;
        std::cout << "平均查询时间: " << (double)duration.count() / QUERY_COUNT << " ms/query" << std::endl;
        
        // 清理
        executeAndPrint("DROP TABLE test_table");
        
        rdbms.shutdown();
        std::cout << "✓ 性能测试完成" << std::endl;
    }
};

void printMainMenu() {
    std::cout << "\n=== 简易RDBMS系统 ===" << std::endl;
    std::cout << "请选择测试模式:" << std::endl;
    std::cout << "1. 基本功能测试" << std::endl;
    std::cout << "2. 交互式SQL模式" << std::endl;
    std::cout << "3. 性能测试" << std::endl;
    std::cout << "4. 退出" << std::endl;
    std::cout << "请输入选择 (1-4): ";
}

int main() {
    RDBMSTester tester;
    int choice;
    
    while (true) {
        printMainMenu();
        
        if (!(std::cin >> choice)) {
            std::cin.clear();
            std::cin.ignore(10000, '\n');
            std::cout << "无效输入，请输入数字 1-4" << std::endl;
            continue;
        }
        
        std::cin.ignore(); // 清除换行符
        
        switch (choice) {
            case 1:
                tester.runBasicTests();
                break;
            case 2:
                tester.runInteractiveMode();
                break;
            case 3:
                tester.runPerformanceTest();
                break;
            case 4:
                std::cout << "谢谢使用！" << std::endl;
                return 0;
            default:
                std::cout << "无效选择，请输入 1-4" << std::endl;
                break;
        }
        
        std::cout << "\n按Enter键继续...";
        std::cin.get();
    }
    
    return 0;
}