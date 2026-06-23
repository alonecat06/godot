# Checklist — Phase 2：录制核心

## Channel 子类

- [x] MemoryChannel 类已创建，继承 InsightsChannel，category=CHANNEL_CATEGORY_MEMORY
- [x] MemoryChannel::on_alloc() 正确记录分配事件（ptr, size, thread_id, timestamp_ns）
- [x] MemoryChannel::on_free() 正确配对释放事件，更新 free_ns
- [x] MemoryChannel::detect_leaks() 返回 free_ns==0 的分配列表
- [x] MemoryChannel::get_total_allocated() 返回当前已分配总字节数
- [x] LogChannel 类已创建，继承 InsightsChannel，category=CHANNEL_CATEGORY_LOG
- [x] LogChannel::Severity 枚举已定义并通过 VARIANT_ENUM_CAST 绑定
- [x] LogChannel::log_message() 根据严重级别阈值过滤消息
- [x] LogChannel::get_messages_in_range() 支持按时间范围和最低级别查询
- [x] ScriptChannel 类已创建，继承 InsightsChannel，category=CHANNEL_CATEGORY_SCRIPT
- [x] ScriptChannel::enter_function()/leave_function() 正确记录函数调用栈
- [x] ScriptChannel::on_gc_event() 正确记录 GC 事件
- [x] ScriptChannel::Language 枚举已定义并通过 VARIANT_ENUM_CAST 绑定
- [x] LoadingChannel 类已创建，继承 InsightsChannel，category=CHANNEL_CATEGORY_LOADING
- [x] LoadingChannel::on_load_begin()/on_load_end()/on_load_fail() 正确记录加载生命周期
- [x] LoadingChannel::get_load_events_in_range() 支持按时间范围查询

## InsightsCapture 与 NativeCapture 扩展

- [x] InsightsCapture 新增 on_resource_load() 纯虚方法
- [x] InsightsCapture 新增 on_log_message() 纯虚方法
- [x] InsightsCapture 新增 on_gc_event() 纯虚方法
- [x] NativeCapture::EventType 新增 RESOURCE_LOAD、LOG_MESSAGE、GC_EVENT
- [x] NativeCapture::TraceEvent 联合体新增 resource_load、log_message、gc_event 数据结构
- [x] NativeCapture 实现 on_resource_load()/on_log_message()/on_gc_event() 虚方法
- [x] NativeCapture::_process_events() 正确处理新事件类型并写入 InsightsDatabase

## ResourceLoadTracker

- [x] ResourceLoadTracker 单例类已创建
- [x] LoadEvent 结构体包含 path, start_ns, end_ns, memory_size, source_loader, parent_stack, thread_id
- [x] on_load_begin() 正确记录 start_ns 和当前 load_paths_stack 作为 parent_stack
- [x] on_load_end() 正确更新 end_ns 和 memory_size
- [x] on_load_fail() 正确标记加载失败
- [x] get_events_in_range() 返回时间范围内的加载事件
- [x] get_active_loads() 返回当前正在加载的资源

## 引擎核心 Hook

- [x] Memory::alloc_static() 中有 MODULE_INSIGHTS_ENABLED 条件编译块，录制时调用 MemoryChannel::on_alloc()
- [x] Memory::free_static() 中有 MODULE_INSIGHTS_ENABLED 条件编译块，录制时调用 MemoryChannel::on_free()
- [x] OS::print_error() 中有 MODULE_INSIGHTS_ENABLED 条件编译块，录制时调用 LogChannel::log_message()
- [x] OS::print_error() 中 Logger::ErrorType 正确映射到 LogChannel::Severity
- [x] ResourceLoader::_load() 入口有 GodotProfileZoneH("loading", "resource", "load")
- [x] ResourceLoader::_load() 中调用 ResourceLoadTracker::on_load_begin()/on_load_end()
- [x] GDScript VM 函数调用处增强为同时通知 ScriptChannel
- [x] GDScript GC 相关代码有 GodotProfileZoneC(GODOT_INSIGHTS_COLOR_SCRIPT, "godot:script/gdscript/gc")
- [x] 所有引擎 Hook 修改均使用 #ifdef MODULE_INSIGHTS_ENABLED 保护

## CPU Zone 扩桩 — P0

- [x] Physics 2D 关键函数有 Zone 宏（命名 `godot:physics/2d/<op>`）
- [x] Physics 3D 关键函数有 Zone 宏（命名 `godot:physics/3d/<op>`）
- [x] Scene Tree 关键函数有 Zone 宏（命名 `godot:scene/<sub>/<op>`，从 1 个扩展到 ~15 个）
- [x] Node 关键函数有 Zone 宏（_ready, _enter_tree, _exit_tree 等）
- [x] Viewport 关键函数有 Zone 宏
- [x] Resource Loader 关键函数有 Zone 宏（命名 `godot:loading/<sub>/<op>`）
- [x] Rendering Substage 关键函数有 Zone 宏（命名 `godot:rendering/<sub>/<op>`）
- [x] 所有新增 Zone 宏使用 GodotProfileZoneC 或 GodotProfileZoneH
- [x] 所有新增 Zone 宏在 GODOT_USE_TRACY 未定义时为 no-op

## CPU Zone 扩桩 — P1

- [x] Audio Server 关键函数有 Zone 宏（命名 `godot:audio/<op>`）
- [x] Navigation 关键函数有 Zone 宏（命名 `godot:navigation/<sub>/<op>`）
- [x] Object Lifecycle 关键函数有 Zone 宏（命名 `godot:object/<op>`）
- [x] Networking 关键函数有 Zone 宏（命名 `godot:network/<op>`）

## InsightsManager 集成

- [x] InsightsManager::start_capture() 自动创建并注册 MemoryChannel、LogChannel、ScriptChannel、LoadingChannel
- [x] InsightsManager::stop_capture() 自动注销并释放内置 Channel
- [x] InsightsManager 提供 get_memory_channel()/get_log_channel()/get_script_channel()/get_loading_channel() 便捷方法

## 注册与编译

- [x] register_types.cpp 已注册 MemoryChannel、LogChannel、ScriptChannel、LoadingChannel、ResourceLoadTracker
- [x] config.py 的 doc_classes 列表已添加新类名
- [x] VARIANT_ENUM_CAST(LogChannel::Severity) 和 VARIANT_ENUM_CAST(ScriptChannel::Language) 已添加
- [x] `scons module_insights_enabled=yes platform=windows dev_build=yes` 编译通过，无新增警告
