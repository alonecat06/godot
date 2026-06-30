# Phase 5：脚本/C# 与录制工作流 Spec

## Why

Phase 1-4 已完成 Insights 模块基础设施、录制核心、GPU Profiling 和编辑器 UI，但脚本语言（C# Mono）的性能分析尚未接入，且缺乏录制工作流工具（Tracy 联动、格式转换、CLI、CI 集成）。Phase 5 需要实现 C# Mono 插桩桥接、GDScript `@profiler_zone` 装饰器、启动带 Tracy 工具、`.gitracy` ↔ `.tracy` 转换器、命令行 CLI 和 CI 集成，使开发者能从脚本层到 CI 流水线全链路使用 Insights。

## What Changes

- **MonoProfilerBridge**：C# Mono 运行时插桩桥接，将 C# 方法调用/异步/协程事件转发到 ScriptChannel
- **GDScript @profiler_zone 装饰器**：在 GDScript 解析器中支持 `@profiler_zone` 和 `@profiler_zone("custom_name")` 装饰器
- **InsightsLauncher**：启动带 Tracy 工具的进程管理器，支持指定端口连接远程 Tracy
- **TracyConverter**：`.gitracy` ↔ `.tracy` 双向格式转换器
- **InsightsCLI**：命令行工具，支持 `godot --insights-export`、`insights-cli compare` 等子命令
- **CI 集成**：`insights-cli compare baseline.gitracy pr.gitracy --threshold 0.1`，回归超阈值返回非零退出码
- **单元测试**：6 个测试用例覆盖 C# 插桩、异步/协程、装饰器、转换器、启动工具、CLI

## Impact

- Affected specs: Phase 2（ScriptChannel 需扩展 C# 语言支持和异步/协程生命周期）
- Affected code:
  - `modules/insights/scripting/mono_profiler_bridge.h/cpp` — 新增 C# Mono 桥接
  - `modules/insights/scripting/gdscript_profiler_decorator.h/cpp` — 新增 GDScript 装饰器支持
  - `modules/insights/tools/insights_launcher.h/cpp` — 新增启动工具
  - `modules/insights/tools/tracy_converter.h/cpp` — 新增格式转换器
  - `modules/insights/tools/insights_cli.h/cpp` — 新增命令行工具
  - `modules/insights/channels/script_channel.h/cpp` — 扩展 C# 语言枚举和异步/协程字段
  - `modules/insights/insights_core/insights_manager.h/cpp` — 新增远程连接支持
  - `modules/insights/register_types.cpp` — 注册新类
  - `modules/insights/config.py` — 新增 doc class
  - `modules/insights/SCsub` — 新增 scripting/*.cpp 和 tools/*.cpp 编译
  - `modules/insights/tests/test_insights_phase5.h` — 新增 Phase 5 单元测试
  - `modules/gdscript/gdscript_parser.cpp` — 修改解析器支持 @profiler_zone 装饰器

## ADDED Requirements

### Requirement: MonoProfilerBridge

系统 SHALL 提供 MonoProfilerBridge 类，将 C# Mono 运行时的方法调用事件桥接到 Insights ScriptChannel。

#### Scenario: C# 方法调用记录
- **WHEN** C# 方法通过 Mono 运行时进入
- **THEN** MonoProfilerBridge 调用 ScriptChannel::enter_function()，language=C_SHARP
- **WHEN** C# 方法离开
- **THEN** MonoProfilerBridge 调用 ScriptChannel::leave_function()

#### Scenario: C# 异步/协程生命周期
- **WHEN** C# async 方法被 await 挂起
- **THEN** MonoProfilerBridge 调用 ScriptChannel::suspend_function()，记录挂起时间
- **WHEN** C# async 方法恢复执行
- **THEN** MonoProfilerBridge 调用 ScriptChannel::resume_function()，记录恢复时间

### Requirement: GDScript @profiler_zone 装饰器

系统 SHALL 在 GDScript 解析器中支持 `@profiler_zone` 装饰器，自动为标记的函数生成 Zone 插桩代码。

#### Scenario: 无参数装饰器
- **WHEN** GDScript 函数标注 `@profiler_zone`
- **THEN** 函数执行时自动创建以函数名命名的 Zone

#### Scenario: 自定义名称装饰器
- **WHEN** GDScript 函数标注 `@profiler_zone("custom_name")`
- **THEN** 函数执行时自动创建以 "custom_name" 命名的 Zone

### Requirement: ScriptChannel C# 语言扩展

系统 SHALL 在 ScriptChannel 中扩展对 C# 语言的支持。

#### Scenario: 语言枚举扩展
- **WHEN** ScriptChannel 记录 C# 方法调用
- **THEN** CallRecord 的 language 字段为 Language::C_SHARP

#### Scenario: 异步/协程字段
- **WHEN** ScriptChannel 记录被挂起的函数调用
- **THEN** CallRecord 包含 was_suspended、suspend_ns、resume_ns 字段

### Requirement: InsightsLauncher

系统 SHALL 提供 InsightsLauncher 类，管理带 Tracy 工具的进程启动。

#### Scenario: 启动带 Insights 的进程
- **WHEN** 调用 launch_with_insights(project_path, port)
- **THEN** 启动 Godot 进程并启用 Tracy 连接
- **AND** 返回 OK 表示启动成功

#### Scenario: 进程状态查询
- **WHEN** 调用 is_running()
- **THEN** 返回进程是否仍在运行

#### Scenario: 停止进程
- **WHEN** 调用 stop()
- **THEN** 终止进程并清理资源

### Requirement: TracyConverter

系统 SHALL 提供 TracyConverter 类，实现 `.gitracy` ↔ `.tracy` 双向格式转换。

#### Scenario: gitracy 转 tracy
- **WHEN** 调用 gitracy_to_tracy(gitracy_path, tracy_path)
- **THEN** 读取 .gitracy 文件，转换为 Tracy 二进制格式写入 .tracy 文件

#### Scenario: tracy 转 gitracy
- **WHEN** 调用 tracy_to_gitracy(tracy_path, gitracy_path)
- **THEN** 读取 .tracy 文件，转换为 .gitracy 格式

#### Scenario: Roundtrip 一致性
- **WHEN** .gitracy → .tracy → .gitracy 转换完成
- **THEN** roundtrip 后的数据与原始数据一致

### Requirement: InsightsCLI

系统 SHALL 提供 InsightsCLI 命令行工具，支持 `godot --insights-export` 和 `insights-cli compare` 子命令。

#### Scenario: 导出命令
- **WHEN** 执行 `godot --insights-export <path.gitracy> [--format tracy|html]`
- **THEN** 将 .gitracy 文件导出为指定格式

#### Scenario: 比较命令
- **WHEN** 执行 `insights-cli compare baseline.gitracy pr.gitracy --threshold 0.1`
- **THEN** 比较两个 trace 文件，检测回归
- **AND** 如果回归超过阈值，返回非零退出码

#### Scenario: CI 报告输出
- **WHEN** 比较检测到回归
- **THEN** 输出包含 "REGRESSION" 和回归 zone 名称的报告

### Requirement: Phase 5 单元测试

系统 SHALL 提供 6 个单元测试（`modules/insights/tests/test_insights_phase5.h`），覆盖脚本/C# 与录制工作流的核心功能。

#### Scenario: C# Mono 方法调用插桩测试
- **WHEN** MonoProfilerBridge 记录 C# 方法调用
- **THEN** 验证 ScriptChannel 的 CallRecord 数量正确
- **AND** 验证 language 字段为 C_SHARP

#### Scenario: C# 异步/协程 zone 生命周期测试
- **WHEN** MonoProfilerBridge 记录 async 方法的挂起/恢复
- **THEN** 验证 was_suspended 为 true
- **AND** 验证 resume_ns > suspend_ns

#### Scenario: GDScript @profiler_zone 装饰器测试
- **WHEN** GDScript 函数标注 @profiler_zone 装饰器
- **THEN** 验证函数执行时产生 Zone 记录
- **AND** 验证自定义名称装饰器使用指定名称

#### Scenario: .gitracy ↔ .tracy 转换器测试
- **WHEN** 执行 gitracy_to_tracy 和 tracy_to_gitracy
- **THEN** 验证转换成功且 roundtrip 数据一致

#### Scenario: 启动带 Tracy 工具测试
- **WHEN** InsightsLauncher 启动进程
- **THEN** 验证 is_running() 返回 true
- **AND** 验证 stop() 后 is_running() 返回 false

#### Scenario: CI 命令行工具测试
- **WHEN** InsightsCLI 比较两个 trace 文件
- **THEN** 验证回归检测返回正确的退出码
- **AND** 验证报告包含回归 zone 信息

## MODIFIED Requirements

### Requirement: ScriptChannel 扩展

Phase 2 已实现 ScriptChannel 的 GDScript 函数追踪。Phase 5 中 ScriptChannel SHALL 扩展 Language 枚举新增 C_SHARP，CallRecord 新增 was_suspended/suspend_ns/resume_ns 字段，新增 suspend_function/resume_function 方法。

### Requirement: InsightsManager 远程连接

Phase 1-4 已实现 InsightsManager 的本地录制。Phase 5 中 InsightsManager SHALL 新增远程 Tracy 连接支持：is_connected_to_remote()、connect_to_remote(host, port)。

## REMOVED Requirements

无。
