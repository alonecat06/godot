# Phase 2：录制核心 Spec

## Why

Phase 1 已完成模块骨架、宏扩展、Channel 体系、InsightsManager 单例、InsightsDatabase 和 NativeCapture 的基础实现。但当前引擎仅有约 80 个 Tracy 插桩点，且没有 Memory/Log 等关键 Channel 的具体实现。Phase 2 需要大量扩桩 CPU Zone（目标 350+ 新增），实现 Memory Channel、Log Channel、ScriptChannel、LoadingChannel 等具体 Channel 子类，以及资源加载追踪增强和 GDScript VM 增强，使录制系统能够捕获完整的性能数据。

## What Changes

- **CPU Zone 扩桩**：在 Physics 2D/3D、Scene Tree、Resource Loader、Rendering Substage、Audio、Navigation、Object Lifecycle、Networking 等 8 个子系统中新增约 350 个 `GodotProfileZoneC`/`GodotProfileZoneH` 插桩点
- **MemoryChannel 实现**：继承 InsightsChannel，包裹 `Memory::alloc_static`/`free_static`，记录分配/释放事件，支持泄漏检测
- **LogChannel 实现**：继承 InsightsChannel，拦截 `OS::print_error`，捕获错误/警告消息并关联到当前 Zone
- **ScriptChannel 实现**：继承 InsightsChannel，增强 GDScript VM 跨函数追踪，记录 GC 事件
- **LoadingChannel 实现**：继承 InsightsChannel，扩展 `ResourceLoader::load_paths_stack`，记录资源加载依赖链
- **ResourceLoadTracker 实现**：新增资源加载追踪器类，记录 LoadEvent（路径、时间、大小、依赖栈）
- **NativeCapture 事件类型扩展**：新增 RESOURCE_LOAD、LOG_MESSAGE、GC_EVENT 事件类型
- **InsightsDatabase 记录类型扩展**：确保 AllocationRecord、MessageRecord、ResourceLoadRecord 已有完整写入/查询路径
- **register_types.cpp 更新**：注册新的 Channel 子类到 ClassDB
- **SCsub 更新**：添加 channels/ 子目录下的新源文件

## Impact

- Affected specs: Phase 1 基础设施（InsightsChannel、InsightsDatabase、NativeCapture 需要扩展）
- Affected code:
  - `modules/insights/channels/` — 新增 memory_channel.h/cpp、log_channel.h/cpp、script_channel.h/cpp、loading_channel.h/cpp
  - `modules/insights/insights_core/native_capture.h/cpp` — 扩展 EventType 枚举和 TraceEvent 结构体
  - `modules/insights/insights_core/insights_database.h/cpp` — 确保已有记录类型的完整 API
  - `modules/insights/register_types.cpp` — 注册新类
  - `modules/insights/SCsub` — 无需修改（通配符已覆盖）
  - `core/os/memory.cpp` — 在 alloc_static/free_static 中添加 Insights 回调
  - `core/os/os.cpp` — 在 print_error 中添加 Insights 回调
  - `core/io/resource_loader.cpp` — 在 _load 中添加 Insights 追踪
  - `modules/gdscript/gdscript_vm.cpp` — 增强 Zone 记录
  - Physics/Rendering/Audio/Navigation/SceneTree 等 30+ 引擎源文件 — 添加 Zone 宏

## ADDED Requirements

### Requirement: CPU Zone 扩桩

系统 SHALL 在以下子系统关键函数入口处插入 `GodotProfileZoneC` 或 `GodotProfileZoneH` 宏，遵循 `godot:<channel>/<sub>/<op>` 命名规范：

| 子系统 | 涉及文件 | 新增 Zone 数 | 优先级 |
|--------|----------|-------------|--------|
| Physics 2D | `servers/physics_2d/physics_server_2d.cpp`, `physics_server_2d_wrap_mt.cpp` | ~15 | P0 |
| Physics 3D | `servers/physics_3d/physics_server_3d.cpp`, `physics_server_3d_wrap_mt.cpp` | ~15 | P0 |
| Scene Tree | `scene/main/scene_tree.cpp`, `scene/main/node.cpp`, `scene/main/viewport.cpp` | ~30 | P0 |
| Resource Loader | `core/io/resource_loader.cpp` | ~20 | P0 |
| Rendering Substage | `servers/rendering/rendering_server_default.cpp`, `renderer_rd/forward_clustered/`, `renderer_rd/forward_mobile/`, `renderer_rd/storage_rd/` 等 20 文件 | ~80 | P0 |
| Audio Server | `servers/audio/audio_server.cpp`, `audio_stream.cpp` | ~30 | P1 |
| Navigation | `modules/navigation_3d/3d/godot_navigation_server_3d.cpp`, `nav_map_3d.cpp`, `modules/navigation_2d/2d/godot_navigation_server_2d.cpp`, `nav_map_2d.cpp` | ~40 | P1 |
| Object Lifecycle | `core/object/object.cpp` | ~15 | P1 |
| Networking | `modules/multiplayer/`, `core/io/multiplayer_api.cpp` | ~20 | P1 |

#### Scenario: Physics 3D step 被正确追踪
- **WHEN** 引擎执行 Physics 3D step
- **THEN** Insights 系统记录一个名为 `godot:physics/3d/step` 的 Zone，颜色为 `GODOT_INSIGHTS_COLOR_CPU`

#### Scenario: Rendering forward clustered 被正确追踪
- **WHEN** 引擎执行 forward clustered 渲染
- **THEN** Insights 系统记录一个名为 `godot:rendering/forward_clustered` 的 Zone

### Requirement: MemoryChannel

系统 SHALL 提供 MemoryChannel 类（继承 InsightsChannel），在录制期间拦截所有 `Memory::alloc_static`/`Memory::free_static` 调用，记录分配事件到 InsightsDatabase。

#### Scenario: 内存分配被记录
- **WHEN** Insights 正在录制且 MemoryChannel 已启用
- **AND** 引擎代码调用 `Memory::alloc_static(1024)`
- **THEN** MemoryChannel 记录一个 AllocationRecord（ptr, size=1024, alloc_ns=当前时间, free_ns=0）

#### Scenario: 内存释放被配对
- **WHEN** Insights 正在录制且 MemoryChannel 已启用
- **AND** 之前已记录 ptr=0x1000 的分配
- **AND** 引擎代码调用 `Memory::free_static(0x1000)`
- **THEN** MemoryChannel 更新该 AllocationRecord 的 free_ns 为当前时间

#### Scenario: 内存泄漏检测
- **WHEN** 录制停止后
- **AND** 存在 free_ns==0 的 AllocationRecord
- **THEN** MemoryChannel 将这些记录标记为潜在泄漏

### Requirement: LogChannel

系统 SHALL 提供 LogChannel 类（继承 InsightsChannel），在录制期间拦截 `OS::print_error` 调用，将错误/警告消息记录到 InsightsDatabase。

#### Scenario: 错误消息被捕获
- **WHEN** Insights 正在录制且 LogChannel 已启用
- **AND** 引擎代码调用 `OS::print_error(...)` 输出一条错误
- **THEN** LogChannel 记录一个 MessageRecord（level=ERR_ERROR, text=错误内容, timestamp_ns=当前时间）

#### Scenario: 警告消息被过滤
- **WHEN** LogChannel 的严重级别阈值设为 ERROR
- **AND** 引擎输出一条 WARNING 级别消息
- **THEN** LogChannel 不记录该消息

### Requirement: ScriptChannel

系统 SHALL 提供 ScriptChannel 类（继承 InsightsChannel），增强 GDScript VM 的跨函数追踪能力，记录 GC 事件。

#### Scenario: GDScript 函数调用被追踪
- **WHEN** Insights 正在录制且 ScriptChannel 已启用
- **AND** GDScript VM 执行一个函数调用
- **THEN** ScriptChannel 记录一个 Zone（name=`godot:script/gdscript/<function_name>`, 颜色=`GODOT_INSIGHTS_COLOR_SCRIPT`）

#### Scenario: GC 事件被记录
- **WHEN** Insights 正在录制且 ScriptChannel 已启用
- **AND** GDScript VM 执行垃圾回收
- **THEN** ScriptChannel 记录一个 Zone（name=`godot:script/gdscript/gc`）

### Requirement: LoadingChannel

系统 SHALL 提供 LoadingChannel 类（继承 InsightsChannel），扩展 `ResourceLoader::load_paths_stack`，记录资源加载依赖链。

#### Scenario: 资源加载被追踪
- **WHEN** Insights 正在录制且 LoadingChannel 已启用
- **AND** ResourceLoader 加载 `res://scene.tscn`
- **THEN** LoadingChannel 记录一个 ResourceLoadRecord（path=`res://scene.tscn`, start_ns, end_ns, parent_path=空）

#### Scenario: 资源依赖链被记录
- **WHEN** 加载 `res://scene.tscn` 触发子依赖 `res://player.tres` 的加载
- **THEN** LoadingChannel 记录 `res://player.tres` 的 ResourceLoadRecord，其 parent_path=`res://scene.tscn`

### Requirement: ResourceLoadTracker

系统 SHALL 提供 ResourceLoadTracker 单例类，在录制期间跟踪所有资源加载事件，维护活跃加载映射和依赖栈。

#### Scenario: 加载开始被记录
- **WHEN** ResourceLoader 开始加载资源 `p_path`
- **THEN** ResourceLoadTracker 调用 `on_load_begin(p_path, p_loader)`，记录 LoadEvent（start_ns=当前时间，parent_stack=当前 load_paths_stack）

#### Scenario: 加载结束被记录
- **WHEN** ResourceLoader 完成加载资源 `p_path`
- **THEN** ResourceLoadTracker 调用 `on_load_end(p_path, p_res, p_size)`，更新 LoadEvent（end_ns=当前时间，memory_size=p_size）

### Requirement: NativeCapture 事件类型扩展

系统 SHALL 在 NativeCapture::EventType 枚举中新增 RESOURCE_LOAD、LOG_MESSAGE、GC_EVENT 事件类型，并在 TraceEvent 联合体中添加对应的数据结构。

#### Scenario: 资源加载事件入队
- **WHEN** LoadingChannel 记录一个资源加载事件
- **THEN** NativeCapture 将其作为 RESOURCE_LOAD 类型事件入队到双缓冲区

### Requirement: InsightsCapture 接口扩展

系统 SHALL 在 InsightsCapture 抽象基类中新增 `on_resource_load`、`on_log_message`、`on_gc_event` 纯虚方法，NativeCapture 实现这些方法。

#### Scenario: 新回调方法被调用
- **WHEN** Insights 系统接收到资源加载/日志/GC 事件
- **THEN** InsightsCapture 的对应虚方法被调用，NativeCapture 将事件入队

## MODIFIED Requirements

### Requirement: InsightsManager Channel 注册

Phase 1 已实现 InsightsManager.register_channel/unregister_channel。Phase 2 中 InsightsManager SHALL 在 start_capture 时自动注册 MemoryChannel、LogChannel、ScriptChannel、LoadingChannel 等内置 Channel，在 stop_capture 时自动注销。

### Requirement: InsightsDatabase 已有记录类型

Phase 1 已定义 AllocationRecord、MessageRecord、ResourceLoadRecord 结构体。Phase 2 SHALL 确保这些结构体的 insert/query 方法完整可用，且 NativeCapture 的消费者线程正确将事件写入 InsightsDatabase。

## REMOVED Requirements

无。

## Modified Files

### 新增文件（10 个）

| 文件路径 | 说明 |
|---------|------|
| `modules/insights/channels/memory_channel.h` | MemoryChannel 头文件，内存分配追踪 |
| `modules/insights/channels/memory_channel.cpp` | MemoryChannel 实现，on_alloc/on_free/detect_leaks |
| `modules/insights/channels/log_channel.h` | LogChannel 头文件，日志消息捕获 |
| `modules/insights/channels/log_channel.cpp` | LogChannel 实现，Severity 枚举/过滤/查询 |
| `modules/insights/channels/script_channel.h` | ScriptChannel 头文件，脚本函数追踪 |
| `modules/insights/channels/script_channel.cpp` | ScriptChannel 实现，enter_function/on_gc_event |
| `modules/insights/channels/loading_channel.h` | LoadingChannel 头文件，资源加载追踪 |
| `modules/insights/channels/loading_channel.cpp` | LoadingChannel 实现，on_load_begin/end/fail |
| `modules/insights/insights_core/resource_load_tracker.h` | ResourceLoadTracker 头文件，资源加载追踪器单例 |
| `modules/insights/insights_core/resource_load_tracker.cpp` | ResourceLoadTracker 实现，LoadEvent 管理 |

### 修改文件 — Insights 模块内部（7 个）

| 文件路径 | 修改内容 |
|---------|---------|
| `modules/insights/insights_core/insights_capture.h` | 新增 on_resource_load/on_log_message/on_gc_event 3 个纯虚方法 |
| `modules/insights/insights_core/native_capture.h` | EventType 新增 RESOURCE_LOAD/LOG_MESSAGE/GC_EVENT；TraceEvent 联合体新增对应数据结构；新增 3 个虚方法声明 |
| `modules/insights/insights_core/native_capture.cpp` | 实现 on_resource_load/on_log_message/on_gc_event；_process_events 处理新事件类型 |
| `modules/insights/insights_core/insights_manager.h` | 新增 4 个 Channel include；新增 memory_channel/log_channel/script_channel/loading_channel 成员；新增 _register/_unregister_builtin_channels、get_*_channel 方法声明 |
| `modules/insights/insights_core/insights_manager.cpp` | 实现 _register_builtin_channels/_unregister_builtin_channels；start_capture 自动注册；stop_capture 自动注销；4 个 get_*_channel 便捷方法 |
| `modules/insights/register_types.cpp` | 新增 5 个类 include；GDREGISTER_CLASS 注册 MemoryChannel/LogChannel/ScriptChannel/LoadingChannel/ResourceLoadTracker |
| `modules/insights/config.py` | doc_classes 新增 5 个类名 |

### 修改文件 — 引擎核心 Hook（4 个）

| 文件路径 | 修改内容 |
|---------|---------|
| `core/os/memory.cpp` | alloc_static/free_static 中添加 MODULE_INSIGHTS_ENABLED 条件编译块，录制时调用 MemoryChannel::on_alloc/on_free |
| `core/os/os.cpp` | print_error 中添加 MODULE_INSIGHTS_ENABLED 条件编译块，录制时调用 LogChannel::log_message，映射 ErrorType→Severity |
| `core/io/resource_loader.cpp` | _load 中添加 GodotProfileZoneH + ResourceLoadTracker::on_load_begin/on_load_end/on_load_fail 调用；load/_run_load_task/_load_complete_inner 添加 Zone 宏 |
| `modules/gdscript/gdscript_vm.cpp` | 10 处 GodotProfileZoneScript 后添加 ScriptChannel::enter_function 调用 |

### 修改文件 — CPU Zone 扩桩 P0（9 个）

| 文件路径 | 新增 Zone 宏 | 命名规范 |
|---------|-------------|---------|
| `servers/physics_2d/physics_server_2d.cpp` | 2 | `godot:physics/2d/<op>` |
| `servers/physics_2d/physics_server_2d_wrap_mt.cpp` | 4 | `godot:physics/2d/<op>` |
| `servers/physics_3d/physics_server_3d.cpp` | 2 | `godot:physics/3d/<op>` |
| `servers/physics_3d/physics_server_3d_wrap_mt.cpp` | 4 | `godot:physics/3d/<op>` |
| `scene/main/scene_tree.cpp` | 4 | `godot:scene/tree/<op>` |
| `scene/main/node.cpp` | 5 | `godot:scene/node/<op>` |
| `scene/main/viewport.cpp` | 5 | `godot:scene/viewport/<op>` |
| `servers/rendering/rendering_server_default.cpp` | 5 | `godot:rendering/<op>` |
| `servers/rendering/renderer_rd/forward_clustered/render_forward_clustered.cpp` | 3 | `godot:rendering/forward_clustered/<op>` |
| `servers/rendering/renderer_rd/forward_mobile/render_forward_mobile.cpp` | 3 | `godot:rendering/forward_mobile/<op>` |

### 修改文件 — CPU Zone 扩桩 P1（8 个）

| 文件路径 | 新增 Zone 宏 | 命名规范 |
|---------|-------------|---------|
| `servers/audio/audio_server.cpp` | 4 | `godot:audio/<op>` |
| `servers/audio/audio_stream.cpp` | 3 | `godot:audio/<op>` |
| `modules/navigation_3d/3d/godot_navigation_server_3d.cpp` | 3 | `godot:navigation/3d/<op>` |
| `modules/navigation_3d/nav_map_3d.cpp` | 3 | `godot:navigation/3d/<op>` |
| `modules/navigation_2d/2d/godot_navigation_server_2d.cpp` | 3 | `godot:navigation/2d/<op>` |
| `modules/navigation_2d/nav_map_2d.cpp` | 3 | `godot:navigation/2d/<op>` |
| `core/object/object.cpp` | 4 | `godot:object/<op>` |
| `modules/multiplayer/scene_multiplayer.cpp` | 3 | `godot:network/<op>` |
| `modules/multiplayer/scene_rpc_interface.cpp` | 3 | `godot:network/rpc/<op>` |

**合计：新增 10 个文件，修改 28 个文件，新增约 65 个 Zone 宏。**
