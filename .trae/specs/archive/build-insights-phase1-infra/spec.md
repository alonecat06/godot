# Godot Insights Phase 1 基础设施 Spec

## Why
Godot 已有 Tracy 客户端集成（`core/profiling/profiling.h`），但缺少结构化的 Channel 体系、统一命名规范和录制管理器。Phase 1 为整个 Godot Insights 工具链奠定基础层：模块骨架、宏扩展、Channel 体系和 InsightsManager 单例，使后续 Phase 能在此基础上扩展插桩、GPU profiling 和编辑器 UI。

## What Changes
- 新增 `modules/insights/` 模块骨架，注册为引擎模块
- 新增 `core/profiling/insights.h` 宏扩展头文件（`GodotProfileZoneC` / `GodotProfileZoneH` / `GodotProfileFiber` / `GodotProfilePlot` / `GodotProfileMessage` / `GodotProfileResourceLoad` / `GodotProfileGpuStage`）
- 新增 `InsightsChannel` 基类 + `ChannelCategory` 枚举 + 命名规范校验
- 新增 `InsightsManager` 单例，提供 start/stop/save/load 基础录制生命周期
- 新增 `InsightsDatabase` 基础框架（SQLite schema 创建 + 基本 zone 插入/查询）
- 新增 `InsightsCapture` 抽象基类 + `NativeCapture` 实现（RingBuffer + 消费者线程）
- 新增 `ChannelCategory` 到 `Color` 的映射表
- **BREAKING**: 无，所有新增宏在 `GODOT_USE_TRACY` 未定义时为 no-op

## Impact
- Affected specs: 无（全新模块）
- Affected code:
  - `modules/insights/` — 全新目录
  - `core/profiling/insights.h` — 新增头文件
  - `core/profiling/SCsub` — 添加 insights.h 到构建
  - `modules/SCsub` — 注册 insights 模块
  - `main/main.cpp` — InsightsManager 初始化/清理调用
  - `core/config/engine.h` — 可选：暴露 `is_insights_active()` 查询

## ADDED Requirements

### Requirement: Insights 模块骨架
系统 SHALL 提供 `modules/insights/` 引擎模块，通过 SCons 构建系统注册，可通过 `module_insights_enabled=yes/no` 控制编译。

#### Scenario: 模块编译启用
- **WHEN** 使用 `scons module_insights_enabled=yes` 编译
- **THEN** `InsightsManager` 类注册到 ClassDB，`modules/insights/` 下所有源文件参与编译

#### Scenario: 模块编译禁用
- **WHEN** 使用 `scons module_insights_enabled=no` 编译（默认）
- **THEN** 不编译任何 insights 代码，`core/profiling/insights.h` 中所有宏为 no-op stub

### Requirement: Insights 宏扩展
系统 SHALL 在 `core/profiling/insights.h` 中提供以下宏，在 `GODOT_USE_TRACY` 定义时映射到 Tracy API，未定义时为 no-op：

| 宏 | 用途 |
|----|------|
| `GodotProfileZoneC(category, name)` | 通道化 Zone |
| `GodotProfileZoneH(subsystem, op1, op2)` | 层级化 Zone（自动拼接 `godot:subsystem/op1/op2`） |
| `GodotProfileFiber(name)` | 异步任务/Fiber 追踪 |
| `GodotProfilePlot(name, value)` | 计数器/Plot |
| `GodotProfileMessage(text)` | 自定义消息 |
| `GodotProfileResourceLoad(path)` | 资源加载追踪 |
| `GodotProfileGpuStage(name)` | GPU 阶段标记 |

#### Scenario: 宏在无 Tracy 时编译通过
- **WHEN** `GODOT_USE_TRACY` 未定义
- **THEN** 所有 `GodotProfileZoneC`/`GodotProfileZoneH`/`GodotProfileFiber`/`GodotProfilePlot`/`GodotProfileMessage`/`GodotProfileResourceLoad`/`GodotProfileGpuStage` 宏展开为空语句，编译通过且零运行时开销

#### Scenario: 宏在 Tracy 启用时正确记录
- **WHEN** `GODOT_USE_TRACY` 已定义
- **THEN** `GodotProfileZoneC("cpu", "godot:physics/3d/step")` 在 Tracy 中创建带 category 颜色的 zone；`GodotProfileZoneH("physics", "3d", "step")` 等价于 `GodotProfileZoneC("cpu", "godot:physics/3d/step")`

### Requirement: Channel 体系与命名规范
系统 SHALL 提供 `InsightsChannel` 基类和 `ChannelCategory` 枚举，支持 Channel 注册、查询和命名规范校验。

#### Scenario: Channel 注册与查询
- **WHEN** 向 `InsightsManager` 注册一个 `CPUChannel`（name="cpu"）
- **THEN** `InsightsManager::get_channel("cpu")` 返回该实例，`get_channel_count()` 递增

#### Scenario: 命名规范校验
- **WHEN** 调用 `InsightsChannel::validate_zone_name("godot:physics/3d/step")`
- **THEN** 返回 `true`
- **WHEN** 调用 `InsightsChannel::validate_zone_name("invalid_name")` 或 `InsightsChannel::validate_zone_name("godot:")`
- **THEN** 返回 `false`

#### Scenario: Category 从 zone 名自动提取
- **WHEN** 调用 `InsightsChannel::get_category("godot:physics/3d/step")`
- **THEN** 返回 `ChannelCategory::CPU`
- **WHEN** 调用 `InsightsChannel::get_category("godot:gpu/command/draw")`
- **THEN** 返回 `ChannelCategory::GPU`

#### Scenario: Channel 颜色映射
- **WHEN** 查询 `ChannelCategory::CPU` 的默认颜色
- **THEN** 返回 `Color(0x7A/0xFF, 0xC0/0xFF, 0xE5/0xFF)` 即 `#7AC0E5`

### Requirement: InsightsManager 单例生命周期
系统 SHALL 提供 `InsightsManager` 单例，管理录制的 start/stop/save/load 生命周期。

#### Scenario: 录制启动
- **WHEN** 调用 `InsightsManager::start_capture("res://test.gitracy")`
- **THEN** `is_recording()` 返回 `true`，`NativeCapture` 启动 RingBuffer 和消费者线程

#### Scenario: 录制停止
- **WHEN** 调用 `InsightsManager::stop_capture()`
- **THEN** `is_recording()` 返回 `false`，返回保存的文件路径字符串

#### Scenario: 未启动时停止
- **WHEN** 在未调用 `start_capture` 的情况下调用 `stop_capture()`
- **THEN** 返回空字符串，不崩溃

#### Scenario: Manager 初始化与清理
- **WHEN** 引擎启动
- **THEN** `InsightsManager::get_singleton()` 返回有效实例
- **WHEN** 引擎关闭
- **THEN** 若正在录制则自动停止并保存，`InsightsManager` 单例被销毁

### Requirement: InsightsDatabase 基础框架
系统 SHALL 提供 `InsightsDatabase` 类，基于 SQLite 实现 `.gitracy` 文件的创建、表结构初始化和基本 zone 查询。

#### Scenario: 数据库创建与表初始化
- **WHEN** 调用 `db.open("res://test.gitracy")` 后调用 `db.create_tables()`
- **THEN** SQLite 文件创建成功，包含 `zones`、`frame_markers`、`allocations`、`gpu_zones`、`resource_loads`、`messages` 表

#### Scenario: Zone 插入与查询
- **WHEN** 插入一条 zone 记录后按 name + 时间范围查询
- **THEN** 返回匹配的记录数组，包含 `name`、`start_ns`、`end_ns`、`depth`、`parent_zone_id` 等字段

#### Scenario: 数据库关闭
- **WHEN** 调用 `db.close()`
- **THEN** SQLite 连接正常关闭，文件可被其他进程读取

### Requirement: InsightsCapture 抽象与 NativeCapture
系统 SHALL 提供 `InsightsCapture` 抽象基类和 `NativeCapture` 实现。`NativeCapture` 使用 RingBuffer + 消费者线程实现低开销的事件采集。

#### Scenario: NativeCapture 启动
- **WHEN** 调用 `NativeCapture::start()`
- **THEN** RingBuffer 分配完成，消费者线程启动，`is_capturing()` 返回 `true`

#### Scenario: 事件入队
- **WHEN** 在录制期间调用 `NativeCapture::on_zone_begin(...)` / `on_zone_end(...)`
- **THEN** 事件写入 RingBuffer，消费者线程异步读取并写入 `InsightsDatabase`

#### Scenario: NativeCapture 停止
- **WHEN** 调用 `NativeCapture::stop()`
- **THEN** 消费者线程完成剩余事件处理后退出，RingBuffer flush，`is_capturing()` 返回 `false`

## MODIFIED Requirements
无

## REMOVED Requirements
无
