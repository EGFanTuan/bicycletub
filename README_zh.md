# Bicycletub（中文指南）

一个包含简单基础设施(缓冲池管理器、磁盘调度)与基础算法(B+ 树、块嵌套循环连接)等实现的 C++ 项目，并配套完整的测试用例。

## 使用 VS Code 配置（推荐）

### 安装推荐扩展

在 VS Code 中安装以下扩展以获得最佳开发体验：

- **C/C++** (ms-vscode.cpptools) - C++ 智能感知和调试
- **CMake Tools** (ms-vscode.cmake-tools) - CMake 项目支持
- **CMake** (twxs.cmake) - CMake 语法高亮

### 配置 CMake 工具链（Windows）

1. 按 `Ctrl+Shift+P` 打开命令面板
2. 输入 `CMake: Select a Kit` 并选择
3. 如果列表中没有您的工具链，选择 **"Scan for kits"** 或手动配置

## 环境准备

### 通用要求

- CMake ≥ 3.20
- C++ 编译器（GCC 11+ 或 Clang）
- Make 或 Ninja（可选，推荐使用 Ninja 以获得更快的构建速度）

快速检查版本：

```bash
cmake --version
gcc --version   # 或 g++ --version
```

### Linux

在 Ubuntu 等发行版上测试通过：

```bash
# Ubuntu/Debian
sudo apt update
sudo apt install build-essential cmake ninja-build

# Fedora
sudo dnf install gcc-c++ cmake ninja-build
```

### Windows

Windows 下有多种方式配置 C++ 编译环境：

#### 方式一：使用 Qt 自带的 MinGW 工具链（推荐）

如果您已安装 Qt（例如安装在 `G:\Qt`），可以直接使用 Qt 自带的编译器和构建工具：

```powershell
# 清理旧的构建目录（如有）
Remove-Item -Recurse -Force build

# 配置 CMake，指定 Qt 工具链路径（请根据实际安装路径修改）
cmake -S . -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Release `
    -DCMAKE_C_COMPILER="G:/Qt/Tools/mingw1310_64/bin/gcc.exe" `
    -DCMAKE_CXX_COMPILER="G:/Qt/Tools/mingw1310_64/bin/g++.exe" `
    -DCMAKE_MAKE_PROGRAM="G:/Qt/Tools/Ninja/ninja.exe"

# 构建项目
cmake --build build --config Release
```

> **提示**：Qt 的 Tools 目录通常包含 `mingw*_64`（MinGW 编译器）、`Ninja` 和 `CMake_64` 等工具。

#### 方式二：使用 Visual Studio Build Tools

1. 下载安装 [Visual Studio Build Tools](https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2022)
2. 安装时选择 **"Desktop development with C++"** 工作负载
3. 打开 **"Developer Command Prompt for VS 2022"** 或 **"x64 Native Tools Command Prompt"**
4. 在命令提示符中执行：

```cmd
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

#### 方式三：使用 MSYS2 + MinGW

1. 从 [MSYS2 官网](https://www.msys2.org/) 下载安装 MSYS2
2. 打开 MSYS2 终端，安装工具链：

```bash
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja
```

3. 将 `C:\msys64\mingw64\bin` 添加到系统 PATH
4. 在 PowerShell 中执行：

```powershell
cmake -S . -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### macOS

```bash
# 使用 Homebrew 安装
brew install cmake ninja

# Xcode Command Line Tools（如未安装）
xcode-select --install
```

## 编译项目

### Linux / macOS

```bash
# 在仓库根目录执行
mkdir -p build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

### Windows（使用 Qt MinGW 工具链示例）

```powershell
# 配置（首次或更换工具链时需要）
cmake -S . -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Release `
    -DCMAKE_C_COMPILER="G:/Qt/Tools/mingw1310_64/bin/gcc.exe" `
    -DCMAKE_CXX_COMPILER="G:/Qt/Tools/mingw1310_64/bin/g++.exe" `
    -DCMAKE_MAKE_PROGRAM="G:/Qt/Tools/Ninja/ninja.exe"

# 构建
cmake --build build --config Release
```

> **注意**：如果更换生成器（如从 NMake 切换到 Ninja），需要先删除 `build` 目录再重新配置。

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

## 许可证

请参阅 [LICENSE](LICENSE) 文件。
