# Checklist — Phase 6：增强（持续）

## CPUChannel 锁竞争检测

- [x] CPUChannel 类已创建
- [x] ContentionEvent 结构体已定义（lock_name, owner_thread, waiter_thread, wait_time_ns）
- [x] enable_contention_tracking() / disable_contention_tracking() 已实现
- [x] on_lock_acquire() 已实现
- [x] on_lock_attempt() 已实现
- [x] on_lock_release() 已实现
- [x] get_contention_events() 已实现
- [x] _bind_methods 已更新

## CustomChannel API

- [x] CustomChannel 类已创建
- [x] CustomZone 结构体已定义
- [x] begin_zone() 已实现
- [x] end_zone() 已实现
- [x] get_zones() 已实现
- [x] 可通过 InsightsManager::register_channel 注册
- [x] _bind_methods 已更新

## WebExporter

- [x] WebExporter 类已创建
- [x] export_to_html() 格式转换
- [x] HTML 包含 timeline 和 flamegraph 内容
- [x] _bind_methods 已更新

## Live Profiling

- [x] InsightsManager 新增 live_mode 成员
- [x] start_live_capture() 已实现（stub）
- [x] stop_live_capture() 已实现
- [x] is_live_mode() 已实现
- [x] get_live_database() 已实现
- [x] _on_live_frame_received() 已实现
- [x] _on_live_zone_received() 已实现
- [x] _bind_methods 已更新

## AIAnalyzer

- [x] AIAnalyzer 类已创建
- [x] AnalysisResult 结构体已定义（bottleneck, suggestions, severity）
- [x] Severity 枚举已定义
- [x] build_analysis_prompt() 已实现
- [x] parse_response() 已实现
- [x] _bind_methods 已更新

## 锁竞争编辑器面板

- [x] InsightsContentionPanel 类已创建
- [x] ContentionEvent 列表显示
- [x] 条件编译 TOOLS_ENABLED 保护

## 注册与编译

- [x] register_types.cpp 已注册所有新类
- [x] config.py 的 doc_classes 列表已添加新类名
- [x] `scons module_insights_enabled=yes platform=windows dev_build=yes` 编译通过

## 单元测试

- [x] 测试文件 `modules/insights/tests/test_insights_phase6.h` 已创建
- [x] Lock contention 测试通过 — CPUChannel 可实例化
- [x] Lock contention 测试通过 — ContentionEvent 数量正确
- [x] Lock contention 测试通过 — wait_time_ns 计算正确
- [x] Custom Channel 测试通过 — CustomChannel 可实例化
- [x] Custom Channel 测试通过 — begin_zone/end_zone 产生正确记录
- [x] Custom Channel 测试通过 — InsightsManager 可查询注册的 Channel
- [x] Web export 测试通过 — WebExporter 可实例化
- [x] Web export 测试通过 — export_to_html 不崩溃
- [x] Live profiling 测试通过 — start/stop live capture 生命周期正确
- [x] Live profiling 测试通过 — is_live_mode 状态切换正确
- [x] AI analysis 测试通过 — AIAnalyzer 可实例化
- [x] AI analysis 测试通过 — build_analysis_prompt 包含 zone 信息
- [x] AI analysis 测试通过 — parse_response 正确解析 JSON
