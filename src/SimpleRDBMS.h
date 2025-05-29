#pragma once

#include "BPlusTree.h"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <sstream>
#include <algorithm>
#include <cctype>

// 前向声明
struct Column;
struct Table;
struct SQLStatement;

// 数据类型枚举
enum class DataType {
    INTEGER,
    VARCHAR,
    BOOLEAN
};

// 比较操作符
enum class Operator {
    EQUAL,
    NOT_EQUAL,
    LESS_THAN,
    GREATER_THAN,
    LESS_EQUAL,
    GREATER_EQUAL,
    LIKE
};

// SQL语句类型
enum class SQLType {
    CREATE_TABLE,
    DROP_TABLE,
    INSERT,
    SELECT,
    UPDATE,
    DELETE,
    UNKNOWN
};

// 列定义
struct Column {
    std::string name;
    DataType type;
    int size;  // VARCHAR的大小
    bool isPrimaryKey;
    bool notNull;
    
    Column() : type(DataType::INTEGER), size(0), isPrimaryKey(false), notNull(false) {}
    Column(const std::string& n, DataType t, int s = 0, bool pk = false, bool nn = false)
        : name(n), type(t), size(s), isPrimaryKey(pk), notNull(nn) {}
};

// 表定义
struct Table {
    std::string name;
    std::vector<Column> columns;
    std::string primaryKeyColumn;
    std::unique_ptr<BPlusTree> index;  // 主键索引
    
    Table() = default;
    Table(const std::string& tableName) : name(tableName) {
        index = std::make_unique<BPlusTree>();
    }
    
    // 移动构造函数
    Table(Table&& other) noexcept 
        : name(std::move(other.name)),
          columns(std::move(other.columns)),
          primaryKeyColumn(std::move(other.primaryKeyColumn)),
          index(std::move(other.index)) {}
    
    // 移动赋值操作符
    Table& operator=(Table&& other) noexcept {
        if (this != &other) {
            name = std::move(other.name);
            columns = std::move(other.columns);
            primaryKeyColumn = std::move(other.primaryKeyColumn);
            index = std::move(other.index);
        }
        return *this;
    }
    
    // 禁用拷贝构造和拷贝赋值
    Table(const Table&) = delete;
    Table& operator=(const Table&) = delete;
};

// WHERE条件
struct WhereCondition {
    std::string column;
    Operator op;
    std::string value;
    
    WhereCondition() : op(Operator::EQUAL) {}
    WhereCondition(const std::string& col, Operator o, const std::string& val)
        : column(col), op(o), value(val) {}
};

// SQL语句结构
struct SQLStatement {
    SQLType type;
    std::string tableName;
    std::vector<Column> columns;  // CREATE TABLE用
    std::vector<std::string> columnNames;  // SELECT, INSERT用
    std::vector<std::string> values;  // INSERT用
    std::vector<WhereCondition> whereConditions;  // WHERE子句
    std::map<std::string, std::string> updateValues;  // UPDATE用
    std::string primaryKeyColumn;  // 主键列名（CREATE TABLE用）
    
    SQLStatement() : type(SQLType::UNKNOWN) {}
};

// 查询结果
struct QueryResult {
    bool success;
    std::string message;
    std::vector<std::string> columnHeaders;
    std::vector<std::vector<std::string>> rows;
    int affectedRows;
    
    QueryResult() : success(false), affectedRows(0) {}
};

// 简易RDBMS类
class SimpleRDBMS {
private:
    std::map<std::string, std::unique_ptr<Table>> tables_;
    std::string dbPath_;
    
    // SQL解析相关方法
    std::vector<std::string> tokenize(const std::string& sql);
    std::string toLowerCase(const std::string& str);
    std::string trim(const std::string& str);
    SQLStatement parseSQL(const std::string& sql);
    
    // 具体解析方法
    SQLStatement parseCreateTable(const std::vector<std::string>& tokens);
    SQLStatement parseDropTable(const std::vector<std::string>& tokens);
    SQLStatement parseInsert(const std::vector<std::string>& tokens);
    SQLStatement parseSelect(const std::vector<std::string>& tokens);
    SQLStatement parseUpdate(const std::vector<std::string>& tokens);
    SQLStatement parseDelete(const std::vector<std::string>& tokens);
    
    // WHERE条件解析
    std::vector<WhereCondition> parseWhereClause(const std::vector<std::string>& tokens, size_t startPos);
    Operator parseOperator(const std::string& op);
    
    // 数据类型解析
    DataType parseDataType(const std::string& typeStr, int& size);
    
    // SQL执行方法
    QueryResult executeCreateTable(const SQLStatement& stmt);
    QueryResult executeDropTable(const SQLStatement& stmt);
    QueryResult executeInsert(const SQLStatement& stmt);
    QueryResult executeSelect(const SQLStatement& stmt);
    QueryResult executeUpdate(const SQLStatement& stmt);
    QueryResult executeDelete(const SQLStatement& stmt);
    
    // 辅助方法
    bool tableExists(const std::string& tableName);
    Table* getTable(const std::string& tableName);
    std::string generateRowId();
    bool validateValue(const std::string& value, const Column& column);
    bool evaluateCondition(const std::vector<std::string>& row, 
                          const WhereCondition& condition, 
                          const Table& table);
    std::string formatValue(const std::string& value, DataType type);
    std::vector<std::string> parseValueList(const std::string& valueStr);
    
    // 文件操作
    std::string getIndexFileName(const std::string& tableName);
    std::string getTableSchemaFileName(const std::string& tableName);
    bool saveTableSchema(const Table& table);
    bool loadTableSchema(const std::string& tableName);
    
public:
    SimpleRDBMS();
    ~SimpleRDBMS();
    
    // 主要接口
    bool initialize(const std::string& dbPath);
    void shutdown();
    QueryResult executeSQL(const std::string& sql);
    
    // 调试和管理接口
    void showTables();
    void describeTable(const std::string& tableName);
    void printQueryResult(const QueryResult& result);
};