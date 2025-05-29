#!/bin/bash

# Build script for B+Tree Database and RDBMS System (Debug Version)
# Usage: ./build.sh [options]
# Options:
#   -c, --clean     Clean build directory before building
#   -r, --run-btree Run B+Tree test after building
#   -R, --run-rdbms Run RDBMS test after building
#   -t, --test      Run both tests after building
#   -v, --verbose   Verbose output
#   -h, --help      Show this help

set -e  # Exit on any error

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 默认选项
CLEAN_BUILD=false
RUN_BTREE=false
RUN_RDBMS=false
VERBOSE=false
BUILD_DIR="build"

# 函数：打印带颜色的消息
print_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# 函数：显示帮助信息
show_help() {
    echo "Build script for B+Tree Database and RDBMS System (Debug Version)"
    echo ""
    echo "Usage: $0 [options]"
    echo ""
    echo "Options:"
    echo "  -c, --clean     Clean build directory before building"
    echo "  -r, --run-btree Run B+Tree test after building"
    echo "  -R, --run-rdbms Run RDBMS test after building"
    echo "  -t, --test      Run both tests after building"
    echo "  -v, --verbose   Verbose output"
    echo "  -h, --help      Show this help"
    echo ""
    echo "Examples:"
    echo "  $0                    # Basic debug build"
    echo "  $0 -c                 # Clean build"
    echo "  $0 -c -t              # Clean build and run tests"
    echo "  $0 -r                 # Build and run B+Tree test"
    echo "  $0 -v                 # Verbose build"
}

# 解析命令行参数
while [[ $# -gt 0 ]]; do
    case $1 in
        -c|--clean)
            CLEAN_BUILD=true
            shift
            ;;
        -r|--run-btree)
            RUN_BTREE=true
            shift
            ;;
        -R|--run-rdbms)
            RUN_RDBMS=true
            shift
            ;;
        -t|--test)
            RUN_BTREE=true
            RUN_RDBMS=true
            shift
            ;;
        -v|--verbose)
            VERBOSE=true
            shift
            ;;
        -h|--help)
            show_help
            exit 0
            ;;
        *)
            print_error "Unknown option: $1"
            show_help
            exit 1
            ;;
    esac
done

# 开始构建过程
print_info "Starting Debug Build Process..."

# 检查是否存在CMakeLists.txt
if [ ! -f "CMakeLists.txt" ]; then
    print_error "CMakeLists.txt not found in current directory!"
    print_error "Please run this script from the project root directory."
    exit 1
fi

# 清理构建目录（如果需要）
if [ "$CLEAN_BUILD" = true ]; then
    print_info "Cleaning build directory..."
    if [ -d "$BUILD_DIR" ]; then
        rm -rf "$BUILD_DIR"
        print_success "Build directory cleaned"
    else
        print_warning "Build directory doesn't exist, skipping clean"
    fi
fi

# 创建构建目录
if [ ! -d "$BUILD_DIR" ]; then
    print_info "Creating build directory..."
    mkdir -p "$BUILD_DIR"
fi

# 进入构建目录
cd "$BUILD_DIR"

# 配置CMake (Debug模式)
print_info "Configuring CMake for Debug build..."
CMAKE_ARGS="-DCMAKE_BUILD_TYPE=Debug"

if [ "$VERBOSE" = true ]; then
    CMAKE_ARGS="$CMAKE_ARGS -DCMAKE_VERBOSE_MAKEFILE=ON"
fi

if cmake $CMAKE_ARGS ..; then
    print_success "CMake configuration completed"
else
    print_error "CMake configuration failed"
    exit 1
fi

# 编译项目
print_info "Building project..."
if [ "$VERBOSE" = true ]; then
    MAKE_ARGS="VERBOSE=1"
else
    MAKE_ARGS=""
fi

if make $MAKE_ARGS; then
    print_success "Build completed successfully!"
else
    print_error "Build failed"
    exit 1
fi

# 显示生成的可执行文件
print_info "Generated executables:"
if [ -f "bplus_tree_test" ]; then
    echo "  - bplus_tree_test"
fi
if [ -f "rdbms_test" ]; then
    echo "  - rdbms_test"
fi

# 运行测试（如果需要）
if [ "$RUN_BTREE" = true ]; then
    print_info "Running B+Tree test..."
    echo "----------------------------------------"
    if ./bplus_tree_test; then
        print_success "B+Tree test completed"
    else
        print_error "B+Tree test failed"
        exit 1
    fi
    echo "----------------------------------------"
fi

if [ "$RUN_RDBMS" = true ]; then
    print_info "Running RDBMS test..."
    echo "----------------------------------------"
    if ./rdbms_test; then
        print_success "RDBMS test completed"
    else
        print_error "RDBMS test failed"
        exit 1
    fi
    echo "----------------------------------------"
fi

# 完成
print_success "All operations completed successfully!"
print_info "You can now run the executables from the build directory:"
print_info "  ./build/bplus_tree_test"
print_info "  ./build/rdbms_test"

# 显示额外的可用命令
print_info "Additional commands available:"
print_info "  make run-btree          # Run B+Tree test"
print_info "  make run-rdbms          # Run RDBMS test"
print_info "  make memcheck-btree     # Run B+Tree test with Valgrind"
print_info "  make memcheck-rdbms     # Run RDBMS test with Valgrind"
print_info "  make help               # Show all available targets"