# Checklist — Phase 5：脚本/C# 与录制工作流

## ScriptChannel C# 语言扩展

- [x] ScriptChannel::Language 枚举新增 C_SHARP
- [x] CallRecord 新增 was_suspended/suspend_ns/resume_ns 字段
- [x] suspend_function() 方法已实现
- [x] resume_function() 方法已实现
- [x] _bind_methods 已更新

## MonoProfilerBridge

- [x] MonoProfilerBridge 类已创建
- [x] enter_function() 调用 ScriptChannel，language=C_SHARP
- [x] leave_function() 调用 ScriptChannel
- [x] suspend_function() 调用 ScriptChannel
- [x] resume_function() 调用 ScriptChannel
- [x] Mono 运行时回调注册（条件编译 MODULE_MONO_ENABLED）

## GDScript @profiler_zone 装饰器

- [x] GDScriptProfilerDecorator 类已创建
- [x] parse_decorator() 解析无参数和带参数装饰器
- [x] generate_zone_code() 生成 Zone 宏调用代码
- [x] GDScript 解析器注册 @profiler_zone 为合法装饰器
- [x] 函数编译时检查装饰器并注入 Zone 代码

## InsightsLauncher

- [x] InsightsLauncher 类已创建
- [x] launch_with_insights() 启动带 Tracy 的进程
- [x] is_running() 检查进程状态
- [x] stop() 终止进程
- [x] get_port() 返回 Tracy 端口

## TracyConverter

- [x] TracyConverter 类已创建
- [x] gitracy_to_tracy() 格式转换
- [x] tracy_to_gitracy() 反向转换
- [x] is_tracy_file() 文件格式检测

## InsightsCLI

- [x] InsightsCLI 类已创建
- [x] export_command() 导出 .gitracy 为指定格式
- [x] compare_command() 比较两个 trace 文件
- [x] get_last_report() 返回比较报告
- [x] run() CLI 入口点

## InsightsManager 远程连接

- [x] is_connected_to_remote() 方法已实现
- [x] connect_to_remote() 方法已实现（stub）
- [x] disconnect_from_remote() 方法已实现（stub）

## 注册与编译

- [x] register_types.cpp 已注册所有新类
- [x] config.py 的 doc_classes 列表已添加新类名
- [x] SCsub 已添加 scripting/*.cpp 和 tools/*.cpp 编译
- [x] `scons module_insights_enabled=yes platform=windows dev_build=yes` 编译通过

## 单元测试

- [x] 测试文件 `modules/insights/tests/test_insights_phase5.h` 已创建
- [x] C# Mono 方法调用测试通过 — MonoProfilerBridge 可实例化
- [x] C# Mono 方法调用测试通过 — enter/leave_function 产生正确 CallRecord
- [x] C# Mono 方法调用测试通过 — language 字段为 C_SHARP
- [x] C# 异步/协程测试通过 — was_suspended 为 true
- [x] C# 异步/协程测试通过 — resume_ns > suspend_ns
- [x] GDScript 装饰器测试通过 — 无参数装饰器解析正确
- [x] GDScript 装饰器测试通过 — 自定义名称装饰器解析正确
- [x] GDScript 装饰器测试通过 — generate_zone_code 生成正确代码
- [x] 转换器测试通过 — TracyConverter 可实例化
- [x] 转换器测试通过 — is_tracy_file() 检测功能正常
- [x] 转换器测试通过 — gitracy_to_tracy() 不崩溃
- [x] 启动工具测试通过 — InsightsLauncher 可实例化
- [x] 启动工具测试通过 — is_running() 初始返回 false
- [x] 启动工具测试通过 — get_port() 返回默认值
- [x] CLI 测试通过 — InsightsCLI 可实例化
- [x] CLI 测试通过 — compare_command 返回正确退出码
- [x] CLI 测试通过 — get_last_report() 包含回归信息
