# B+树数据库管理系统

一个高性能的B+树数据库管理系统实现，支持磁盘持久化、内存缓冲池管理和完整的CRUD操作。

## 🌟 项目特性

### 核心功能
- **完整的B+树实现**：支持插入、删除、查询操作
- **磁盘持久化**：数据可靠存储到磁盘文件
- **智能缓冲池**：LRU算法管理内存页面，提高访问效率
- **自动节点分裂与合并**：维护树的平衡性
- **事务安全**：脏页管理和一致性保证

### 性能特性
- **高效的页面管理**：4KB页面大小，优化磁盘I/O
- **内存优化**：可配置缓冲池大小，控制内存使用
- **批量操作支持**：高效的批量插入和查询
- **统计信息收集**：树高度、填充率、命中率等性能指标

## 📋 系统要求

- **编译器**：支持C++17标准的编译器（GCC 7+, Clang 6+, MSVC 2019+）
- **CMake**：版本3.10或更高
- **内存**：推荐至少512MB可用内存
- **磁盘空间**：根据数据量需求而定

### 可选依赖
- **Valgrind**：用于内存泄漏检测（Linux系统）

## 🚀 快速开始

### 编译构建

```bash
# 克隆项目
git clone <repository-url>
cd bplus-tree-db

# 创建构建目录
mkdir build && cd build

# 配置项目
cmake ..

# 编译项目
make

# 查看所有可用目标
make show-help
```

### 构建选项

```bash
# Debug模式（用于开发调试）
cmake -DCMAKE_BUILD_TYPE=Debug ..

# Release模式（用于生产环境）
cmake -DCMAKE_BUILD_TYPE=Release ..
```

### 运行测试

```bash
# 运行简单测试（推荐首次使用）
make run-simple

# 运行完整性能测试
make run-btree

# 运行所有测试
make run-all-tests
```

## 🧪 测试说明

项目包含三个主要测试程序：

### 1. 简单功能测试 (`simple_test`)
验证基本的增删改查操作和边界情况：
```bash
make run-simple
```

### 2. 性能测试 (`bplus_tree_test`)
包含大规模数据的性能测试：
- 插入50,000条记录的性能测试
- 查询性能测试
- 内存管理测试
- 压力测试

```bash
make run-btree
```

### 3. 树结构测试 (`tree_test`)
深度验证B+树结构的正确性：
- 树高度验证
- 节点分裂行为分析
- 平衡性检查

```bash
make tree_test
```

### 内存检查

如果系统安装了Valgrind：
```bash
# 内存泄漏检查
make memcheck-simple
make memcheck-btree
make memcheck-all
```

## 📁 项目结构

```
bplus-tree-db/
├── src/
│   ├── BPlusTree.h          # B+树核心头文件
│   ├── BPlusTree.cpp        # B+树实现
│   ├── BufferPool.h         # 缓冲池头文件
│   ├── BufferPool.cpp       # 缓冲池实现
│   ├── main.cpp             # 性能测试主程序
│   ├── simple_tests.cpp     # 简单测试程序
│   └── test_tree_struct.cpp # 树结构测试程序
├── CMakeLists.txt           # CMake构建配置
├── README.md               # 项目说明文档
└── build/                  # 构建输出目录
```

## 💡 使用示例

### 基本操作示例

```cpp
#include "BPlusTree.h"

int main() {
    BPlusTree tree;
    
    // 创建数据库
    if (!tree.create("example.db", PAGE_SIZE, 100)) {
        std::cerr << "Failed to create database" << std::endl;
        return 1;
    }
    
    // 插入数据
    std::vector<std::string> value = {"Hello, World!"};
    tree.insert("key1", value, "row1");
    
    // 查询数据
    auto results = tree.get("key1");
    if (!results.empty()) {
        std::cout << "Found: " << results[0][0] << std::endl;
    }
    
    // 删除数据
    tree.remove("key1");
    
    // 获取统计信息
    TreeStats stats = tree.getStat();
    std::cout << "Tree height: " << stats.height << std::endl;
    std::cout << "Node count: " << stats.nodeCount << std::endl;
    std::cout << "Fill factor: " << stats.fillFactor << std::endl;
    
    // 关闭数据库
    tree.close();
    return 0;
}
```

## ⚙️ 配置参数

### 页面配置
- `PAGE_SIZE`: 页面大小（默认4096字节）
- `MAX_KEYS_PER_PAGE`: 每页最大键数
- `KEY_SIZE`: 键的最大长度
- `VALUE_SIZE`: 值的最大长度

### 缓冲池配置
- `bufferPoolSize`: 缓冲池大小（页面数）
- 建议值：50-1000页面，根据可用内存调整

## 📊 性能特性

### 时间复杂度
- **查询**: O(log n)
- **插入**: O(log n)
- **删除**: O(log n)

### 空间复杂度
- **磁盘存储**: O(n)
- **内存使用**: O(缓冲池大小)

### 性能指标
- **插入速度**: ~1000-5000 records/second
- **查询速度**: ~10000-50000 queries/second
- **缓冲命中率**: 通常>90%

## 🛠️ 高级功能

### 缓冲池管理
```cpp
// 设置缓冲池大小
tree.setBufferPoolSize(200);

// 获取缓冲池统计
auto stats = tree.getBufferPoolStats();
std::cout << "Hit ratio: " << stats.hitRatio << std::endl;

// 手动刷新缓冲池
tree.flushBuffer();
```

### 树状态监控
```cpp
// 打印树结构
tree.printTree();

// 打印缓冲池状态
tree.printBufferPoolStatus();

// 获取详细统计
TreeStats stats = tree.getStat();
```

## 🔧 故障排除

### 常见问题

**1. 编译错误**
- 确保使用C++17标准编译器
- 检查CMake版本是否符合要求

**2. 内存不足**
- 减少缓冲池大小
- 增加系统可用内存

**3. 性能问题**
- 增加缓冲池大小
- 检查磁盘I/O性能
- 使用SSD存储

**4. 数据损坏**
- 确保程序正常关闭
- 检查磁盘空间是否充足

### 调试技巧

```bash
# 使用Debug模式编译
cmake -DCMAKE_BUILD_TYPE=Debug ..

# 运行内存检查
make memcheck-simple

# 清理数据库文件
make clean-db
```

## 🤝 贡献指南

欢迎提交问题报告和功能请求！

### 开发流程
1. Fork项目
2. 创建功能分支
3. 编写代码和测试
4. 提交Pull Request

### 代码规范
- 遵循现有代码风格
- 添加必要的注释
- 编写相应的测试用例

## 📝 许可证

本项目采用MIT许可证，详见LICENSE文件。

## 🔗 相关资源

- [B+树算法原理](https://en.wikipedia.org/wiki/B%2B_tree)
- [数据库系统概念](https://www.db-book.com/)
- [CMake官方文档](https://cmake.org/documentation/)

---

**项目维护者**: [QCQCQC]  
**最后更新**: 2025年 5月 29日
**版本**: 1.0.0