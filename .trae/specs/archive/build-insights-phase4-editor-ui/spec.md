# Phase 4：编辑器 UI Spec

## Why

Phase 1-3 已完成 Insights 模块基础设施、录制核心和 GPU Profiling 实现，但所有性能数据只能通过代码 API 或 `.gitracy` 文件访问，缺乏可视化界面。Phase 4 需要实现编辑器内嵌 UI，提供时间轴、火焰图、内存瀑布、资源依赖图、网络流量图和 Trace 对比视图，使开发者能在 Godot 编辑器中直接查看和分析性能数据。

## What Changes

- **InsightsEditorPlugin**：EditorPlugin 子类，注册主屏幕和底部面板，提供录制/打开/对比菜单入口
- **InsightsDock**：底部面板主控件，包含工具栏、Channel 标签页、回放控制栏
- **InsightsTimeline**：自绘时间轴控件（CanvasItem `_draw()`），支持 CPU/GPU/Loading/Network 多 Track 渲染、缩放/平移、Zone 点击选中
- **InsightsFlamegraph**：火焰图控件，从 InsightsDatabase 构建 Zone 层级树，支持搜索过滤和颜色按 Channel 分配
- **InsightsMemoryPanel**：内存分配瀑布面板，显示分配生命周期、峰值内存、泄漏检测、大小过滤
- **InsightsLoadingPanel**：资源加载依赖图面板，构建依赖树、检测加载瓶颈
- **InsightsNetworkPanel**：网络流量面板，显示 RPC/包流量
- **InsightsComparePanel**：双 Trace 对比面板，Diff 算法检测回归/改进/新增/移除 Zone
- **单元测试**：6 个测试用例覆盖 EditorPlugin 注册、Timeline 渲染、Flamegraph 层级、Memory 生命周期、Loading 依赖树、Compare Diff

## Impact

- Affected specs: Phase 1-3（InsightsManager/InsightsDatabase API 需被 UI 消费）
- Affected code:
  - `modules/insights/editor/insights_editor_plugin.h/cpp` — 新增 EditorPlugin 入口
  - `modules/insights/editor/insights_dock.h/cpp` — 新增底部面板主控件
  - `modules/insights/editor/insights_timeline.h/cpp` — 新增时间轴控件
  - `modules/insights/editor/insights_flamegraph.h/cpp` — 新增火焰图控件
  - `modules/insights/editor/insights_memory_panel.h/cpp` — 新增内存瀑布面板
  - `modules/insights/editor/insights_loading_panel.h/cpp` — 新增资源依赖图面板
  - `modules/insights/editor/insights_network_panel.h/cpp` — 新增网络流量面板
  - `modules/insights/editor/insights_compare_panel.h/cpp` — 新增对比面板
  - `modules/insights/insights_core/insights_comparator.h/cpp` — 新增 Diff 算法
  - `modules/insights/insights_core/insights_replay.h/cpp` — 新增回放引擎
  - `modules/insights/register_types.cpp` — 注册新类
  - `modules/insights/config.py` — 新增 doc class
  - `modules/insights/SCsub` — 新增 editor/*.cpp 编译
  - `modules/insights/tests/test_insights_phase4.h` — 新增 Phase 4 单元测试

## ADDED Requirements

### Requirement: InsightsEditorPlugin

系统 SHALL 提供 InsightsEditorPlugin 类（继承 EditorPlugin），在编辑器中注册 Insights 主屏幕和底部面板。

#### Scenario: 编辑器插件注册
- **WHEN** InsightsEditorPlugin 进入场景树
- **THEN** 注册主屏幕 "Insights" 和底部面板 "Insights"
- **AND** 提供菜单项 "Project/Tools/Insights/Start Capture"、"Open Trace..."、"Compare Traces..."

#### Scenario: 录制控制
- **WHEN** 用户点击 "Start Capture"
- **THEN** 调用 InsightsManager::start_capture() 开始录制
- **WHEN** 用户点击 "Stop"
- **THEN** 调用 InsightsManager::stop_capture() 停止录制并显示数据

### Requirement: InsightsDock

系统 SHALL 提供 InsightsDock 控件（继承 VBoxContainer），作为底部面板主容器。

#### Scenario: 工具栏
- **WHEN** InsightsDock 初始化
- **THEN** 显示工具栏按钮：Start、Stop、Open、Compare、Clear、Settings

#### Scenario: Channel 标签页
- **WHEN** InsightsDock 初始化
- **THEN** 显示 Channel 标签页：CPU、GPU、Memory、Loading、Network、Log、Compare

#### Scenario: 回放控制
- **WHEN** 用户加载 .gitracy 文件
- **THEN** 显示回放控制栏：Play、Pause、Speed、Time 显示

### Requirement: InsightsTimeline

系统 SHALL 提供 InsightsTimeline 控件（继承 Control），自绘时间轴渲染 CPU/GPU/Loading/Network Track。

#### Scenario: 时间轴渲染
- **WHEN** InsightsTimeline 设置了 InsightsDatabase
- **THEN** 根据 frame_markers 和 zone 数据自绘时间轴
- **AND** 按 Channel 分类渲染 CPU Track、GPU Track、Loading Track、Network Track

#### Scenario: 缩放与平移
- **WHEN** 用户滚动鼠标滚轮
- **THEN** 调整 current_scale 缩放时间轴
- **WHEN** 用户拖拽
- **THEN** 调整 scroll_x 平移时间轴

#### Scenario: Zone 点击选中
- **WHEN** 用户点击时间轴上的 Zone
- **THEN** 设置 selected_zone 并广播选中信号

### Requirement: InsightsFlamegraph

系统 SHALL 提供 InsightsFlamegraph 控件（继承 Control），自绘火焰图渲染 Zone 层级。

#### Scenario: 层级构建
- **WHEN** InsightsFlamegraph 设置了 InsightsDatabase
- **THEN** 从 zone 数据构建嵌套层级树
- **AND** 根节点为最外层 Zone，子节点为嵌套 Zone

#### Scenario: 搜索过滤
- **WHEN** 用户输入搜索关键词
- **THEN** 高亮匹配的 Zone，灰化不匹配的 Zone

#### Scenario: 颜色分配
- **WHEN** 火焰图渲染 Zone
- **THEN** 按 Channel Category 分配颜色

### Requirement: InsightsMemoryPanel

系统 SHALL 提供 InsightsMemoryPanel 控件（继承 Control），显示内存分配瀑布图。

#### Scenario: 分配生命周期
- **WHEN** InsightsMemoryPanel 设置了 InsightsDatabase
- **THEN** 显示所有分配记录的时间范围（alloc → free）
- **AND** 未释放的分配标记为泄漏

#### Scenario: 峰值内存
- **WHEN** 面板加载数据
- **THEN** 计算并显示峰值内存（所有活跃分配之和的最大值）

#### Scenario: 泄漏检测
- **WHEN** 存在未释放的分配
- **THEN** 在泄漏列表中显示 ptr、size、alloc_site

#### Scenario: 大小过滤
- **WHEN** 用户设置大小过滤阈值
- **THEN** 仅显示 >= 阈值的分配记录

### Requirement: InsightsLoadingPanel

系统 SHALL 提供 InsightsLoadingPanel 控件（继承 Control），显示资源加载依赖图。

#### Scenario: 依赖树构建
- **WHEN** InsightsLoadingPanel 设置了 InsightsDatabase
- **THEN** 从 LoadEvent 的 parent_stack 构建依赖树
- **AND** 根节点为顶层资源加载，子节点为依赖资源

#### Scenario: 瓶颈检测
- **WHEN** 面板加载数据
- **THEN** 检测并高亮最慢的资源加载（按耗时或内存大小）

### Requirement: InsightsNetworkPanel

系统 SHALL 提供 InsightsNetworkPanel 控件（继承 Control），显示网络 RPC/包流量。

#### Scenario: 网络流量渲染
- **WHEN** InsightsNetworkPanel 设置了 InsightsDatabase
- **THEN** 显示 RPC 调用和包传输事件的时间线

### Requirement: InsightsComparePanel

系统 SHALL 提供 InsightsComparePanel 控件（继承 Control），支持双 Trace 对比。

#### Scenario: Diff 计算
- **WHEN** 用户选择 baseline 和 current 两个 .gitracy 文件
- **THEN** 按 zone name 匹配两侧数据
- **AND** 计算每个 zone 的 avg/max/p50 差异

#### Scenario: 回归检测
- **WHEN** zone 的变化率超过阈值
- **THEN** 标记为 regression（红色高亮）

#### Scenario: 改进检测
- **WHEN** zone 的变化率为负（性能提升）
- **THEN** 标记为 improvement（绿色高亮）

#### Scenario: 新增/移除 Zone
- **WHEN** zone 仅存在于 current
- **THEN** 标记为 new zone
- **WHEN** zone 仅存在于 baseline
- **THEN** 标记为 removed zone

### Requirement: InsightsComparator

系统 SHALL 提供 InsightsComparator 类，实现双 Trace Diff 算法。

#### Scenario: Diff 输出
- **WHEN** 调用 compute_diff(db_a, db_b)
- **THEN** 返回 InsightsDiff 结构体，包含 regressions、improvements、new_zones、removed_zones 列表

### Requirement: InsightsReplay

系统 SHALL 提供 InsightsReplay 类，实现 Trace 回放引擎。

#### Scenario: 回放控制
- **WHEN** 调用 play()
- **THEN** 按当前速度推进 current_time_ns
- **WHEN** 调用 seek(time_ns)
- **THEN** 跳转到指定时间位置
- **WHEN** 调用 set_speed(scalar)
- **THEN** 调整回放速度倍率

### Requirement: Phase 4 单元测试

系统 SHALL 提供 6 个单元测试（`modules/insights/tests/test_insights_phase4.h`），覆盖编辑器 UI 的核心功能，使用 doctest 框架的 `TEST_CASE` 宏。

#### Scenario: EditorPlugin 注册与生命周期测试
- **WHEN** InsightsEditorPlugin 被创建并添加到编辑器
- **THEN** 验证 plugin_name 为 "Insights"
- **AND** 验证 _has_main_screen() 返回 true
- **AND** 验证 BottomPanel 已注册且 dock 在场景树中

#### Scenario: Timeline 渲染测试
- **WHEN** InsightsTimeline 设置了包含 zone 数据的 InsightsDatabase
- **THEN** 验证 total_duration_ns 正确
- **AND** 验证 visible_range 有效
- **AND** 验证点击 Zone 可选中

#### Scenario: Flamegraph 层级构建测试
- **WHEN** InsightsFlamegraph 设置了包含嵌套 zone 的 InsightsDatabase
- **THEN** 验证根节点数量和名称
- **AND** 验证子节点数量和名称
- **AND** 验证搜索过滤功能

#### Scenario: Memory Panel 分配生命周期测试
- **WHEN** InsightsMemoryPanel 设置了包含分配/释放数据的 InsightsDatabase
- **THEN** 验证峰值内存计算正确
- **AND** 验证泄漏列表正确
- **AND** 验证大小过滤功能

#### Scenario: Loading Panel 依赖树测试
- **WHEN** InsightsLoadingPanel 设置了包含资源加载依赖的 InsightsDatabase
- **THEN** 验证根节点和依赖子节点正确
- **AND** 验证瓶颈检测正确

#### Scenario: Compare Panel Diff 测试
- **WHEN** InsightsComparePanel 设置了 baseline 和 current 两个 InsightsDatabase
- **THEN** 验证回归检测正确（regressions 列表）
- **AND** 验证新增 zone 检测正确（new_zones 列表）

## MODIFIED Requirements

### Requirement: InsightsManager UI 集成

Phase 1-3 已实现 InsightsManager 的录制/停止/Channel 注册。Phase 4 中 InsightsManager SHALL 提供 UI 所需的便捷方法：get_current_database()、is_recording() 等。

### Requirement: InsightsDatabase 查询扩展

Phase 1-3 已实现 zone/allocation/gpu_zone 的 insert/query。Phase 4 中 InsightsDatabase SHALL 新增 UI 所需的查询方法：query_zones_by_depth()、query_allocations_by_size()、query_resource_loads() 等。

## REMOVED Requirements

无。
