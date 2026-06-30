# Tasks — Phase 6：增强（持续）

## 1. CPUChannel 锁竞争检测

- [x] Task 1.1: 实现 CPUChannel 锁竞争追踪（`modules/insights/channels/cpu_channel.h/cpp`）
  - [x] 定义 ContentionEvent 结构体（lock_name, owner_thread, waiter_thread, wait_time_ns, acquire_ns, release_ns）
  - [x] 新增 enable_contention_tracking() / disable_contention_tracking()
  - [x] 新增 on_lock_acquire(lock_name, thread_id, timestamp_ns)
  - [x] 新增 on_lock_attempt(lock_name, thread_id, timestamp_ns)
  - [x] 新增 on_lock_release(lock_name, thread_id, timestamp_ns)
  - [x] 新增 get_contention_events() 返回已记录的竞争事件
  - [x] 新增 is_contention_tracking_enabled()
  - [x] 更新 _bind_methods
  - [x] GDREGISTER_CLASS 注册

## 2. CustomChannel API

- [x] Task 2.1: 实现 CustomChannel（`modules/insights/channels/custom_channel.h/cpp`）
  - [x] 定义 CustomZone 结构体（name, channel, file, line, start_ns, end_ns）
  - [x] 新增 begin_zone(name, file, line) — 开始自定义 Zone
  - [x] 新增 end_zone() — 结束当前 Zone
  - [x] 新增 get_zones() — 返回所有 Zone 记录
  - [x] 继承 InsightsChannel，category 为 CHANNEL_CATEGORY_CUSTOM
  - [x] 更新 _bind_methods
  - [x] GDREGISTER_CLASS 注册

## 3. WebExporter

- [x] Task 3.1: 实现 WebExporter（`modules/insights/tools/web_exporter.h/cpp`）
  - [x] 定义 HTML 模板（含内嵌 CSS/JS，Timeline + Flamegraph）
  - [x] 实现 export_to_html(gitracy_path, html_path) — 读取 .gitracy，生成 HTML
  - [x] 实现 _generate_timeline_html(database) — 生成时间轴 HTML 片段
  - [x] 实现 _generate_flamegraph_html(database) — 生成火焰图 HTML 片段
  - [x] 更新 _bind_methods
  - [x] GDREGISTER_CLASS 注册

## 4. Live Profiling

- [x] Task 4.1: 扩展 InsightsManager live capture 支持
  - [x] 新增 bool live_mode 成员
  - [x] 新增 Ref<InsightsDatabase> live_database 成员
  - [x] 新增 start_live_capture(host, port) 方法（stub）
  - [x] 新增 stop_live_capture() 方法
  - [x] 新增 is_live_mode() 方法
  - [x] 新增 get_live_database() 方法
  - [x] 新增 _on_live_frame_received(start_ns, end_ns) 方法
  - [x] 新增 _on_live_zone_received(name, thread_id, start_ns, end_ns, depth) 方法
  - [x] 更新 _bind_methods

## 5. AIAnalyzer

- [x] Task 5.1: 实现 AIAnalyzer（`modules/insights/insights_core/ai_analyzer.h/cpp`）
  - [x] 定义 AnalysisResult 结构体（bottleneck, suggestions, severity）
  - [x] 定义 Severity 枚举（LOW, MEDIUM, HIGH, CRITICAL）
  - [x] 实现 build_analysis_prompt(database) — 根据 zone 数据构建分析提示词
  - [x] 实现 parse_response(json_string) — 解析 LLM JSON 响应为 AnalysisResult
  - [x] 更新 _bind_methods
  - [x] GDREGISTER_CLASS 注册

## 6. 锁竞争编辑器面板

- [x] Task 6.1: 实现 InsightsContentionPanel（`modules/insights/editor/insights_contention_panel.h/cpp`）
  - [x] 继承 VBoxContainer，条件编译 TOOLS_ENABLED
  - [x] 显示 ContentionEvent 列表（Tree 控件）
  - [x] 显示等待时间最长的竞争
  - [x] set_database(db) 接口
  - [x] GDREGISTER_CLASS 注册

## 7. 注册与编译

- [x] Task 7.1: 更新 `modules/insights/register_types.cpp`
  - [x] 添加 CPUChannel、CustomChannel、WebExporter、AIAnalyzer、InsightsContentionPanel 的 include
  - [x] 添加 GDREGISTER_CLASS 注册

- [x] Task 7.2: 更新 `modules/insights/config.py`
  - [x] 在 doc_classes 列表中添加新类名

- [x] Task 7.3: 编译验证
  - [x] 运行 `scons module_insights_enabled=yes platform=windows dev_build=yes` 确保编译通过

## 8. 单元测试

> 对应设计文档 `godot_insights_design.md` Phase 6 单元测试部分，使用 doctest 框架的 `TEST_CASE` 宏。
> 测试文件路径：`modules/insights/tests/test_insights_phase6.h`

- [x] Task 8.1: 创建 Lock contention 检测测试
  - [x] 创建测试文件 `modules/insights/tests/test_insights_phase6.h`
  - [x] 测试 CPUChannel 可实例化
  - [x] 测试 enable_contention_tracking 后记录锁事件
  - [x] 测试 ContentionEvent 数量和 wait_time_ns 计算

- [x] Task 8.2: 创建 Custom Channel API 测试
  - [x] 测试 CustomChannel 可实例化并设置 name/color
  - [x] 测试 begin_zone/end_zone 产生正确的 Zone 记录
  - [x] 测试 InsightsManager::register_channel 后可查询

- [x] Task 8.3: 创建 Web export 测试
  - [x] 测试 WebExporter 可实例化
  - [x] 测试 export_to_html 生成文件不崩溃

- [x] Task 8.4: 创建 Live profiling 测试
  - [x] 测试 start_live_capture / stop_live_capture 生命周期
  - [x] 测试 is_live_mode 状态切换

- [x] Task 8.5: 创建 AI analysis 测试
  - [x] 测试 AIAnalyzer 可实例化
  - [x] 测试 build_analysis_prompt 包含 zone 信息
  - [x] 测试 parse_response 正确解析 JSON

# Task Dependencies

- Task 1.1 (CPUChannel 锁竞争) is independent
- Task 2.1 (CustomChannel) is independent
- Task 3.1 (WebExporter) is independent
- Task 4.1 (Live Profiling) is independent
- Task 5.1 (AIAnalyzer) is independent
- Task 6.1 (ContentionPanel) depends on Task 1.1
- Task 7.x depends on all other tasks
- Task 8.x depends on Task 7.x
