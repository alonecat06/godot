# Tasks — Phase 2：录制核心

## 1. Channel 子类实现

- [x] Task 1.1: 实现 MemoryChannel（`modules/insights/channels/memory_channel.h/cpp`）
  - [x] 继承 InsightsChannel，category=CHANNEL_CATEGORY_MEMORY，color=GODOT_INSIGHTS_COLOR_MEMORY
  - [x] 实现 `on_alloc(ptr, size, thread_id, timestamp_ns)` — 记录分配事件
  - [x] 实现 `on_free(ptr, thread_id, timestamp_ns)` — 配对释放事件
  - [x] 实现 `get_total_allocated()` — 返回当前已分配总字节数
  - [x] 实现 `get_allocation_count()` — 返回当前活跃分配数
  - [x] 实现 `detect_leaks()` — 返回 free_ns==0 的分配列表
  - [x] 实现 `on_event()` 和 `serialize()` 虚方法
  - [x] GDREGISTER_CLASS 注册

- [x] Task 1.2: 实现 LogChannel（`modules/insights/channels/log_channel.h/cpp`）
  - [x] 继承 InsightsChannel，category=CHANNEL_CATEGORY_LOG，color=GODOT_INSIGHTS_COLOR_LOG
  - [x] 定义 Severity 枚举（VERBOSE=0, DEBUG=1, INFO=2, WARNING=3, ERROR=4）
  - [x] 实现 `log_message(severity, text, file, line, zone_id)` — 记录日志消息
  - [x] 实现 `set_severity_threshold(severity)` — 设置最低记录级别
  - [x] 实现 `get_messages_in_range(start_ns, end_ns, min_severity)` — 查询消息
  - [x] 实现 `on_event()` 和 `serialize()` 虚方法
  - [x] GDREGISTER_CLASS 注册，VARIANT_ENUM_CAST(LogChannel::Severity)

- [x] Task 1.3: 实现 ScriptChannel（`modules/insights/channels/script_channel.h/cpp`）
  - [x] 继承 InsightsChannel，category=CHANNEL_CATEGORY_SCRIPT，color=GODOT_INSIGHTS_COLOR_SCRIPT
  - [x] 定义 Language 枚举（GDSCRIPT, C_SHARP）
  - [x] 实现 `enter_function(name, file, line, language)` — 记录函数进入
  - [x] 实现 `leave_function()` — 记录函数退出
  - [x] 实现 `on_gc_event(generation, objects_collected)` — 记录 GC 事件
  - [x] 实现 `get_call_records()` — 返回函数调用记录列表
  - [x] 实现 `on_event()` 和 `serialize()` 虚方法
  - [x] GDREGISTER_CLASS 注册

- [x] Task 1.4: 实现 LoadingChannel（`modules/insights/channels/loading_channel.h/cpp`）
  - [x] 继承 InsightsChannel，category=CHANNEL_CATEGORY_LOADING，color=GODOT_INSIGHTS_COLOR_LOADING
  - [x] 实现 `on_load_begin(path, loader)` — 记录加载开始
  - [x] 实现 `on_load_end(path, size_bytes)` — 记录加载完成
  - [x] 实现 `on_load_fail(path, error)` — 记录加载失败
  - [x] 实现 `get_load_events_in_range(start_ns, end_ns)` — 查询加载事件
  - [x] 实现 `on_event()` 和 `serialize()` 虚方法
  - [x] GDREGISTER_CLASS 注册

## 2. InsightsCapture 接口与 NativeCapture 扩展

- [x] Task 2.1: 扩展 InsightsCapture 抽象基类
  - [x] 在 `insights_capture.h` 中新增纯虚方法：`on_resource_load(path, loader, start_ns, end_ns, size_bytes, parent_path, thread_id)`
  - [x] 新增纯虚方法：`on_log_message(level, text, file, line, timestamp_ns, zone_id)`
  - [x] 新增纯虚方法：`on_gc_event(generation, objects_collected, timestamp_ns)`

- [x] Task 2.2: 扩展 NativeCapture 事件类型
  - [x] 在 EventType 枚举中新增 RESOURCE_LOAD、LOG_MESSAGE、GC_EVENT
  - [x] 在 TraceEvent 联合体中添加 resource_load、log_message、gc_event 数据结构
  - [x] 实现 `on_resource_load()`、`on_log_message()`、`on_gc_event()` 虚方法
  - [x] 在 `_process_events()` 中处理新事件类型，写入 InsightsDatabase

## 3. ResourceLoadTracker 实现

- [x] Task 3.1: 实现 ResourceLoadTracker（`modules/insights/insights_core/resource_load_tracker.h/cpp`）
  - [x] 定义 LoadEvent 结构体（path, start_ns, end_ns, memory_size, source_loader, parent_stack, thread_id）
  - [x] 实现 `on_load_begin(path, loader)` — 创建 LoadEvent，记录 start_ns 和当前 load_paths_stack
  - [x] 实现 `on_load_end(path, res, size)` — 更新 LoadEvent 的 end_ns 和 memory_size
  - [x] 实现 `on_load_fail(path, error)` — 标记加载失败
  - [x] 实现 `get_events_in_range(t0, t1)` — 返回时间范围内的加载事件
  - [x] 实现 `get_active_loads()` — 返回当前正在加载的资源
  - [x] 实现 `clear()` — 清空所有记录
  - [x] 单例模式（get_singleton()），GDREGISTER_CLASS 注册

## 4. 引擎核心 Hook 接入

- [x] Task 4.1: Memory Channel Hook — 修改 `core/os/memory.cpp`
  - [x] 在 `Memory::alloc_static()` 中添加 `#ifdef MODULE_INSIGHTS_ENABLED` 条件编译块
  - [x] 录制期间调用 MemoryChannel::on_alloc()（通过 InsightsManager 获取 Channel）
  - [x] 在 `Memory::free_static()` 中添加 `#ifdef MODULE_INSIGHTS_ENABLED` 条件编译块
  - [x] 录制期间调用 MemoryChannel::on_free()

- [x] Task 4.2: Log Channel Hook — 修改 `core/os/os.cpp`
  - [x] 在 `OS::print_error()` 中添加 `#ifdef MODULE_INSIGHTS_ENABLED` 条件编译块
  - [x] 录制期间调用 LogChannel::log_message()（通过 InsightsManager 获取 Channel）
  - [x] 映射 Logger::ErrorType 到 LogChannel::Severity

- [x] Task 4.3: Loading Channel Hook — 修改 `core/io/resource_loader.cpp`
  - [x] 在 `ResourceLoader::_load()` 入口添加 `GodotProfileZoneH("loading", "resource", "load")`
  - [x] 在加载开始处调用 ResourceLoadTracker::on_load_begin()
  - [x] 在加载结束处调用 ResourceLoadTracker::on_load_end()
  - [x] 在加载失败处调用 ResourceLoadTracker::on_load_fail()
  - [x] 所有调用用 `#ifdef MODULE_INSIGHTS_ENABLED` 保护

- [x] Task 4.4: Script Channel Hook — 修改 `modules/gdscript/gdscript_vm.cpp`
  - [x] 在 GDScript 函数调用处（已有 GodotProfileZoneScript 的位置）增强为同时通知 ScriptChannel
  - [x] 在 GC 相关代码处添加 `GodotProfileZoneC(GODOT_INSIGHTS_COLOR_SCRIPT, "godot:script/gdscript/gc")`
  - [x] 所有修改用 `#ifdef MODULE_INSIGHTS_ENABLED` 保护

## 5. CPU Zone 扩桩（P0 优先级）

- [x] Task 5.1: Physics 2D Zone 扩桩
  - [x] `servers/physics_2d/physics_server_2d.cpp` — 在 step、sync、flush_queries、call_script 等关键函数添加 Zone
  - [x] `servers/physics_2d/physics_server_2d_wrap_mt.cpp` — 在多线程包装方法添加 Zone
  - [x] Zone 命名：`godot:physics/2d/<op>`，颜色 `GODOT_INSIGHTS_COLOR_CPU`

- [x] Task 5.2: Physics 3D Zone 扩桩
  - [x] `servers/physics_3d/physics_server_3d.cpp` — 在 step、sync、flush_queries、call_script 等关键函数添加 Zone
  - [x] `servers/physics_3d/physics_server_3d_wrap_mt.cpp` — 在多线程包装方法添加 Zone
  - [x] Zone 命名：`godot:physics/3d/<op>`，颜色 `GODOT_INSIGHTS_COLOR_CPU`

- [x] Task 5.3: Scene Tree Zone 扩桩
  - [x] `scene/main/scene_tree.cpp` — 在 _process、_physics_process、initialize、iteration 等添加 Zone（从 1 个扩展到 ~15 个）
  - [x] `scene/main/node.cpp` — 在 _ready、_enter_tree、_exit_tree、notification 等添加 Zone
  - [x] `scene/main/viewport.cpp` — 在 draw、input、gui_input 等添加 Zone
  - [x] Zone 命名：`godot:scene/<sub>/<op>`

- [x] Task 5.4: Resource Loader Zone 扩桩
  - [x] `core/io/resource_loader.cpp` — 在 load、_load、reload 等添加 Zone
  - [x] Zone 命名：`godot:loading/<sub>/<op>`，颜色 `GODOT_INSIGHTS_COLOR_LOADING`

- [x] Task 5.5: Rendering Substage Zone 扩桩
  - [x] `servers/rendering/rendering_server_default.cpp` — 已有 15 个 Zone，补充缺失的子阶段
  - [x] `servers/rendering/renderer_rd/forward_clustered/render_forward_clustered.cpp` — 在 render_scene、render_camera 等添加 Zone
  - [x] `servers/rendering/renderer_rd/forward_mobile/render_forward_mobile.cpp` — 同上
  - [x] `servers/rendering/renderer_rd/storage_rd/*.cpp` — 在材质编译、纹理上传等添加 Zone
  - [x] `servers/rendering/renderer_viewport.cpp` — 已有 5 个 Zone，补充
  - [x] Zone 命名：`godot:rendering/<sub>/<op>`，颜色 `GODOT_INSIGHTS_COLOR_GPU`

## 6. CPU Zone 扩桩（P1 优先级）

- [x] Task 6.1: Audio Server Zone 扩桩
  - [x] `servers/audio/audio_server.cpp` — 在 mix、update、bus_process 等添加 Zone
  - [x] `servers/audio/audio_stream.cpp` — 在 playback 等添加 Zone
  - [x] Zone 命名：`godot:audio/<op>`，颜色 `GODOT_INSIGHTS_COLOR_CPU`

- [x] Task 6.2: Navigation Zone 扩桩
  - [x] `modules/navigation_3d/3d/godot_navigation_server_3d.cpp` — 在 step、sync 等添加 Zone
  - [x] `modules/navigation_3d/nav_map_3d.cpp` — 在 iteration 等添加 Zone
  - [x] `modules/navigation_2d/2d/godot_navigation_server_2d.cpp` — 同上
  - [x] `modules/navigation_2d/nav_map_2d.cpp` — 同上
  - [x] Zone 命名：`godot:navigation/<sub>/<op>`

- [x] Task 6.3: Object Lifecycle Zone 扩桩
  - [x] `core/object/object.cpp` — 在 memnew/memdelete 通知、notification 等添加 Zone
  - [x] Zone 命名：`godot:object/<op>`

- [x] Task 6.4: Networking Zone 扩桩
  - [x] `modules/multiplayer/scene_multiplayer.cpp` — 在 RPC 等添加 Zone
  - [x] `modules/multiplayer/scene_rpc_interface.cpp` — 在 send/recv 等添加 Zone
  - [x] Zone 命名：`godot:network/<op>`，颜色 `GODOT_INSIGHTS_COLOR_NETWORK`

## 7. InsightsManager 内置 Channel 自动注册

- [x] Task 7.1: 修改 InsightsManager::start_capture()
  - [x] 在录制开始时自动创建并注册 MemoryChannel、LogChannel、ScriptChannel、LoadingChannel
  - [x] 将内置 Channel 存储到成员变量中以便快速访问

- [x] Task 7.2: 修改 InsightsManager::stop_capture()
  - [x] 在录制停止时自动注销并释放内置 Channel
  - [x] 调用 ResourceLoadTracker::clear() 清空追踪数据

- [x] Task 7.3: 添加 InsightsManager 便捷访问方法
  - [x] `get_memory_channel()` — 返回 MemoryChannel 指针
  - [x] `get_log_channel()` — 返回 LogChannel 指针
  - [x] `get_script_channel()` — 返回 ScriptChannel 指针
  - [x] `get_loading_channel()` — 返回 LoadingChannel 指针

## 8. 编译验证

- [x] Task 8.1: 更新 `modules/insights/register_types.cpp`
  - [x] 添加 MemoryChannel、LogChannel、ScriptChannel、LoadingChannel、ResourceLoadTracker 的 include
  - [x] 添加 GDREGISTER_CLASS 注册
  - [x] 添加 VARIANT_ENUM_CAST(LogChannel::Severity) 和 VARIANT_ENUM_CAST(ScriptChannel::Language)

- [x] Task 8.2: 更新 `modules/insights/config.py`
  - [x] 在 doc_classes 列表中添加新类名

- [x] Task 8.3: 编译验证
  - [x] 运行 `scons module_insights_enabled=yes platform=windows dev_build=yes` 确保编译通过
  - [x] 确认无新增编译警告

# Task Dependencies

- Task 2.1 depends on Task 1.1, 1.2, 1.3, 1.4（接口扩展需要先定义 Channel 子类）
- Task 2.2 depends on Task 2.1（NativeCapture 实现依赖接口定义）
- Task 3.1 is independent（可与 Task 1.x 并行）
- Task 4.1 depends on Task 1.1（Memory Hook 需要 MemoryChannel）
- Task 4.2 depends on Task 1.2（Log Hook 需要 LogChannel）
- Task 4.3 depends on Task 1.4, 3.1（Loading Hook 需要 LoadingChannel 和 ResourceLoadTracker）
- Task 4.4 depends on Task 1.3（Script Hook 需要 ScriptChannel）
- Task 5.x are independent of each other（可并行扩桩）
- Task 6.x are independent of each other（可并行扩桩）
- Task 7.1 depends on Task 1.1, 1.2, 1.3, 1.4（自动注册需要所有 Channel 子类）
- Task 8.1 depends on Task 1.x, 2.x, 3.1, 7.x（注册需要所有新类）
- Task 8.3 depends on all other tasks
