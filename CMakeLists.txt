# CMakeLists.txt for B+Tree Database and RDBMS System
cmake_minimum_required(VERSION 3.10)
project(BPlusTreeRDBMS)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置编译选项
set(CMAKE_CXX_FLAGS "-Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g3 -O0 -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# 默认构建类型
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "Build type" FORCE)
endif()

# 包含目录
include_directories(src)

# 源文件定义
set(BTREE_SOURCES
    src/BPlusTree.cpp
    src/BufferPool.cpp
)

set(TEST_SOURCES
    src/main.cpp
)

set(SIMPLE_TEST_SOURCES
    src/simple_tests.cpp
)

set(test_tree_struct_SOURCES
    src/test_tree_struct.cpp
)

# 创建B+树测试可执行文件
add_executable(bplus_tree_test
    ${BTREE_SOURCES}
    ${TEST_SOURCES}
)

# 创建简单测试可执行文件
add_executable(simple_test
    ${BTREE_SOURCES}
    ${SIMPLE_TEST_SOURCES}
)

# 树结构测试
add_executable(tree_test
    ${BTREE_SOURCES}
    ${test_tree_struct_SOURCES}
)

# 设置输出目录（可选）
set_target_properties(bplus_tree_test simple_test tree_test
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

# 添加自定义目标
# 运行B+树测试
add_custom_target(run-btree
    COMMAND bplus_tree_test
    DEPENDS bplus_tree_test
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running B+Tree test"
)

# 运行简单测试
add_custom_target(run-simple
    COMMAND simple_test
    DEPENDS simple_test
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running Simple B+Tree test"
)

# 运行所有测试
add_custom_target(run-all-tests
    COMMAND simple_test
    COMMAND echo "----------------------------------------"
    COMMAND bplus_tree_test
    DEPENDS simple_test bplus_tree_test
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running all tests sequentially"
)

# 清理数据库文件
add_custom_target(clean-db
    COMMAND ${CMAKE_COMMAND} -E remove -f *.db *.idx *.schema
    COMMAND ${CMAKE_COMMAND} -E remove_directory test_db
    COMMAND ${CMAKE_COMMAND} -E remove_directory interactive_db
    COMMAND ${CMAKE_COMMAND} -E remove_directory perf_db
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Cleaning database files"
)

# 深度清理
add_custom_target(distclean
    COMMAND ${CMAKE_COMMAND} -E remove -f *.log core.*
    DEPENDS clean-db
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Deep cleaning all generated files"
)

# 内存检查目标（需要Valgrind）
find_program(VALGRIND_PROGRAM valgrind)
if(VALGRIND_PROGRAM)
    add_custom_target(memcheck-btree
        COMMAND ${VALGRIND_PROGRAM} --leak-check=full --show-leak-kinds=all $<TARGET_FILE:bplus_tree_test>
        DEPENDS bplus_tree_test
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Running B+Tree test with Valgrind"
    )
    
    add_custom_target(memcheck-simple
        COMMAND ${VALGRIND_PROGRAM} --leak-check=full --show-leak-kinds=all $<TARGET_FILE:simple_test>
        DEPENDS simple_test
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Running Simple test with Valgrind"
    )
    
    add_custom_target(memcheck-all
        COMMAND ${VALGRIND_PROGRAM} --leak-check=full --show-leak-kinds=all $<TARGET_FILE:simple_test>
        COMMAND echo "----------------------------------------"
        COMMAND ${VALGRIND_PROGRAM} --leak-check=full --show-leak-kinds=all $<TARGET_FILE:bplus_tree_test>
        DEPENDS simple_test bplus_tree_test
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Running all tests with Valgrind"
    )
endif()

# 安装规则
install(TARGETS bplus_tree_test simple_test
    RUNTIME DESTINATION bin
)

# 显示构建信息
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "C++ flags: ${CMAKE_CXX_FLAGS}")
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Debug flags: ${CMAKE_CXX_FLAGS_DEBUG}")
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    message(STATUS "Release flags: ${CMAKE_CXX_FLAGS_RELEASE}")
endif()

# 帮助信息（通过自定义目标）
add_custom_target(show-help
    COMMAND ${CMAKE_COMMAND} -E echo "Available targets:"
    COMMAND ${CMAKE_COMMAND} -E echo "  simple_test      - Build Simple B+Tree test executable"
    COMMAND ${CMAKE_COMMAND} -E echo "  bplus_tree_test  - Build B+Tree test executable"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Run targets:"
    COMMAND ${CMAKE_COMMAND} -E echo "  run-simple       - Build and run Simple test (recommended)"
    COMMAND ${CMAKE_COMMAND} -E echo "  run-btree        - Build and run B+Tree test"
    COMMAND ${CMAKE_COMMAND} -E echo "  run-all-tests    - Build and run all tests sequentially"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Utility targets:"
    COMMAND ${CMAKE_COMMAND} -E echo "  clean-db         - Remove database files"
    COMMAND ${CMAKE_COMMAND} -E echo "  distclean        - Remove all generated files"
    COMMAND ${CMAKE_COMMAND} -E echo "  show-help        - Show this help message"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    if(VALGRIND_PROGRAM)
        COMMAND ${CMAKE_COMMAND} -E echo "Memory check targets (Valgrind available):"
        COMMAND ${CMAKE_COMMAND} -E echo "  memcheck-simple  - Run Simple test with Valgrind"
        COMMAND ${CMAKE_COMMAND} -E echo "  memcheck-btree   - Run B+Tree test with Valgrind"
        COMMAND ${CMAKE_COMMAND} -E echo "  memcheck-all     - Run all tests with Valgrind"
        COMMAND ${CMAKE_COMMAND} -E echo ""
    endif()
    COMMAND ${CMAKE_COMMAND} -E echo "Build types:"
    COMMAND ${CMAKE_COMMAND} -E echo "  -DCMAKE_BUILD_TYPE=Debug     - Debug build"
    COMMAND ${CMAKE_COMMAND} -E echo "  -DCMAKE_BUILD_TYPE=Release   - Release build"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Quick start examples:"
    COMMAND ${CMAKE_COMMAND} -E echo "  mkdir build && cd build"
    COMMAND ${CMAKE_COMMAND} -E echo "  cmake .."
    COMMAND ${CMAKE_COMMAND} -E echo "  make show-help          # Show this help"
    COMMAND ${CMAKE_COMMAND} -E echo "  make run-simple         # Run simple test (start here)"
    COMMAND ${CMAKE_COMMAND} -E echo "  make run-all-tests      # Run comprehensive tests"
)