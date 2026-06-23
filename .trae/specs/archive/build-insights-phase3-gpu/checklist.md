# Checklist — Phase 3：GPU Profiling

## GPUTimestampQuery 抽象接口

- [x] GPUTimestampQuery 抽象基类已创建
- [x] Stage 枚举包含 BEGIN_RENDER_PASS, END_RENDER_PASS, DRAW, DISPATCH, PIPELINE_BARRIER, COPY, PRESENT
- [x] write_timestamp / fetch_results 纯虚方法已声明
- [x] begin_frame / end_frame / is_supported / get_timestamp_period 虚方法已声明

## GPU 后端实现

- [x] GPUProfilerVulkan 已创建，继承 GPUTimestampQuery
- [x] GPUProfilerVulkan::write_timestamp() stub 实现，未初始化时返回 0
- [x] GPUProfilerVulkan::fetch_results() stub 实现
- [x] GPUProfilerVulkan::get_timestamp_period() 返回默认 timestamp_period
- [x] GPUProfilerD3D12 已创建，继承 GPUTimestampQuery
- [x] GPUProfilerD3D12::write_timestamp() stub 实现
- [x] GPUProfilerD3D12::fetch_results() stub 实现
- [x] GPUProfilerMetal 已创建，继承 GPUTimestampQuery
- [x] GPUProfilerMetal::fetch_results() stub 实现

## GPUChannel

- [x] GPUChannel 类已创建，继承 InsightsChannel，category=CHANNEL_CATEGORY_GPU
- [x] GPUChannel::on_gpu_timestamp() 正确记录单个 GPU 时间戳
- [x] GPUChannel::on_gpu_zone() 正确记录 GPU Zone
- [x] GPUChannel::collect_frame_timestamps() 从 RenderingDevice 读取所有时间戳
- [x] GPUChannel::get_gpu_zones_in_range() 支持按时间范围查询
- [x] GPUChannel::get_gpu_zones_for_cpu_zone() 支持 GPU-CPU 关联查询

## InsightsCapture/NativeCapture 扩展

- [x] InsightsCapture 新增 on_gpu_zone() 纯虚方法
- [x] NativeCapture::EventType 新增 GPU_ZONE
- [x] NativeCapture::TraceEvent 联合体新增 gpu_zone 数据结构
- [x] NativeCapture 实现 on_gpu_zone() 虚方法
- [x] NativeCapture::_process_events() 正确处理 GPU_ZONE 事件

## RenderingDevice GPU 桥接

- [x] RenderingDevice::_end_frame() 中有 MODULE_INSIGHTS_ENABLED 条件编译块
- [x] 录制期间调用 GPUChannel::collect_frame_timestamps()
- [x] 利用已有 get_captured_timestamps_count/name/gpu_time/cpu_time API
- [x] GPU-CPU 关联通过 cpu_time 作为 submit_ns 实现

## InsightsManager 集成

- [x] InsightsManager::start_capture() 自动创建并注册 GPUChannel
- [x] InsightsManager::stop_capture() 自动注销并释放 GPUChannel
- [x] InsightsManager 提供 get_gpu_channel() 便捷方法

## InsightsDatabase GPU 查询

- [x] insert_gpu_zone() 已实现（Phase 1 遗留）
- [x] query_gpu_zones_for_cpu_zone() 按 CPU Zone 查询关联 GPU Zone
- [x] query_gpu_zones_in_range() 按时间范围查询 GPU Zone

## 注册与编译

- [x] register_types.cpp 已注册 GPUChannel、GPUTimestampQuery、GPUProfilerVulkan、GPUProfilerD3D12、GPUProfilerMetal
- [x] config.py 的 doc_classes 列表已添加新类名
- [x] SCsub 已添加 gpu/*.cpp 编译
- [x] `scons module_insights_enabled=yes platform=windows dev_build=yes` 编译通过

## 单元测试

- [x] 测试文件 `modules/insights/tests/test_insights_phase3.h` 已创建
- [x] 测试文件已通过 SCons 自动发现并包含在 `modules_tests.gen.h` 中
- [x] GPU 时间戳 RDD 接口测试通过 — 验证 GPUChannel::on_gpu_timestamp() 不崩溃
- [x] GPU 时间戳 RDD 接口测试通过 — 验证 GPUChannel 的 zone_count 正确
- [x] GPU 时间戳 RDD 接口测试通过 — 验证 GPUChannel 的 name 属性为 "gpu"
- [x] Vulkan 时间戳精度测试通过 — `get_timestamp_period()` 返回值 > 0 且 < 1000
- [x] Vulkan 时间戳精度测试通过 — `get_max_queries()` 返回 256
- [x] Vulkan 时间戳精度测试通过 — `is_supported()` 在未初始化时返回 false
- [x] GPU Zone 端到端时间测量测试通过 — GPUZoneRecord 的 `start_ns < end_ns`
- [x] GPU Zone 端到端时间测量测试通过 — GPU Zone 持续时间 > 0 且 < 1s（纳秒单位）
- [x] GPU Zone 端到端时间测量测试通过 — `get_gpu_zones_in_range()` 正确查询到记录
- [x] GPU-CPU 关联测试通过 — `query_gpu_zones_in_range()` 返回正确结果
- [x] GPU-CPU 关联测试通过 — GPU Zone 的 `submit_ns` 在 CPU Zone 的时间范围内
- [x] 多后端兼容性测试通过 — GPUProfilerVulkan/D3D12/Metal 均可实例化且 `is_supported()` 可调用
- [x] 多后端兼容性测试通过 — 各后端继承自 GPUTimestampQuery 抽象基类
- [x] 多后端兼容性测试通过 — 各后端的 `get_timestamp_period()` 和 `get_max_queries()` 返回合理值
