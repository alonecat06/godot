# Tasks

- [x] Task 1: 扩展 populate_database() — Memory、Message、Plot 数据桥接
  - [x] 1.1: 在 populate_database() 中添加 Memory allocation 桥接：遍历 Worker 的 MemNameMap，调用 db->insert_allocation()
  - [x] 1.2: 在 populate_database() 中添加 Message 桥接：遍历 Worker 的 GetMessages()，调用 db->insert_message()
  - [x] 1.3: 在 InsightsDatabase 中添加 PlotPointRecord 结构、plot_points 容器、insert_plot_point() 方法
  - [x] 1.4: 在 populate_database() 中添加 Plot 数据桥接：遍历 Worker 的 GetPlots()，调用 db->insert_plot_point()
  - [x] 1.5: 在 InsightsDatabase 中添加 LockEventRecord 结构、lock_events 容器、insert_lock_event() 方法

- [x] Task 2: 新增 TracyBridge 查询 API — Lock 和 Callstack
  - [x] 2.1: 在 insights_tracy_bridge.h 中声明 get_lock_events() 方法
  - [x] 2.2: 在 insights_tracy_bridge.cpp 中实现 get_lock_events()：遍历 Worker 的 GetLockMap()，返回 Lock 事件数组
  - [x] 2.3: 在 insights_tracy_bridge.h 中声明 get_callstack(int p_srcloc) 方法
  - [x] 2.4: 在 insights_tracy_bridge.cpp 中实现 get_callstack()：查询 Worker 的 GetCallstack()，返回调用栈帧数组
  - [x] 2.5: 在 populate_database() 中桥接 Lock 事件数据
  - [x] 2.6: 绑定 GDScript 方法

- [x] Task 3: GPU Zone Timeline 渲染
  - [x] 3.1: 修改 InsightsTimeline 在 GPU channel 行上渲染 GPU Zone 矩形
  - [x] 3.2: Zone 点击回调处理 GPU Zone 详情显示

- [x] Task 4: 创建 InsightsMemoryPanel
  - [x] 4.1: 创建 insights_memory_panel.h/.cpp，继承 VBoxContainer
  - [x] 4.2: 实现表格视图：使用 Tree 控件显示列 Address、Size、Alloc Time、Free Time、Thread
  - [x] 4.3: 实现 update_data(const Ref<InsightsDatabase> &p_db) 方法，从 db 查询 allocation 数据填充表格

- [x] Task 5: 创建 InsightsPlotPanel
  - [x] 5.1: 创建 insights_plot_panel.h/.cpp，继承 VBoxContainer
  - [x] 5.2: 实现曲线绘制：使用自定义 _draw() 绘制 Plot 数据点的折线图
  - [x] 5.3: 实现 update_data(const Ref<InsightsDatabase> &p_db) 方法，从 db 查询 plot_points 填充曲线

- [x] Task 6: 创建 InsightsMessagePanel
  - [x] 6.1: 创建 insights_message_panel.h/.cpp，继承 VBoxContainer
  - [x] 6.2: 实现日志列表：使用 ItemList 显示时间戳 + 消息文本
  - [x] 6.3: 实现 update_data(const Ref<InsightsDatabase> &p_db) 方法，从 db 查询 messages 填充列表

- [x] Task 7: 集成面板到 InsightsDock
  - [x] 7.1: 在 InsightsDock 中添加 Memory/Plots/Messages 标签页（使用 TabContainer 或手动切换）
  - [x] 7.2: 在 set_database() 调用时更新所有面板的数据
  - [x] 7.3: 在 TracyBridge 刷新定时器回调中也更新面板数据

- [x] Task 8: 生成 .tracy 测试数据
  - [x] 8.1: 创建 tracy_test_data.h 测试辅助文件
  - [x] 8.2: 使用 Tracy Import API 生成包含 CPU Zone、Message、Plot 的 .tracy 文件

- [x] Task 9: 编译验证和基础测试
  - [x] 9.1: 非 Tracy 构建编译通过（23秒）
  - [x] 9.2: Tracy 构建需要 profiler_path 指向 Tracy 源码（当前环境不可用，代码已有条件编译隔离）

- [x] Task 10: 单元测试
  - [x] 10.1: 创建 `tests/test_insights_phase10.h`
  - [x] 10.2: 测试 get_lock_events 无 Worker 时返回空数组
  - [x] 10.3: 测试 get_callstack 无 Worker 时返回空数组
  - [x] 10.4: 测试 InsightsDatabase insert_plot_point 和查询
  - [x] 10.5: 测试 InsightsDatabase insert_lock_event 和查询
  - [x] 10.6: 测试 InsightsDatabase insert_message 和查询
  - [x] 10.7: 测试 InsightsDatabase GPU zone count
  - [x] 10.8: 测试 InsightsDatabase save/load round-trip 含新数据类型
  - [x] 10.9: 测试 populate_database 无 Worker 时新数据类型为空

# Task Dependencies
- Task 2 depends on Task 1
- Task 3 depends on Task 1
- Task 4 depends on Task 1
- Task 5 depends on Task 1
- Task 6 depends on Task 1
- Task 7 depends on Task 4, Task 5, Task 6
- Task 8 depends on Task 7
- Task 9 depends on Task 7, Task 8
- Task 10 depends on Task 1, Task 2, Task 9
