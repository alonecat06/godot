# Tasks

- [x] Task 1: 复制 Tracy Server 源码到模块目录
  - [x] 1.1: 创建 `modules/insights/tracy_server/` 目录，复制 Tracy server/ 下的必需源文件
  - [x] 1.2: 创建 `modules/insights/tracy_common/` 目录，复制 Tracy public/common/ 下的必需源文件
  - [x] 1.3: 对源文件进行最小化适配（include 路径修复、条件编译宏添加）

- [x] Task 2: 修改 SCsub 构建脚本
  - [x] 2.1: 在 SCsub 中添加条件编译逻辑，检测 `profiler=tracy` 时定义 `TRACY_SERVER_ENABLED`
  - [x] 2.2: 添加 tracy_server/ 和 tracy_common/ 的源文件编译
  - [x] 2.3: 添加必要的 include 路径和编译宏（`TRACY_NO_SYMBOL_CODE`, `TRACY_NO_STATISTICS`, `TRACY_NO_FRAME_IMAGE`）
  - [x] 2.4: 编译验证通过

- [x] Task 3: 实现 InsightsTracyBridge 类
  - [x] 3.1: 创建 `insights_tracy_bridge.h`，声明 InsightsTracyBridge 类
  - [x] 3.2: 实现连接管理（connect_to_client, disconnect）
  - [x] 3.3: 实现数据查询 API（get_thread_list, get_thread_zones, get_frame_sets 等）
  - [x] 3.4: 实现 .tracy 文件保存（save_tracy_file）和加载（load_tracy_file）
  - [x] 3.5: 用 `#ifdef TRACY_SERVER_ENABLED` 包裹所有 Tracy Server 相关代码

- [x] Task 4: 修改 InsightsDock 集成 TracyBridge
  - [x] 4.1: 在 InsightsDock 中添加 InsightsTracyBridge 成员（条件编译）
  - [x] 4.2: 修改 `_on_start_pressed`：优先使用 TracyBridge 连接，回退到 NativeCapture
  - [x] 4.3: 修改 `_on_stop_pressed`：断开 TracyBridge，自动保存 .tracy 文件
  - [x] 4.4: 修改 `_on_open_pressed`：支持打开 .tracy 文件（通过 TracyBridge 加载）

- [x] Task 5: 注册和初始化
  - [x] 5.1: 在 register_types.cpp 中注册 InsightsTracyBridge 类（条件编译）
  - [x] 5.2: 在 config.py 的 get_doc_classes 中添加 InsightsTracyBridge

- [x] Task 6: 编译验证和基础测试
  - [x] 6.1: 非 Tracy 构建编译通过（无 TRACY_SERVER_ENABLED）
  - [x] 6.2: Tracy 构建编译通过（profiler=tracy）
  - [x] 6.3: 启动引擎，验证 Insights 面板正常显示

- [x] Task 7: 单元测试
  - [x] 7.1: 创建 `tests/test_insights_phase7.h`，编写 InsightsTracyBridge 单元测试
  - [x] 7.2: 测试 connect_to_client / disconnect 生命周期
  - [x] 7.3: 测试 save_tracy_file 无数据时返回错误
  - [x] 7.4: 测试 load_tracy_file 无效文件返回错误
  - [x] 7.5: 测试 get_thread_list / get_frame_sets 查询 API 空数据返回正确格式
  - [x] 7.6: 测试条件编译隔离（整个测试文件被 #ifdef TRACY_SERVER_ENABLED 包裹）

# Task Dependencies
- Task 2 depends on Task 1
- Task 3 depends on Task 1
- Task 4 depends on Task 3
- Task 5 depends on Task 3
- Task 6 depends on Task 2, Task 3, Task 4, Task 5
- Task 7 depends on Task 3, Task 6
