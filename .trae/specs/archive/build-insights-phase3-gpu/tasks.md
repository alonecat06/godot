# Tasks — Phase 3：GPU Profiling

## 1. GPUTimestampQuery 抽象接口

- [x] Task 1.1: 实现 GPUTimestampQuery 抽象基类（`modules/insights/gpu/gpu_timestamp_query.h`）
  - [x] 定义 Stage 枚举（BEGIN_RENDER_PASS, END_RENDER_PASS, DRAW, DISPATCH, PIPELINE_BARRIER, COPY, PRESENT）
  - [x] 定义纯虚方法 `write_timestamp(cmd_buffer, stage)` → uint32_t query_index
  - [x] 定义纯虚方法 `fetch_results(results, count)` → bool
  - [x] 定义虚方法 `begin_frame(cmd_buffer)` 和 `end_frame()`
  - [x] 定义虚方法 `is_supported()` → bool
  - [x] 定义虚方法 `get_timestamp_period()` → float（返回 GPU 时间戳精度）

## 2. GPU 后端实现

- [x] Task 2.1: 实现 GPUProfilerVulkan（`modules/insights/gpu/gpu_profiler_vulkan.h/cpp`）
  - [x] 继承 GPUTimestampQuery
  - [x] 实现 `write_timestamp()` — stub 实现，未初始化时返回 0
  - [x] 实现 `fetch_results()` — stub 实现
  - [x] 实现 `begin_frame()` — 重置 current_query
  - [x] 实现 `is_supported()` — 检查 initialized 标志
  - [x] 实现 `get_timestamp_period()` — 返回默认 timestamp_period
  - [x] GDREGISTER_CLASS 注册

- [x] Task 2.2: 实现 GPUProfilerD3D12（`modules/insights/gpu/gpu_profiler_d3d12.h/cpp`）
  - [x] 继承 GPUTimestampQuery
  - [x] 实现 `write_timestamp()` — stub 实现
  - [x] 实现 `fetch_results()` — stub 实现
  - [x] 实现 `is_supported()` — 检查 initialized 标志
  - [x] GDREGISTER_CLASS 注册

- [x] Task 2.3: 实现 GPUProfilerMetal（`modules/insights/gpu/gpu_profiler_metal.h/cpp`）
  - [x] 继承 GPUTimestampQuery
  - [x] 实现 `write_timestamp()` — stub 实现
  - [x] 实现 `fetch_results()` — stub 实现
  - [x] 实现 `is_supported()` — 检查 initialized 标志
  - [x] GDREGISTER_CLASS 注册

## 3. GPUChannel 实现

- [x] Task 3.1: 实现 GPUChannel（`modules/insights/channels/gpu_channel.h/cpp`）
  - [x] 继承 InsightsChannel，category=CHANNEL_CATEGORY_GPU，color=GODOT_INSIGHTS_COLOR_GPU
  - [x] 实现 `on_gpu_timestamp(name, gpu_time_ns, cpu_time_ns, frame_index)` — 记录单个 GPU 时间戳
  - [x] 实现 `on_gpu_zone(name, queue_id, submit_ns, start_ns, end_ns, context_id)` — 记录 GPU Zone
  - [x] 实现 `collect_frame_timestamps(RenderingDevice*)` — 从 RD 读取所有时间戳
  - [x] 实现 `get_gpu_zones_in_range(start_ns, end_ns)` — 查询时间范围内的 GPU Zone
  - [x] 实现 `get_gpu_zones_for_cpu_zone(cpu_zone_id)` — GPU-CPU 关联查询
  - [x] 实现 `on_event()` 和 `serialize()` 虚方法
  - [x] GDREGISTER_CLASS 注册

## 4. InsightsCapture/NativeCapture 扩展

- [x] Task 4.1: 扩展 InsightsCapture 抽象基类
  - [x] 新增纯虚方法 `on_gpu_zone(name, queue_id, submit_ns, start_ns, end_ns, context_id)`

- [x] Task 4.2: 扩展 NativeCapture 事件类型
  - [x] 在 EventType 枚举中新增 GPU_ZONE
  - [x] 在 TraceEvent 联合体中添加 gpu_zone 数据结构（name[128], queue_id, submit_ns, start_ns, end_ns, context_id）
  - [x] 实现 `on_gpu_zone()` 虚方法
  - [x] 在 `_process_events()` 中处理 GPU_ZONE 事件，调用 database->insert_gpu_zone()

## 5. RenderingDevice GPU 桥接

- [x] Task 5.1: 修改 `servers/rendering/rendering_device.cpp`
  - [x] 在 `_end_frame()` 中添加 `#ifdef MODULE_INSIGHTS_ENABLED` 条件编译块
  - [x] 录制期间调用 GPUChannel::collect_frame_timestamps(this)
  - [x] 利用已有的 `get_captured_timestamps_count/name/gpu_time/cpu_time` API

## 6. InsightsManager 集成

- [x] Task 6.1: 修改 InsightsManager
  - [x] 新增 gpu_channel 成员指针
  - [x] 在 start_capture() 中自动创建并注册 GPUChannel
  - [x] 在 stop_capture() 中自动注销并释放 GPUChannel
  - [x] 新增 `get_gpu_channel()` 便捷方法

## 7. InsightsDatabase GPU 查询扩展

- [x] Task 7.1: 确保 InsightsDatabase GPUZoneRecord API 完整
  - [x] 验证 `insert_gpu_zone()` 已实现（Phase 1 已定义）
  - [x] 实现 `query_gpu_zones_for_cpu_zone(cpu_zone_id)` — 按 CPU Zone 查询关联 GPU Zone
  - [x] 实现 `query_gpu_zones_in_range(start_ns, end_ns)` — 按时间范围查询 GPU Zone

## 8. 注册与编译

- [x] Task 8.1: 更新 `modules/insights/register_types.cpp`
  - [x] 添加 GPUChannel、GPUTimestampQuery、GPUProfilerVulkan、GPUProfilerD3D12、GPUProfilerMetal 的 include
  - [x] 添加 GDREGISTER_ABSTRACT_CLASS(GPUTimestampQuery) 和 GDREGISTER_CLASS 注册

- [x] Task 8.2: 更新 `modules/insights/config.py`
  - [x] 在 doc_classes 列表中添加新类名

- [x] Task 8.3: 更新 `modules/insights/SCsub`
  - [x] 添加 `env_insights.add_source_files(module_obj, "gpu/*.cpp")` 编译 GPU 后端

- [x] Task 8.4: 编译验证
  - [x] 运行 `scons module_insights_enabled=yes platform=windows dev_build=yes` 确保编译通过

## 9. 单元测试

> 对应设计文档 `godot_insights_design.md` Phase 3 单元测试部分，使用 doctest 框架的 `TEST_CASE` 宏。
> 测试文件路径：`modules/insights/tests/test_insights_phase3.h`（SCons 自动发现并生成 `modules_tests.gen.h`）

- [x] Task 9.1: 创建 GPU 时间戳 RDD 接口测试
  - [x] 创建测试文件 `modules/insights/tests/test_insights_phase3.h`
  - [x] 测试 GPUChannel::on_gpu_timestamp() 不崩溃
  - [x] 测试 GPUChannel 的 zone_count 正确
  - [x] 测试 GPUChannel 的 name 属性为 "gpu"

- [x] Task 9.2: 创建 Vulkan 时间戳精度测试
  - [x] 测试 GPUProfilerVulkan 可实例化（`memnew(GPUProfilerVulkan)` 不崩溃）
  - [x] 测试 `get_timestamp_period()` 返回值 > 0 且 < 1000（默认 1.0f）
  - [x] 测试 `get_max_queries()` 返回 256
  - [x] 测试 `is_supported()` 在未初始化时返回 false
  - [x] 测试 `write_timestamp()` 在未初始化时返回 0 且不崩溃

- [x] Task 9.3: 创建 GPU Zone 端到端时间测量测试
  - [x] 测试 GPUChannel::on_gpu_zone() 记录 GPUZoneRecord
  - [x] 测试 GPUZoneRecord 的 `start_ns < end_ns`（构造有效的时间范围）
  - [x] 测试 GPU Zone 持续时间 > 0 且 < 1s（纳秒单位）
  - [x] 测试 `GPUChannel::get_gpu_zones_in_range()` 能正确查询到记录
  - [x] 测试 `GPUChannel::get_zone_count()` 返回正确数量

- [x] Task 9.4: 创建 GPU-CPU 关联测试
  - [x] 测试 InsightsDatabase::insert_gpu_zone() 插入 GPU Zone 记录
  - [x] 测试 InsightsDatabase 同时存在 CPU Zone 和 GPU Zone 记录
  - [x] 测试 `query_gpu_zones_in_range()` 返回正确结果
  - [x] 测试 GPU Zone 的 `submit_ns` 在 CPU Zone 的时间范围内

- [x] Task 9.5: 创建多后端兼容性测试
  - [x] 测试 GPUProfilerVulkan 可实例化且 `is_supported()` 可调用
  - [x] 测试 GPUProfilerD3D12 可实例化且 `is_supported()` 可调用
  - [x] 测试 GPUProfilerMetal 可实例化且 `is_supported()` 可调用
  - [x] 测试各后端继承自 GPUTimestampQuery 抽象基类（`Object::cast_to<GPUTimestampQuery>()` 非空）
  - [x] 测试各后端的 `get_timestamp_period()` 和 `get_max_queries()` 返回合理值

# Task Dependencies

- Task 2.1/2.2/2.3 depend on Task 1.1（后端实现依赖抽象接口）
- Task 3.1 depends on Task 1.1（GPUChannel 需要 GPUTimestampQuery 接口）
- Task 4.1/4.2 depend on Task 3.1（Capture 扩展依赖 GPUChannel 定义）
- Task 5.1 depends on Task 3.1（RenderingDevice 桥接依赖 GPUChannel）
- Task 6.1 depends on Task 3.1（Manager 集成依赖 GPUChannel）
- Task 7.1 is independent（可与 Task 3.x 并行）
- Task 8.x depends on all other tasks
- Task 9.x depends on Task 8.x（测试在编译通过后执行）
- Task 2.1/2.2/2.3 are independent of each other（可并行）
