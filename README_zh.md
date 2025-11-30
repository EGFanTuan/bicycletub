# Bicycletub（中文指南）

一个包含简单基础设施(缓冲池管理器、磁盘调度)与基础算法(B+ 树、块嵌套循环连接)等实现的 C++ 项目，并配套完整的测试用例。

## 推荐
- 使用vscode + CMake Extension
- 这能为您提供良好的GUI构建体验和调试支持

## 环境准备

- Linux（bash Shell），在 Ubuntu 等发行版上测试通过
- CMake ≥ 3.20（仓库构建目录显示为 3.28）
- C++ 编译器（GCC 或 Clang），推荐 GCC 11+
- Make 或 Ninja（可选，默认生成 Makefile）
- 在 Windows 下您可以使用 MinGW 或 WSL

快速检查版本：

```bash
cmake --version
gcc --version
```

## 编译项目

```bash
# 在仓库根目录执行
mkdir -p build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

说明：
- 调试模式使用 `-DCMAKE_BUILD_TYPE=Debug`。
- 如果偏好 Ninja，可在配置时加入 `-G Ninja`。

## 运行全部测试（CTest）

构建完成后，使用 CTest 运行测试：

```bash
ctest --test-dir build --output-on-failure
```

这会发现并执行所有已注册的测试（包含 B+ 树、BNLJ、缓冲池管理器等）。

可按名称模式过滤测试：

```bash
# 仅运行 B+ 树相关测试
ctest --test-dir build -R b_plus_tree --output-on-failure

# 仅运行 bicycletub 相关测试
ctest --test-dir build -R bicycletub --output-on-failure
```

## 直接运行指定测试二进制

除了 CTest，你也可以直接运行 `build/` 下的测试可执行文件：

- `build/b_plus_tree_tests`
- `build/bicycletub_tests`
- `build/bnlj_test` / `build/bnlj_stress_test`
- `build/buffer_pool_manager_test`

示例：

```bash
# 运行 B+ 树测试集合（其中长跑用例被禁用）
./build/b_plus_tree_tests

# 运行 bicycletub 测试集合
./build/bicycletub_tests
```

部分测试把多个用例打包为一个可执行文件；可查看该可执行的命令行参数或环境变量支持（如有），不确定时建议用 CTest + `-R` 过滤。

## 增量构建与回归测试

当你修改源代码（例如 `src/b_plus_tree.cpp`）或测试（例如 `tests/` 下的文件）：

```bash
cmake --build build -j
ctest --test-dir build --output-on-failure
```

## 常见问题排查

- 配置失败：确认编译器与 CMake 版本足够新（GCC 11+、CMake 3.20+）。
- 链接错误：尝试全新构建：
  ```bash
  rm -rf build
  mkdir -p build
  cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
  cmake --build build -j
  ```
- 测试未被发现：列出被 CTest 识别的测试：
  ```bash
  ctest -N --test-dir build
  ```
- 缺少可执行文件：查看可用构建目标：
  ```bash
  cmake --build build --target help | grep tests
  ```
- 构建日志过少：配置时加 `-DCMAKE_VERBOSE_MAKEFILE=ON`，或在使用 Make 构建时加入 `VERBOSE=1`。

## 项目结构

- `src/` —— 源码
- `include/` —— 头文件
- `tests/` —— 单元与压力测试
- `build/` —— 运行 CMake 后生成的构建产物
