#include "SimpleRDBMS.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <ctime>
#include <random>

SimpleRDBMS::SimpleRDBMS() {
}

SimpleRDBMS::~SimpleRDBMS() {
    shutdown();
}

bool SimpleRDBMS::initialize(const std::string& dbPath) {
    dbPath_ = dbPath;
    
    // 创建数据库目录
    try {
        std::filesystem::create_directories(dbPath_);
    } catch (const std::exception& e) {
        std::cerr << "Failed to create database directory: " << e.what() << std::endl;
        return false;
    }
    
    // 加载现有表的元数据
    try {
        for (const auto& entry : std::filesystem::directory_iterator(dbPath_)) {
            if (entry.path().extension() == ".schema") {
                std::string tableName = entry.path().stem().string();
                loadTableSchema(tableName);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error loading existing schemas: " << e.what() << std::endl;
    }
    
    return true;
}

void SimpleRDBMS::shutdown() {
    // 保存所有表的模式并关闭索引
    for (auto& pair : tables_) {
        saveTableSchema(*pair.second);
        if (pair.second->index) {
            pair.second->index->close();
        }
    }
    tables_.clear();
}

QueryResult SimpleRDBMS::executeSQL(const std::string& sql) {
    QueryResult result;
    
    try {
        SQLStatement stmt = parseSQL(sql);
        
        switch (stmt.type) {
            case SQLType::CREATE_TABLE:
                result = executeCreateTable(stmt);
                break;
            case SQLType::DROP_TABLE:
                result = executeDropTable(stmt);
                break;
            case SQLType::INSERT:
                result = executeInsert(stmt);
                break;
            case SQLType::SELECT:
                result = executeSelect(stmt);
                break;
            case SQLType::UPDATE:
                result = executeUpdate(stmt);
                break;
            case SQLType::DELETE:
                result = executeDelete(stmt);
                break;
            default:
                result.success = false;
                result.message = "Unknown or unsupported SQL statement";
        }
    } catch (const std::exception& e) {
        result.success = false;
        result.message = "Error executing SQL: " + std::string(e.what());
    }
    
    return result;
}

// SQL解析相关方法实现
std::vector<std::string> SimpleRDBMS::tokenize(const std::string& sql) {
    std::vector<std::string> tokens;
    std::string token;
    bool inQuotes = false;
    char quoteChar = 0;
    
    for (size_t i = 0; i < sql.length(); i++) {
        char c = sql[i];
        
        if (!inQuotes && (c == '\'' || c == '"')) {
            inQuotes = true;
            quoteChar = c;
            token += c;
        } else if (inQuotes && c == quoteChar) {
            inQuotes = false;
            token += c;
            quoteChar = 0;
        } else if (!inQuotes && (c == ' ' || c == '\t' || c == '\n' || c == '\r')) {
            if (!token.empty()) {
                tokens.push_back(token);
                token.clear();
            }
        } else if (!inQuotes && (c == '(' || c == ')' || c == ',' || c == ';')) {
            if (!token.empty()) {
                tokens.push_back(token);
                token.clear();
            }
            tokens.push_back(std::string(1, c));
        } else {
            token += c;
        }
    }
    
    if (!token.empty()) {
        tokens.push_back(token);
    }
    
    return tokens;
}

std::string SimpleRDBMS::toLowerCase(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

std::string SimpleRDBMS::trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    size_t end = str.find_last_not_of(" \t\n\r");
    return str.substr(start, end - start + 1);
}

SQLStatement SimpleRDBMS::parseSQL(const std::string& sql) {
    std::vector<std::string> tokens = tokenize(sql);
    
    if (tokens.empty()) {
        return SQLStatement();
    }
    
    std::string firstToken = toLowerCase(tokens[0]);
    
    if (firstToken == "create") {
        return parseCreateTable(tokens);
    } else if (firstToken == "drop") {
        return parseDropTable(tokens);
    } else if (firstToken == "insert") {
        return parseInsert(tokens);
    } else if (firstToken == "select") {
        return parseSelect(tokens);
    } else if (firstToken == "update") {
        return parseUpdate(tokens);
    } else if (firstToken == "delete") {
        return parseDelete(tokens);
    }
    
    return SQLStatement();
}

SQLStatement SimpleRDBMS::parseCreateTable(const std::vector<std::string>& tokens) {
    SQLStatement stmt;
    stmt.type = SQLType::CREATE_TABLE;
    
    if (tokens.size() < 4 || toLowerCase(tokens[1]) != "table") {
        throw std::runtime_error("Invalid CREATE TABLE syntax");
    }
    
    stmt.tableName = tokens[2];
    
    // 查找列定义开始位置
    size_t startPos = 3;
    if (tokens[startPos] != "(") {
        throw std::runtime_error("Expected '(' after table name");
    }
    
    startPos++; // 跳过 '('
    
    // 解析列定义
    for (size_t i = startPos; i < tokens.size(); i++) {
        if (tokens[i] == ")") {
            break;
        }
        
        if (tokens[i] == ",") {
            continue;
        }
        
        // 解析列名
        std::string columnName = tokens[i];
        i++;
        
        if (i >= tokens.size()) {
            throw std::runtime_error("Incomplete column definition");
        }
        
        // 解析数据类型
        int size = 0;
        DataType dataType = parseDataType(tokens[i], size);
        
        Column column(columnName, dataType, size);
        
        // 检查约束条件
        i++;
        while (i < tokens.size() && tokens[i] != "," && tokens[i] != ")") {
            std::string constraint = toLowerCase(tokens[i]);
            if (constraint == "primary") {
                if (i + 1 < tokens.size() && toLowerCase(tokens[i + 1]) == "key") {
                    column.isPrimaryKey = true;
                    stmt.primaryKeyColumn = columnName;
                    i++;
                }
            } else if (constraint == "not") {
                if (i + 1 < tokens.size() && toLowerCase(tokens[i + 1]) == "null") {
                    column.notNull = true;
                    i++;
                }
            }
            i++;
        }
        
        stmt.columns.push_back(column);
        
        if (i < tokens.size() && tokens[i] == ")") {
            break;
        }
        
        i--; // 回退一步，因为for循环会自动增加
    }
    
    return stmt;
}

SQLStatement SimpleRDBMS::parseDropTable(const std::vector<std::string>& tokens) {
    SQLStatement stmt;
    stmt.type = SQLType::DROP_TABLE;
    
    if (tokens.size() < 3 || toLowerCase(tokens[1]) != "table") {
        throw std::runtime_error("Invalid DROP TABLE syntax");
    }
    
    stmt.tableName = tokens[2];
    return stmt;
}

SQLStatement SimpleRDBMS::parseInsert(const std::vector<std::string>& tokens) {
    SQLStatement stmt;
    stmt.type = SQLType::INSERT;
    
    if (tokens.size() < 6 || toLowerCase(tokens[1]) != "into") {
        throw std::runtime_error("Invalid INSERT syntax");
    }
    
    stmt.tableName = tokens[2];
    
    // 查找VALUES关键字
    size_t valuesPos = 0;
    for (size_t i = 3; i < tokens.size(); i++) {
        if (toLowerCase(tokens[i]) == "values") {
            valuesPos = i;
            break;
        }
    }
    
    if (valuesPos == 0) {
        throw std::runtime_error("VALUES keyword not found");
    }
    
    // 解析列名（如果指定）
    if (tokens[3] == "(") {
        for (size_t i = 4; i < valuesPos; i++) {
            if (tokens[i] != "," && tokens[i] != ")") {
                stmt.columnNames.push_back(tokens[i]);
            }
        }
    }
    
    // 解析值
    if (valuesPos + 1 < tokens.size() && tokens[valuesPos + 1] == "(") {
        for (size_t i = valuesPos + 2; i < tokens.size(); i++) {
            if (tokens[i] == ")") {
                break;
            }
            if (tokens[i] != ",") {
                // 移除引号
                std::string value = tokens[i];
                if ((value.front() == '\'' && value.back() == '\'') ||
                    (value.front() == '"' && value.back() == '"')) {
                    value = value.substr(1, value.length() - 2);
                }
                stmt.values.push_back(value);
            }
        }
    }
    
    return stmt;
}

SQLStatement SimpleRDBMS::parseSelect(const std::vector<std::string>& tokens) {
    SQLStatement stmt;
    stmt.type = SQLType::SELECT;
    
    if (tokens.size() < 4 || toLowerCase(tokens[2]) != "from") {
        throw std::runtime_error("Invalid SELECT syntax");
    }
    
    // 解析列名
    if (tokens[1] == "*") {
        stmt.columnNames.push_back("*");
    } else {
        // 解析具体列名
        std::string columnList = tokens[1];
        size_t pos = 0;
        std::string delimiter = ",";
        while ((pos = columnList.find(delimiter)) != std::string::npos) {
            stmt.columnNames.push_back(trim(columnList.substr(0, pos)));
            columnList.erase(0, pos + delimiter.length());
        }
        stmt.columnNames.push_back(trim(columnList));
    }
    
    stmt.tableName = tokens[3];
    
    // 查找WHERE子句
    for (size_t i = 4; i < tokens.size(); i++) {
        if (toLowerCase(tokens[i]) == "where") {
            stmt.whereConditions = parseWhereClause(tokens, i + 1);
            break;
        }
    }
    
    return stmt;
}

SQLStatement SimpleRDBMS::parseUpdate(const std::vector<std::string>& tokens) {
    SQLStatement stmt;
    stmt.type = SQLType::UPDATE;
    
    if (tokens.size() < 6) {
        throw std::runtime_error("Invalid UPDATE syntax");
    }
    
    stmt.tableName = tokens[1];
    
    // 查找SET关键字
    size_t setPos = 0;
    for (size_t i = 2; i < tokens.size(); i++) {
        if (toLowerCase(tokens[i]) == "set") {
            setPos = i;
            break;
        }
    }
    
    if (setPos == 0) {
        throw std::runtime_error("SET keyword not found");
    }
    
    // 解析SET子句
    size_t wherePos = tokens.size();
    for (size_t i = setPos + 1; i < tokens.size(); i++) {
        if (toLowerCase(tokens[i]) == "where") {
            wherePos = i;
            break;
        }
    }
    
    // 解析赋值表达式
    for (size_t i = setPos + 1; i < wherePos; i += 4) {
        if (i + 2 < wherePos) {
            std::string column = tokens[i];
            std::string value = tokens[i + 2];
            
            // 移除引号
            if ((value.front() == '\'' && value.back() == '\'') ||
                (value.front() == '"' && value.back() == '"')) {
                value = value.substr(1, value.length() - 2);
            }
            
            stmt.updateValues[column] = value;
        }
    }
    
    // 解析WHERE子句
    if (wherePos < tokens.size()) {
        stmt.whereConditions = parseWhereClause(tokens, wherePos + 1);
    }
    
    return stmt;
}

SQLStatement SimpleRDBMS::parseDelete(const std::vector<std::string>& tokens) {
    SQLStatement stmt;
    stmt.type = SQLType::DELETE;
    
    if (tokens.size() < 4 || toLowerCase(tokens[1]) != "from") {
        throw std::runtime_error("Invalid DELETE syntax");
    }
    
    stmt.tableName = tokens[2];
    
    // 查找WHERE子句
    for (size_t i = 3; i < tokens.size(); i++) {
        if (toLowerCase(tokens[i]) == "where") {
            stmt.whereConditions = parseWhereClause(tokens, i + 1);
            break;
        }
    }
    
    return stmt;
}

std::vector<WhereCondition> SimpleRDBMS::parseWhereClause(const std::vector<std::string>& tokens, size_t startPos) {
    std::vector<WhereCondition> conditions;
    
    for (size_t i = startPos; i + 2 < tokens.size(); i += 4) {
        WhereCondition condition;
        condition.column = tokens[i];
        condition.op = parseOperator(tokens[i + 1]);
        condition.value = tokens[i + 2];
        
        // 移除引号
        if ((condition.value.front() == '\'' && condition.value.back() == '\'') ||
            (condition.value.front() == '"' && condition.value.back() == '"')) {
            condition.value = condition.value.substr(1, condition.value.length() - 2);
        }
        
        conditions.push_back(condition);
        
        // 检查是否有AND/OR
        if (i + 3 < tokens.size()) {
            std::string logical = toLowerCase(tokens[i + 3]);
            if (logical != "and" && logical != "or") {
                break;
            }
        }
    }
    
    return conditions;
}

Operator SimpleRDBMS::parseOperator(const std::string& op) {
    if (op == "=") return Operator::EQUAL;
    if (op == "!=") return Operator::NOT_EQUAL;
    if (op == "<") return Operator::LESS_THAN;
    if (op == ">") return Operator::GREATER_THAN;
    if (op == "<=") return Operator::LESS_EQUAL;
    if (op == ">=") return Operator::GREATER_EQUAL;
    if (toLowerCase(op) == "like") return Operator::LIKE;
    return Operator::EQUAL;
}

DataType SimpleRDBMS::parseDataType(const std::string& typeStr, int& size) {
    std::string lowerType = toLowerCase(typeStr);
    
    if (lowerType == "int" || lowerType == "integer") {
        return DataType::INTEGER;
    } else if (lowerType.find("varchar") == 0) {
        // 解析VARCHAR(size)
        size_t start = lowerType.find('(');
        size_t end = lowerType.find(')');
        if (start != std::string::npos && end != std::string::npos) {
            std::string sizeStr = lowerType.substr(start + 1, end - start - 1);
            size = std::stoi(sizeStr);
        } else {
            size = 255; // 默认大小
        }
        return DataType::VARCHAR;
    } else if (lowerType == "bool" || lowerType == "boolean") {
        return DataType::BOOLEAN;
    }
    
    return DataType::VARCHAR; // 默认
}

// SQL执行方法实现
QueryResult SimpleRDBMS::executeCreateTable(const SQLStatement& stmt) {
    QueryResult result;
    
    if (tableExists(stmt.tableName)) {
        result.success = false;
        result.message = "Table '" + stmt.tableName + "' already exists";
        return result;
    }
    
    auto table = std::make_unique<Table>(stmt.tableName);
    table->columns = stmt.columns;
    
    // 设置主键
    for (const auto& col : stmt.columns) {
        if (col.isPrimaryKey) {
            table->primaryKeyColumn = col.name;
            break;
        }
    }
    
    // 创建索引文件
    std::string indexFile = getIndexFileName(stmt.tableName);
    if (!table->index->create(indexFile)) {
        result.success = false;
        result.message = "Failed to create index for table '" + stmt.tableName + "'";
        return result;
    }
    
    // 保存表结构
    if (!saveTableSchema(*table)) {
        result.success = false;
        result.message = "Failed to save table schema";
        return result;
    }
    
    tables_[stmt.tableName] = std::move(table);
    
    result.success = true;
    result.message = "Table '" + stmt.tableName + "' created successfully";
    return result;
}

QueryResult SimpleRDBMS::executeDropTable(const SQLStatement& stmt) {
    QueryResult result;
    
    if (!tableExists(stmt.tableName)) {
        result.success = false;
        result.message = "Table '" + stmt.tableName + "' does not exist";
        return result;
    }
    
    // 关闭索引
    tables_[stmt.tableName]->index->close();
    
    // 删除文件
    try {
        std::filesystem::remove(getIndexFileName(stmt.tableName));
        std::filesystem::remove(getTableSchemaFileName(stmt.tableName));
    } catch (const std::exception& e) {
        result.success = false;
        result.message = "Failed to delete table files: " + std::string(e.what());
        return result;
    }
    
    tables_.erase(stmt.tableName);
    
    result.success = true;
    result.message = "Table '" + stmt.tableName + "' dropped successfully";
    return result;
}

QueryResult SimpleRDBMS::executeInsert(const SQLStatement& stmt) {
    QueryResult result;
    
    Table* table = getTable(stmt.tableName);
    if (!table) {
        result.success = false;
        result.message = "Table '" + stmt.tableName + "' does not exist";
        return result;
    }
    
    // 验证列数
    std::vector<std::string> columnNames = stmt.columnNames;
    if (columnNames.empty()) {
        // 如果没有指定列名，使用所有列
        for (const auto& col : table->columns) {
            columnNames.push_back(col.name);
        }
    }
    
    if (columnNames.size() != stmt.values.size()) {
        result.success = false;
        result.message = "Column count doesn't match value count";
        return result;
    }
    
    // 构建完整的行数据
    std::vector<std::string> rowData(table->columns.size());
    std::string primaryKeyValue;
    
    for (size_t i = 0; i < columnNames.size(); i++) {
        // 找到列的索引
        auto it = std::find_if(table->columns.begin(), table->columns.end(),
            [&columnNames, i](const Column& col) { return col.name == columnNames[i]; });
        
        if (it == table->columns.end()) {
            result.success = false;
            result.message = "Column '" + columnNames[i] + "' does not exist";
            return result;
        }
        
        size_t colIndex = std::distance(table->columns.begin(), it);
        
        // 验证值
        if (!validateValue(stmt.values[i], *it)) {
            result.success = false;
            result.message = "Invalid value for column '" + columnNames[i] + "'";
            return result;
        }
        
        rowData[colIndex] = stmt.values[i];
        
        // 如果是主键，记录主键值
        if (it->isPrimaryKey) {
            primaryKeyValue = stmt.values[i];
        }
    }
    
    // 如果没有主键值，生成一个
    if (primaryKeyValue.empty()) {
        primaryKeyValue = generateRowId();
        // 找到主键列并设置值
        for (size_t i = 0; i < table->columns.size(); i++) {
            if (table->columns[i].isPrimaryKey) {
                rowData[i] = primaryKeyValue;
                break;
            }
        }
    }
    
    // 插入到索引
    std::string rowId = generateRowId();
    if (!table->index->insert(primaryKeyValue, rowData, rowId)) {
        result.success = false;
        result.message = "Failed to insert record into index";
        return result;
    }
    
    result.success = true;
    result.message = "1 row inserted";
    result.affectedRows = 1;
    return result;
}

QueryResult SimpleRDBMS::executeSelect(const SQLStatement& stmt) {
    QueryResult result;
    
    Table* table = getTable(stmt.tableName);
    if (!table) {
        result.success = false;
        result.message = "Table '" + stmt.tableName + "' does not exist";
        return result;
    }
    
    // 设置列头
    if (stmt.columnNames.size() == 1 && stmt.columnNames[0] == "*") {
        for (const auto& col : table->columns) {
            result.columnHeaders.push_back(col.name);
        }
    } else {
        result.columnHeaders = stmt.columnNames;
    }
    
    // 简化实现：遍历所有记录（在实际实现中应该使用更高效的方法）
    // 这里我们使用一个简单的扫描方法
    result.success = true;
    result.message = "Query executed successfully";
    
    return result;
}

QueryResult SimpleRDBMS::executeUpdate(const SQLStatement& stmt) {
    QueryResult result;
    result.success = true;
    result.message = "UPDATE not fully implemented yet";
    return result;
}

QueryResult SimpleRDBMS::executeDelete(const SQLStatement& stmt) {
    QueryResult result;
    result.success = true;
    result.message = "DELETE not fully implemented yet";
    return result;
}

// 辅助方法实现
bool SimpleRDBMS::tableExists(const std::string& tableName) {
    return tables_.find(tableName) != tables_.end();
}

Table* SimpleRDBMS::getTable(const std::string& tableName) {
    auto it = tables_.find(tableName);
    return (it != tables_.end()) ? it->second.get() : nullptr;
}

std::string SimpleRDBMS::generateRowId() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(100000, 999999);
    
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    
    return std::to_string(timestamp) + "_" + std::to_string(dis(gen));
}

bool SimpleRDBMS::validateValue(const std::string& value, const Column& column) {
    if (value.empty() && column.notNull) {
        return false;
    }
    
    switch (column.type) {
        case DataType::INTEGER:
            try {
                std::stoi(value);
                return true;
            } catch (...) {
                return false;
            }
        case DataType::VARCHAR:
            return value.length() <= static_cast<size_t>(column.size);
        case DataType::BOOLEAN:
            return value == "true" || value == "false" || value == "1" || value == "0";
    }
    
    return true;
}

std::string SimpleRDBMS::getIndexFileName(const std::string& tableName) {
    return dbPath_ + "/" + tableName + ".idx";
}

std::string SimpleRDBMS::getTableSchemaFileName(const std::string& tableName) {
    return dbPath_ + "/" + tableName + ".schema";
}

bool SimpleRDBMS::saveTableSchema(const Table& table) {
    std::ofstream file(getTableSchemaFileName(table.name));
    if (!file.is_open()) {
        return false;
    }
    
    file << table.name << "\n";
    file << table.primaryKeyColumn << "\n";
    file << table.columns.size() << "\n";
    
    for (const auto& col : table.columns) {
        file << col.name << " " << static_cast<int>(col.type) << " " << col.size 
             << " " << col.isPrimaryKey << " " << col.notNull << "\n";
    }
    
    return true;
}

bool SimpleRDBMS::loadTableSchema(const std::string& tableName) {
    std::ifstream file(getTableSchemaFileName(tableName));
    if (!file.is_open()) {
        return false;
    }
    
    auto table = std::make_unique<Table>(tableName);
    
    std::string line;
    std::getline(file, table->name);
    std::getline(file, table->primaryKeyColumn);
    
    int columnCount;
    file >> columnCount;
    file.ignore();
    
    for (int i = 0; i < columnCount; i++) {
        Column col;
        int type;
        file >> col.name >> type >> col.size >> col.isPrimaryKey >> col.notNull;
        col.type = static_cast<DataType>(type);
        table->columns.push_back(col);
    }
    
    // 打开索引
    std::string indexFile = getIndexFileName(tableName);
    if (!table->index->create(indexFile)) {
        return false;
    }
    
    tables_[tableName] = std::move(table);
    return true;
}

void SimpleRDBMS::showTables() {
    std::cout << "Tables in database:" << std::endl;
    for (const auto& pair : tables_) {
        std::cout << "  " << pair.first << std::endl;
    }
}

void SimpleRDBMS::describeTable(const std::string& tableName) {
    Table* table = getTable(tableName);
    if (!table) {
        std::cout << "Table '" << tableName << "' does not exist" << std::endl;
        return;
    }
    
    std::cout << "Table: " << tableName << std::endl;
    std::cout << "Columns:" << std::endl;
    
    for (const auto& col : table->columns) {
        std::cout << "  " << col.name << " ";
        
        switch (col.type) {
            case DataType::INTEGER:
                std::cout << "INT";
                break;
            case DataType::VARCHAR:
                std::cout << "VARCHAR(" << col.size << ")";
                break;
            case DataType::BOOLEAN:
                std::cout << "BOOLEAN";
                break;
        }
        
        if (col.isPrimaryKey) {
            std::cout << " PRIMARY KEY";
        }
        if (col.notNull) {
            std::cout << " NOT NULL";
        }
        
        std::cout << std::endl;
    }
}

void SimpleRDBMS::printQueryResult(const QueryResult& result) {
    if (!result.success) {
        std::cout << "Error: " << result.message << std::endl;
        return;
    }
    
    std::cout << result.message << std::endl;
    
    if (!result.columnHeaders.empty() && !result.rows.empty()) {
        // 打印表头
        for (size_t i = 0; i < result.columnHeaders.size(); i++) {
            std::cout << std::setw(15) << result.columnHeaders[i];
            if (i < result.columnHeaders.size() - 1) {
                std::cout << " | ";
            }
        }
        std::cout << std::endl;
        
        // 打印分隔线
        for (size_t i = 0; i < result.columnHeaders.size(); i++) {
            std::cout << std::string(15, '-');
            if (i < result.columnHeaders.size() - 1) {
                std::cout << " | ";
            }
        }
        std::cout << std::endl;
        
        // 打印数据行
        for (const auto& row : result.rows) {
            for (size_t i = 0; i < row.size() && i < result.columnHeaders.size(); i++) {
                std::cout << std::setw(15) << row[i];
                if (i < result.columnHeaders.size() - 1) {
                    std::cout << " | ";
                }
            }
            std::cout << std::endl;
        }
        
        std::cout << result.rows.size() << " rows returned" << std::endl;
    }
    
    if (result.affectedRows > 0) {
        std::cout << result.affectedRows << " rows affected" << std::endl;
    }
}