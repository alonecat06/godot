# 桥接层 + 实时录制 Spec (Phase 8)

## Why
Phase 7 已完成 Tracy Server 编译集成和 InsightsTracyBridge 的 API 骨架，但现有 UI（Timeline、Flamegraph 等）仅使用 InsightsDatabase 作为数据源，无法直接渲染来自 Tracy Worker 的实时数据。Phase 8 需要将 TracyBridge 的查询结果转换为 InsightsDatabase 格式，使现有 UI 无需修改即可展示实时录制的 CPU Zone 和帧标记数据。

## What Changes
- 新增 `InsightsTracyBridge::populate_database()` 方法，将 Worker 数据批量写入 InsightsDatabase
- 修改 InsightsDock 的 Start/Stop 流程：使用 TracyBridge 时，停止录制后自动 populate_database 并设置到 UI
- 修改 InsightsDock 的 Open .tracy 流程：加载 .tracy 文件后自动 populate_database
- 新增实时刷新 Timer：录制过程中周期性调用 populate_database 更新 UI
- 补充 TracyBridge 查询 API 中缺失的 zone_count 统计

## Impact
- Affected code: `modules/insights/insights_tracy_bridge.h/.cpp`, `modules/insights/editor/insights_dock.cpp`
- Affected specs: Phase 7 (InsightsTracyBridge API 扩展)
- 无破坏性变更，现有 NativeCapture 回退路径不受影响

## ADDED Requirements

### Requirement: TracyBridge → InsightsDatabase 数据桥接
系统 SHALL 提供 InsightsTracyBridge::populate_database() 方法，将 Tracy Worker 的 CPU Zone、GPU Zone、帧标记数据批量写入指定的 InsightsDatabase。

#### Scenario: CPU Zone 数据桥接
- **WHEN** 调用 populate_database() 且 Worker 包含 CPU Zone 数据
- **THEN** InsightsDatabase 中包含所有线程的 Zone 记录，channel="cpu"，name/file/line 正确映射

#### Scenario: 帧标记数据桥接
- **WHEN** 调用 populate_database() 且 Worker 包含帧数据
- **THEN** InsightsDatabase 中包含帧标记记录，start_ns/end_ns 正确

#### Scenario: GPU Zone 数据桥接
- **WHEN** 调用 populate_database() 且 Worker 包含 GPU Zone 数据
- **THEN** InsightsDatabase 中包含 GPU Zone 记录，channel="gpu"

### Requirement: 实时录制自动刷新
系统 SHALL 在 TracyBridge 实时录制期间周期性更新 InsightsDatabase，使 Timeline 显示最新数据。

#### Scenario: 录制中自动刷新
- **WHEN** 用户点击 Start 且 TracyBridge 成功连接到 Client
- **THEN** InsightsDock 启动定时器，每 500ms 调用 populate_database() 更新 UI
- **THEN** Timeline 实时显示 CPU Zone 和帧标记

#### Scenario: 停止录制
- **WHEN** 用户点击 Stop
- **THEN** 停止定时器，最终 populate_database() 调用确保数据完整
- **THEN** 自动保存 .tracy 文件

### Requirement: 打开 .tracy 文件后展示数据
系统 SHALL 在加载 .tracy 文件后自动将数据写入 InsightsDatabase 并设置到 UI。

#### Scenario: 打开 .tracy 成功
- **WHEN** 用户通过 Open 按钮选择 .tracy 文件
- **THEN** TracyBridge 加载文件，populate_database() 将数据写入 InsightsDatabase
- **THEN** Timeline 显示完整数据

### Requirement: TracyBridge 连接生命周期与按钮状态同步
系统 SHALL 确保 Start/Stop 按钮状态与 TracyBridge 连接状态一致。

#### Scenario: TracyBridge 连接成功
- **WHEN** Start 成功连接到 Tracy Client
- **THEN** Start 按钮禁用，Stop 按钮启用，显示 "Recording..." 文本

#### Scenario: TracyBridge 连接失败
- **WHEN** Start 连接失败
- **THEN** 回退到 NativeCapture，按钮状态与现有行为一致

## MODIFIED Requirements

### Requirement: InsightsDock Start/Stop 流程
现有 InsightsDock 的 Start/Stop 流程在 Tracy 构建下优先使用 TracyBridge 连接，停止后自动保存 .tracy 文件。Phase 8 扩展此流程，在停止后（或录制中）调用 populate_database() 将数据写入 InsightsDatabase 供 UI 使用。

### Requirement: InsightsDock Open 流程
现有 Open 流程在加载 .tracy 文件后仅 print_line 确认。Phase 8 扩展为加载后自动 populate_database() 并 set_database()。
