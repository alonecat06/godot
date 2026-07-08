# Tasks

- [x] Task 1: 实现 InsightsTracyBridge::populate_database()
  - [x] 1.1: 在 insights_tracy_bridge.h 中声明 `Error populate_database(const Ref<InsightsDatabase> &p_db)` 方法（条件编译）
  - [x] 1.2: 在 insights_tracy_bridge.cpp 中实现 populate_database：遍历 Worker 的线程数据，将每个 Zone 通过 `p_db->insert_zone()` 写入
  - [x] 1.3: 实现 GPU Zone 桥接：遍历 Worker 的 GPU 上下文，将 GPU Zone 写入 InsightsDatabase，channel="gpu"
  - [x] 1.4: 实现帧标记桥接：遍历 Worker 的 FrameData，调用 `p_db->insert_frame_marker()`
  - [x] 1.5: 绑定 GDScript 方法

- [x] Task 2: 修改 InsightsDock Start 流程 — TracyBridge 实时录制
  - [x] 2.1: 在 InsightsDock 中添加 Timer *tracy_refresh_timer 成员（条件编译）
  - [x] 2.2: Start 成功连接 TracyBridge 后：创建 InsightsDatabase，启动 500ms 定时器，定时器回调调用 populate_database + set_database
  - [x] 2.3: Start 成功后更新按钮状态：Start 禁用、Stop 启用、显示 "Recording..."

- [x] Task 3: 修改 InsightsDock Stop 流程 — TracyBridge 停止录制
  - [x] 3.1: Stop 时停止定时器，最终调用 populate_database 确保数据完整
  - [x] 3.2: 断开 TracyBridge 连接，保存 .tracy 文件
  - [x] 3.3: 更新按钮状态

- [x] Task 4: 修改 InsightsDock Open 流程 — .tracy 文件加载
  - [x] 4.1: 加载 .tracy 成功后，创建 InsightsDatabase，调用 populate_database
  - [x] 4.2: 调用 set_database 将数据设置到 UI

- [x] Task 5: 补充 InsightsTracyBridge 查询 API
  - [x] 5.1: get_thread_list 返回 zone_count 统计（遍历 td->timeline 计数）
  - [x] 5.2: 添加 get_zone_count() 方法返回 Worker 中总 Zone 数量

- [x] Task 6: 编译验证和基础测试
  - [x] 6.1: 非 Tracy 构建编译通过
  - [x] 6.2: Tracy 构建编译通过
  - [x] 6.3: 启动引擎，验证 Insights 面板正常显示

- [x] Task 7: 单元测试
  - [x] 7.1: 创建 `tests/test_insights_phase8.h`
  - [x] 7.2: 测试 populate_database 在无 Worker 时返回错误
  - [x] 7.3: 测试 populate_database 写入 InsightsDatabase 后 zone_count 为 0（无数据源）
  - [x] 7.4: 测试 TracyBridge 的 get_zone_count 方法
  - [x] 7.5: 测试条件编译隔离（非 TRACY_SERVER_ENABLED 时测试文件为空）
  - [x] 7.6: 测试 get_thread_list 返回正确格式

# Task Dependencies
- Task 2 depends on Task 1
- Task 3 depends on Task 2
- Task 4 depends on Task 1
- Task 5 depends on Task 1
- Task 6 depends on Task 2, Task 3, Task 4, Task 5
- Task 7 depends on Task 1, Task 5, Task 6
