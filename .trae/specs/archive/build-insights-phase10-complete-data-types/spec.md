# 完整数据类型 Spec (Phase 10)

## Why
Phase 8 的 populate_database() 只桥接了 CPU Zone、GPU Zone 和帧标记，Tracy Worker 还包含 Memory 分配/释放、Plot/Counter 曲线、Message 日志、Lock 事件、Callstack 等丰富数据，当前 UI 无法展示。Phase 10 需要将这些数据类型完整桥接到 InsightsDatabase 并在 UI 上渲染，实现 Tracy UI 模块的 Godot 化。用户应能打开 .tracy 文件并在 Godot 编辑器中看到完整的数据可视化。

## What Changes
- 扩展 populate_database()：增加 Memory allocation、Message、Plot 数据桥接
- 新增 TracyBridge 查询 API：get_lock_events()、get_callstack()
- 扩展 InsightsDatabase：增加 insert_plot_point()、insert_lock_event() 存储方法和对应容器
- Timeline 渲染 GPU Zone 行（gpu channel 行上显示 GPU Zone 矩形）
- 新增 InsightsMemoryPanel：显示内存分配/释放事件列表
- 新增 InsightsPlotPanel：显示 Plot/Counter 曲线图
- 新增 InsightsMessagePanel：显示 Message 日志列表
- 生成 .tracy 测试数据文件用于端到端验证

## Impact
- Affected code: `modules/insights/insights_tracy_bridge.h/.cpp`, `modules/insights/insights_core/insights_database.h/.cpp`, `modules/insights/editor/insights_dock.h/.cpp`, `modules/insights/editor/insights_timeline.h/.cpp`
- 新增文件: `insights_memory_panel.h/.cpp`, `insights_plot_panel.h/.cpp`, `insights_message_panel.h/.cpp`
- 新增测试数据: `tests/data/test_capture.tracy`
- 无破坏性变更

## ADDED Requirements

### Requirement: Memory 数据桥接
系统 SHALL 将 Tracy Worker 的内存分配/释放事件桥接到 InsightsDatabase。

#### Scenario: Memory 数据桥接
- **WHEN** populate_database() 且 Worker 包含 Memory 数据
- **THEN** InsightsDatabase 中包含 allocation 记录（ptr, size, alloc_ns, free_ns, thread_id）
- **THEN** InsightsMemoryPanel 显示内存分配/释放事件列表

### Requirement: Message 数据桥接
系统 SHALL 将 Tracy Worker 的 Message 日志桥接到 InsightsDatabase。

#### Scenario: Message 数据桥接
- **WHEN** populate_database() 且 Worker 包含 Message 数据
- **THEN** InsightsDatabase 中包含 message 记录（level, text, timestamp_ns）
- **THEN** InsightsMessagePanel 显示消息日志列表

### Requirement: Plot 数据桥接
系统 SHALL 将 Tracy Worker 的 Plot/Counter 数据桥接到 InsightsDatabase。

#### Scenario: Plot 数据桥接
- **WHEN** populate_database() 且 Worker 包含 Plot 数据
- **THEN** InsightsDatabase 中包含 plot_point 记录（name, time_ns, value）
- **THEN** InsightsPlotPanel 显示 Plot 曲线图

### Requirement: GPU Zone 渲染
系统 SHALL 在 Timeline 的 GPU channel 行上渲染 GPU Zone 矩形。

#### Scenario: GPU Zone 渲染
- **WHEN** InsightsDatabase 包含 GPU Zone 数据
- **THEN** Timeline 在 gpu channel 行上显示 GPU Zone 矩形（名称、时间段）
- **THEN** 点击 GPU Zone 显示详情面板

### Requirement: Lock 事件查询
系统 SHALL 提供 get_lock_events() 方法查询 Lock 等待/持有事件。

#### Scenario: Lock 事件查询
- **WHEN** TracyBridge 持有数据且包含 Lock 事件
- **THEN** get_lock_events() 返回 Array，每个元素包含 srcloc, time_ns, lock_id, type

### Requirement: Callstack 查询
系统 SHALL 提供 get_callstack() 方法查询指定 Zone 的调用栈。

#### Scenario: Callstack 查询
- **WHEN** TracyBridge 持有数据且 Zone 有调用栈信息
- **THEN** get_callstack(zone_srcloc) 返回 Array，每个元素包含 function, file, line

### Requirement: InsightsMemoryPanel
系统 SHALL 提供内存分配面板，显示内存事件的表格视图。

#### Scenario: 显示内存面板
- **WHEN** 用户切换到 Memory 标签页
- **THEN** 显示内存分配/释放事件列表（Address、Size、Alloc Time、Free Time、Thread）

### Requirement: InsightsPlotPanel
系统 SHALL 提供 Plot 曲线面板，绘制 Counter 值随时间的变化曲线。

#### Scenario: 显示 Plot 面板
- **WHEN** 用户切换到 Plots 标签页
- **THEN** 显示所有 Plot 的曲线图，X 轴为时间，Y 轴为值

### Requirement: InsightsMessagePanel
系统 SHALL 提供消息日志面板，显示 Tracy 消息的时间排序列表。

#### Scenario: 显示消息面板
- **WHEN** 用户切换到 Messages 标签页
- **THEN** 显示消息日志列表（时间、文本）

### Requirement: .tracy 测试数据
系统 SHALL 提供一个预生成的 .tracy 测试数据文件，用于端到端验证数据加载和渲染。

#### Scenario: 打开测试 .tracy 文件
- **WHEN** 用户通过 Open 按钮打开 test_capture.tracy
- **THEN** TracyBridge 加载成功，populate_database() 桥接所有数据类型
- **THEN** Timeline 显示 CPU/GPU Zone，Memory/Plot/Message 面板显示对应数据

## MODIFIED Requirements

### Requirement: populate_database 扩展
Phase 8 的 populate_database() 只桥接 CPU Zone、GPU Zone、帧标记。Phase 10 扩展为桥接全部数据类型（Memory、Message、Plot）。
