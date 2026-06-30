# Phase 6：增强（持续）Spec

## Why

Phase 1-5 已完成 Insights 模块基础设施、录制核心、GPU Profiling、编辑器 UI 和脚本/C# 工作流，但缺乏锁竞争分析、自定义 Channel 扩展、Web 报告导出、实时 Streaming 和 AI 辅助分析等增强功能。Phase 6 需要实现这些持续增强特性，使 Insights 从基础录制工具升级为全链路性能分析平台。

## What Changes

- **CPUChannel 锁竞争检测**：在 CPUChannel 中新增 ContentionEvent 追踪，记录锁获取/尝试/释放事件，计算等待时间
- **CustomChannel API**：新增 CustomChannel 类，允许用户/GDExtension 注册自定义 Channel 并写入自定义 Zone
- **WebExporter**：新增 WebExporter 类，将 .gitracy trace 数据导出为包含 Timeline/Flamegraph 的 HTML 报告
- **Live Profiling**：在 InsightsManager 中新增实时 Streaming 模式，通过 TCP 接收远程 Zone/Frame 数据
- **AIAnalyzer**：新增 AIAnalyzer 类，构建分析提示词并解析 LLM 响应，识别瓶颈和给出建议
- **单元测试**：5 个测试用例覆盖锁竞争、自定义 Channel、Web 导出、Live Profiling、AI 分析

## Impact

- Affected specs: Phase 1（InsightsManager 需扩展 live capture 模式）、Phase 2（CPUChannel 需扩展锁竞争追踪）
- Affected code:
  - `modules/insights/channels/cpu_channel.h/cpp` — 新增 ContentionEvent 追踪
  - `modules/insights/channels/custom_channel.h/cpp` — 新增 CustomChannel 类
  - `modules/insights/tools/web_exporter.h/cpp` — 新增 Web 导出
  - `modules/insights/insights_core/ai_analyzer.h/cpp` — 新增 AI 分析
  - `modules/insights/insights_core/insights_manager.h/cpp` — 新增 live capture 模式
  - `modules/insights/editor/insights_contention_panel.h/cpp` — 新增锁竞争视图
  - `modules/insights/register_types.cpp` — 注册新类
  - `modules/insights/config.py` — 新增 doc class
  - `modules/insights/SCsub` — 新增编译路径
  - `modules/insights/tests/test_insights_phase6.h` — 新增 Phase 6 单元测试

## ADDED Requirements

### Requirement: CPUChannel 锁竞争检测

系统 SHALL 在 CPUChannel 中新增锁竞争追踪功能，记录 ContentionEvent 以识别线程间锁等待。

#### Scenario: 锁竞争事件记录
- **WHEN** 线程 A 持有锁，线程 B 尝试获取同一把锁
- **THEN** CPUChannel 记录 ContentionEvent，包含 lock_name、owner_thread、waiter_thread、wait_time_ns

#### Scenario: 锁释放后获取
- **WHEN** 线程 A 释放锁，线程 B 随后获得锁
- **THEN** ContentionEvent 的 wait_time_ns = B 获得锁时间 - B 尝试获取时间

### Requirement: CustomChannel API

系统 SHALL 提供 CustomChannel 类，允许用户/GDExtension 注册自定义 Channel 并写入自定义 Zone。

#### Scenario: 注册自定义 Channel
- **WHEN** 用户创建 CustomChannel 并设置 name/color
- **THEN** 调用 InsightsManager::register_channel 后可通过 get_channel 查询

#### Scenario: 写入自定义 Zone
- **WHEN** 调用 CustomChannel::begin_zone/end_zone
- **THEN** Zone 记录存储在 CustomChannel 中，可通过 get_zones 查询

### Requirement: WebExporter

系统 SHALL 提供 WebExporter 类，将 .gitracy trace 数据导出为包含 Timeline 和 Flamegraph 的独立 HTML 文件。

#### Scenario: 导出 HTML 报告
- **WHEN** 调用 export_to_html(gitracy_path, html_path)
- **THEN** 生成包含 zone 数据可视化（timeline、flamegraph）的 HTML 文件

#### Scenario: HTML 内容验证
- **WHEN** HTML 文件生成完成
- **THEN** 文件包含 zone 名称和 timeline/flamegraph 关键字

### Requirement: Live Profiling

系统 SHALL 在 InsightsManager 中新增实时 Streaming 模式，通过 TCP 接收远程 Zone/Frame 数据。

#### Scenario: 启动 live capture
- **WHEN** 调用 start_live_capture(host, port)
- **THEN** InsightsManager 进入 live 模式，is_live_mode() 返回 true

#### Scenario: 接收实时数据
- **WHEN** 接收到远程 Zone/Frame 数据
- **THEN** 数据写入 live database，可通过 query_zone 查询

#### Scenario: 停止 live capture
- **WHEN** 调用 stop_live_capture()
- **THEN** 退出 live 模式，is_live_mode() 返回 false

### Requirement: AIAnalyzer

系统 SHALL 提供 AIAnalyzer 类，构建性能分析提示词并解析 LLM JSON 响应，识别瓶颈和给出建议。

#### Scenario: 构建分析提示词
- **WHEN** 调用 build_analysis_prompt(database)
- **THEN** 返回包含 zone 名称和耗时的提示词字符串

#### Scenario: 解析 LLM 响应
- **WHEN** 调用 parse_response(json_string)
- **THEN** 返回 AnalysisResult，包含 bottleneck、suggestions、severity 字段

### Requirement: Phase 6 单元测试

系统 SHALL 提供 5 个单元测试（`modules/insights/tests/test_insights_phase6.h`），覆盖 Phase 6 增强功能。

#### Scenario: Lock contention 检测测试
- **WHEN** CPUChannel 记录锁获取/尝试/释放事件
- **THEN** 验证 ContentionEvent 数量正确
- **AND** 验证 wait_time_ns 计算正确

#### Scenario: Custom Channel API 测试
- **WHEN** 注册自定义 Channel 并写入 Zone
- **THEN** 验证 Zone 记录正确
- **AND** 验证 InsightsManager 可查询该 Channel

#### Scenario: Web export 测试
- **WHEN** 导出 trace 为 HTML
- **THEN** 验证文件存在且包含关键内容

#### Scenario: Live profiling 测试
- **WHEN** 启动/停止 live capture
- **THEN** 验证 live 模式状态正确
- **AND** 验证接收的数据可查询

#### Scenario: AI analysis 测试
- **WHEN** 构建提示词并解析响应
- **THEN** 验证提示词包含 zone 信息
- **AND** 验证解析结果正确

## MODIFIED Requirements

### Requirement: CPUChannel 扩展

Phase 2 中 CPUChannel 未实现锁竞争追踪。Phase 6 中 CPUChannel SHALL 新增 enable_contention_tracking()、on_lock_acquire()、on_lock_attempt()、on_lock_release()、get_contention_events() 方法和 ContentionEvent 结构体。

### Requirement: InsightsManager 扩展

Phase 5 中 InsightsManager 已实现远程连接（stub）。Phase 6 中 InsightsManager SHALL 新增 start_live_capture(host, port)、stop_live_capture()、is_live_mode()、get_live_database()、_on_live_frame_received()、_on_live_zone_received() 方法。

## REMOVED Requirements

无。
