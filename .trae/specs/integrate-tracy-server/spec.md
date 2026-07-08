# Tracy Server 编译集成 Spec (Phase 7)

## Why
当前 Godot Insights 使用自建 `NativeCapture` 收集性能数据，数据经过 `.gitracy` 中间格式转换后才能导出，且存在数据丢失（GPU Context、Memory、Callstack 等不完整）。直接将 Tracy 的 `Worker` 编译进 Godot 模块，可以零损失地接收 Tracy Client 的全部数据，并原生生成 `.tracy` 文件。

## What Changes
- 从 Tracy 源码 (`E:\Code\03_Tool\Tracy`) 抽取 server/ 和 common/ 组件到 `modules/insights/tracy_server/` 和 `modules/insights/tracy_common/`
- 新增 `InsightsTracyBridge` 类，封装 `tracy::Worker` 的生命周期管理和数据查询 API
- 修改 SCsub 构建脚本，编译 Tracy Server 源码
- 修改 `InsightsDock` 录制逻辑，使用 `InsightsTracyBridge` 替代 `NativeCapture` 进行实时录制
- 保留 `NativeCapture` 作为回退方案（非 Tracy 构建时使用）

## Impact
- Affected code: `modules/insights/SCsub`, `modules/insights/config.py`, `modules/insights/register_types.cpp`, `modules/insights/editor/insights_dock.h/.cpp`
- 新增文件: `modules/insights/insights_tracy_bridge.h/.cpp`, `modules/insights/tracy_server/*`, `modules/insights/tracy_common/*`
- 编译依赖: 需要 `profiler=tracy` 编译选项（Worker 需要 `TracySocket.cpp` 连接到 Client）
- 二进制增量: 约 3-4 MB（TracyWorker + LZ4 + Zstd）

## ADDED Requirements

### Requirement: Tracy Server Source Integration
系统 SHALL 将 Tracy 的 server/ 和 common/ 组件编译为 Godot Insights 模块的一部分。

#### Scenario: 编译成功
- **WHEN** 使用 `profiler=tracy` 编译选项构建 Godot
- **THEN** Tracy Worker、FileWrite、FileRead、Socket 等组件编译成功，无链接错误

#### Scenario: 非 Tracy 构建
- **WHEN** 未使用 `profiler=tracy` 编译选项
- **THEN** 模块仍然编译成功，InsightsTracyBridge 不可用，回退到 NativeCapture

### Requirement: InsightsTracyBridge 连接管理
系统 SHALL 提供 `InsightsTracyBridge` 类，管理 Tracy Worker 的连接、断开和数据查询。

#### Scenario: 实时连接
- **WHEN** 用户点击 Start Recording
- **THEN** InsightsTracyBridge 创建 Worker 连接到 `127.0.0.1:8086`，开始接收 Tracy Client 数据

#### Scenario: 断开连接
- **WHEN** 用户点击 Stop Recording
- **THEN** InsightsTracyBridge 断开 Worker 连接，保留数据模型供浏览

### Requirement: 保存 .tracy 文件
系统 SHALL 使用 Worker.Write() 将录制数据保存为标准 .tracy 文件。

#### Scenario: 保存成功
- **WHEN** 录制停止后保存文件
- **THEN** 生成标准 .tracy 文件，外部 Tracy 可以直接 File→Open 打开

#### Scenario: 加载 .tracy 文件
- **WHEN** 用户打开 .tracy 文件
- **THEN** 通过 Worker(FileRead) 加载数据，UI 显示完整内容

### Requirement: 数据查询 API
系统 SHALL 通过 InsightsTracyBridge 暴露 Worker 的只读查询 API，供 Godot UI 渲染。

#### Scenario: 获取线程列表
- **WHEN** UI 需要渲染 Timeline
- **THEN** 可查询所有线程及其 Zone 数据

#### Scenario: 获取帧数据
- **WHEN** UI 需要显示帧标记
- **THEN** 可查询所有帧集和帧时间戳

### Requirement: 编译条件隔离
系统 SHALL 通过条件编译隔离 Tracy Server 代码，确保非 Tracy 构建不受影响。

#### Scenario: TRACY_SERVER_ENABLED 宏
- **WHEN** 编译时定义了 TRACY_SERVER_ENABLED
- **THEN** InsightsTracyBridge 和相关功能可用

#### Scenario: 无 TRACY_SERVER_ENABLED
- **WHEN** 编译时未定义 TRACY_SERVER_ENABLED
- **THEN** InsightsDock 使用 NativeCapture 作为回退，模块正常编译
