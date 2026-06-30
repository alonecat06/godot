# Tasks — Phase 5：脚本/C# 与录制工作流

## 1. ScriptChannel C# 语言扩展

- [x] Task 1.1: 扩展 ScriptChannel 支持 C# 语言和异步/协程
  - [x] 在 Language 枚举中新增 C_SHARP
  - [x] 在 CallRecord 结构体中新增 was_suspended(bool)、suspend_ns(uint64_t)、resume_ns(uint64_t) 字段
  - [x] 新增 suspend_function(name, script_path, line) 方法
  - [x] 新增 resume_function(name) 方法
  - [x] 更新 _bind_methods 绑定新字段和方法

## 2. MonoProfilerBridge

- [x] Task 2.1: 实现 MonoProfilerBridge（`modules/insights/scripting/mono_profiler_bridge.h/cpp`）
  - [x] 定义静态方法 enter_function(name, script_path, line) — 调用 ScriptChannel::enter_function，language=C_SHARP
  - [x] 定义静态方法 leave_function() — 调用 ScriptChannel::leave_function
  - [x] 定义静态方法 suspend_function(name) — 调用 ScriptChannel::suspend_function
  - [x] 定义静态方法 resume_function(name) — 调用 ScriptChannel::resume_function
  - [x] 实现 Mono 运行时回调注册（stub，条件编译 MODULE_MONO_ENABLED）
  - [x] GDREGISTER_CLASS 注册

## 3. GDScript @profiler_zone 装饰器

- [x] Task 3.1: 实现 GDScript 装饰器支持（`modules/insights/scripting/gdscript_profiler_decorator.h/cpp`）
  - [x] 定义 GDScriptProfilerDecorator 类，提供装饰器解析和 Zone 生成
  - [x] 实现 parse_decorator(annotation) — 解析 @profiler_zone 和 @profiler_zone("name")
  - [x] 实现 generate_zone_code(function_name, zone_name) — 生成 Zone 宏调用代码
  - [x] GDREGISTER_CLASS 注册

- [x] Task 3.2: 修改 GDScript 解析器支持 @profiler_zone
  - [x] 在 `modules/gdscript/gdscript_parser.cpp` 中注册 @profiler_zone 为合法装饰器
  - [x] 在函数编译时检查装饰器，注入 Zone 生成代码

## 4. InsightsLauncher

- [x] Task 4.1: 实现 InsightsLauncher（`modules/insights/tools/insights_launcher.h/cpp`）
  - [x] 定义成员：process_id, port, is_running_flag
  - [x] 实现 launch_with_insights(project_path, port) — 启动 Godot 进程并启用 Tracy
  - [x] 实现 is_running() — 检查进程状态
  - [x] 实现 stop() — 终止进程
  - [x] 实现 get_port() — 返回 Tracy 端口
  - [x] GDREGISTER_CLASS 注册

## 5. TracyConverter

- [x] Task 5.1: 实现 TracyConverter（`modules/insights/tools/tracy_converter.h/cpp`）
  - [x] 定义 Tracy 文件头结构（Tracy 二进制格式 magic + version）
  - [x] 实现 gitracy_to_tracy(gitracy_path, tracy_path) — 读取 .gitracy，写入 .tracy
  - [x] 实现 tracy_to_gitracy(tracy_path, gitracy_path) — 读取 .tracy，写入 .gitracy
  - [x] 实现 is_tracy_file(path) — 检测文件是否为 Tracy 格式
  - [x] GDREGISTER_CLASS 注册

## 6. InsightsCLI

- [x] Task 6.1: 实现 InsightsCLI（`modules/insights/tools/insights_cli.h/cpp`）
  - [x] 定义 CLI 子命令枚举：EXPORT, COMPARE, INFO
  - [x] 实现 export_command(gitracy_path, format) — 导出 .gitracy 为指定格式
  - [x] 实现 compare_command(baseline_path, pr_path, threshold) — 比较两个 trace
  - [x] 实现 get_last_report() — 返回最近一次比较的文本报告
  - [x] 实现 run(argc, argv) — CLI 入口点
  - [x] GDREGISTER_CLASS 注册

## 7. InsightsManager 远程连接扩展

- [x] Task 7.1: 扩展 InsightsManager 远程连接支持
  - [x] 新增 is_connected_to_remote() 方法
  - [x] 新增 connect_to_remote(host, port) 方法（stub）
  - [x] 新增 disconnect_from_remote() 方法（stub）
  - [x] 更新 _bind_methods

## 8. 注册与编译

- [x] Task 8.1: 更新 `modules/insights/register_types.cpp`
  - [x] 添加新类的 include
  - [x] 添加 GDREGISTER_CLASS 注册

- [x] Task 8.2: 更新 `modules/insights/config.py`
  - [x] 在 doc_classes 列表中添加新类名

- [x] Task 8.3: 更新 `modules/insights/SCsub`
  - [x] 添加 `env_insights.add_source_files(module_obj, "scripting/*.cpp")`
  - [x] 添加 `env_insights.add_source_files(module_obj, "tools/*.cpp")`

- [x] Task 8.4: 编译验证
  - [x] 运行 `scons module_insights_enabled=yes platform=windows dev_build=yes` 确保编译通过

## 9. 单元测试

> 对应设计文档 `godot_insights_design.md` Phase 5 单元测试部分，使用 doctest 框架的 `TEST_CASE` 宏。
> 测试文件路径：`modules/insights/tests/test_insights_phase5.h`

- [x] Task 9.1: 创建 C# Mono 方法调用插桩测试
  - [x] 创建测试文件 `modules/insights/tests/test_insights_phase5.h`
  - [x] 测试 MonoProfilerBridge 可实例化
  - [x] 测试 enter_function/leave_function 调用后 ScriptChannel 有正确的 CallRecord
  - [x] 测试 CallRecord 的 language 字段为 C_SHARP

- [x] Task 9.2: 创建 C# 异步/协程 zone 生命周期测试
  - [x] 测试 suspend_function/resume_function 调用后 CallRecord.was_suspended 为 true
  - [x] 测试 resume_ns > suspend_ns

- [x] Task 9.3: 创建 GDScript @profiler_zone 装饰器测试
  - [x] 测试 GDScriptProfilerDecorator 解析无参数装饰器
  - [x] 测试 GDScriptProfilerDecorator 解析自定义名称装饰器
  - [x] 测试 generate_zone_code 生成正确的 Zone 代码

- [x] Task 9.4: 创建 .gitracy ↔ .tracy 转换器测试
  - [x] 测试 TracyConverter 可实例化
  - [x] 测试 is_tracy_file() 检测功能
  - [x] 测试 gitracy_to_tracy() 转换不崩溃

- [x] Task 9.5: 创建启动带 Tracy 工具测试
  - [x] 测试 InsightsLauncher 可实例化
  - [x] 测试 is_running() 初始返回 false
  - [x] 测试 get_port() 返回默认值

- [x] Task 9.6: 创建 CI 命令行工具测试
  - [x] 测试 InsightsCLI 可实例化
  - [x] 测试 compare_command 返回正确的退出码
  - [x] 测试 get_last_report() 返回包含回归信息的报告

# Task Dependencies

- Task 1.1 (ScriptChannel 扩展) is independent
- Task 2.1 (MonoProfilerBridge) depends on Task 1.1
- Task 3.1 (GDScript 装饰器) is independent
- Task 3.2 (GDScript 解析器修改) depends on Task 3.1
- Task 4.1 (InsightsLauncher) is independent
- Task 5.1 (TracyConverter) is independent
- Task 6.1 (InsightsCLI) depends on Task 5.1
- Task 7.1 (InsightsManager 远程连接) is independent
- Task 8.x depends on all other tasks
- Task 9.x depends on Task 8.x
