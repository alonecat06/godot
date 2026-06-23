# Phase 3：GPU Profiling Spec

## Why

Phase 1 和 Phase 2 已完成 CPU Zone 扩桩、Memory/Log/Script/Loading Channel 实现和引擎 Hook 接入。但 GPU 端的性能数据尚未被捕获——当前 RenderingDevice 已有 `capture_timestamp` 基础设施（timestamp query pool + `get_captured_timestamp_gpu_time`），却未与 Insights 系统对接。Phase 3 需要实现 GPUChannel、GPU 时间戳桥接（Vulkan/D3D12/Metal）、GPU-CPU 关联，使录制系统能够捕获完整的 GPU 性能数据。

## What Changes

- **GPUChannel 实现**：继承 InsightsChannel，封装 GPU 时间戳查询和 GPU Zone 记录
- **GPUTimestampQuery 抽象**：定义 GPU 时间戳查询的统一接口（write_timestamp / fetch_results）
- **GPUProfilerVulkan 实现**：基于 `vkCmdWriteTimestamp` 的 Vulkan 后端
- **GPUProfilerD3D12 实现**：基于 `ID3D12GraphicsCommandList::EndQuery` 的 D3D12 后端
- **GPUProfilerMetal 实现**：基于 `MTLCommandBuffer.gpuStartTime/EndTime` 的 Metal 后端
- **RenderingDevice 桥接**：在 `RenderingDevice::_end_frame()` 中收集 GPU 时间戳并上报 Insights
- **InsightsCapture 接口扩展**：新增 `on_gpu_zone` 纯虚方法
- **NativeCapture 事件类型扩展**：新增 GPU_ZONE 事件类型
- **InsightsDatabase 扩展**：确保 GPUZoneRecord 的 insert/query 完整可用
- **GPU-CPU 关联**：通过 submit_ns 时间戳关联 CPU Zone 和 GPU Zone

## Impact

- Affected specs: Phase 2（InsightsCapture/NativeCapture 需要扩展）
- Affected code:
  - `modules/insights/gpu/gpu_timestamp_query.h` — 新增 GPUTimestampQuery 抽象基类（Stage 枚举、TimestampResult 结构体、纯虚方法）
  - `modules/insights/gpu/gpu_timestamp_query.cpp` — 实现 begin_frame/is_supported/get_timestamp_period 等虚方法
  - `modules/insights/gpu/gpu_profiler_vulkan.h` — 新增 GPUProfilerVulkan 类声明
  - `modules/insights/gpu/gpu_profiler_vulkan.cpp` — Vulkan 后端 stub 实现
  - `modules/insights/gpu/gpu_profiler_d3d12.h` — 新增 GPUProfilerD3D12 类声明
  - `modules/insights/gpu/gpu_profiler_d3d12.cpp` — D3D12 后端 stub 实现
  - `modules/insights/gpu/gpu_profiler_metal.h` — 新增 GPUProfilerMetal 类声明
  - `modules/insights/gpu/gpu_profiler_metal.cpp` — Metal 后端 stub 实现
  - `modules/insights/channels/gpu_channel.h` — 新增 GPUChannel 类声明（GPUZoneRecord、on_gpu_zone、collect_frame_timestamps）
  - `modules/insights/channels/gpu_channel.cpp` — GPUChannel 实现（时间戳收集、Zone 查询）
  - `modules/insights/insights_core/insights_capture.h` — 新增 on_gpu_zone 纯虚方法
  - `modules/insights/insights_core/insights_capture.cpp` — on_gpu_zone 默认实现
  - `modules/insights/insights_core/native_capture.h` — 新增 GPU_ZONE 事件类型和 gpu_zone 联合体成员
  - `modules/insights/insights_core/native_capture.cpp` — 实现 on_gpu_zone 和 GPU_ZONE 事件处理
  - `modules/insights/insights_core/insights_database.h` — 新增 query_gpu_zones_for_cpu_zone/query_gpu_zones_in_range 方法
  - `modules/insights/insights_core/insights_database.cpp` — 实现 GPU Zone 查询方法
  - `modules/insights/insights_core/insights_manager.h` — 新增 gpu_channel 成员和 get_gpu_channel() 方法
  - `modules/insights/insights_core/insights_manager.cpp` — GPUChannel 自动注册/注销
  - `modules/insights/register_types.cpp` — 注册 GPUChannel、GPUTimestampQuery、GPUProfilerVulkan/D3D12/Metal
  - `modules/insights/config.py` — 新增 5 个 GPU 相关 doc class
  - `modules/insights/SCsub` — 新增 gpu/*.cpp 编译
  - `modules/insights/tests/test_insights_phase3.h` — 新增 Phase 3 单元测试（5 个 TEST_CASE）
  - `servers/rendering/rendering_device.cpp` — _end_frame 中添加 MODULE_INSIGHTS_ENABLED 条件编译块

## ADDED Requirements

### Requirement: GPUChannel

系统 SHALL 提供 GPUChannel 类（继承 InsightsChannel），在录制期间收集 GPU 时间戳查询结果，记录 GPU Zone 到 InsightsDatabase。

#### Scenario: GPU Zone 被记录
- **WHEN** Insights 正在录制且 GPUChannel 已启用
- **AND** RenderingDevice 完成一帧渲染并收集 GPU 时间戳
- **THEN** GPUChannel 记录 GPUZoneRecord（name, queue_id, submit_ns, start_ns, end_ns, context_id）

#### Scenario: GPU 时间戳被读取
- **WHEN** RenderingDevice 调用 `get_captured_timestamps_count()` 返回 N 个时间戳
- **THEN** GPUChannel 通过 `get_captured_timestamp_name/gpu_time/cpu_time` 读取每个时间戳并记录

### Requirement: GPUTimestampQuery 抽象

系统 SHALL 提供 GPUTimestampQuery 抽象基类，定义 GPU 时间戳查询的统一接口。

#### Scenario: 抽象接口定义
- **WHEN** 系统需要执行 GPU 时间戳查询
- **THEN** GPUTimestampQuery 提供 `write_timestamp(cmd_buffer, stage)` 和 `fetch_results(results, count)` 纯虚方法

#### Scenario: Stage 枚举
- **WHEN** 系统需要标记 GPU 时间戳的阶段
- **THEN** GPUTimestampQuery::Stage 枚举包含 BEGIN_RENDER_PASS、END_RENDER_PASS、DRAW、DISPATCH、PIPELINE_BARRIER、COPY、PRESENT

### Requirement: GPUProfilerVulkan

系统 SHALL 提供 GPUProfilerVulkan 类（继承 GPUTimestampQuery），基于 Vulkan timestamp query pool 实现 GPU 时间戳查询。

#### Scenario: Vulkan 时间戳查询池创建
- **WHEN** GPUProfilerVulkan 初始化
- **THEN** 创建 VkQueryPool，最大查询数 256

#### Scenario: Vulkan 时间戳写入
- **WHEN** 调用 `write_timestamp(cmd_buffer, stage)`
- **THEN** 执行 `vkCmdWriteTimestamp(cmd, map_stage(stage), pool, query_index)`，返回 query_index

#### Scenario: Vulkan 时间戳读回
- **WHEN** 帧结束调用 `fetch_results(results, count)`
- **THEN** 通过 `vkGetQueryPoolResults` 读回时间戳，乘以 `timestampPeriod` 转换为纳秒

### Requirement: GPUProfilerD3D12

系统 SHALL 提供 GPUProfilerD3D12 类（继承 GPUTimestampQuery），基于 D3D12 timestamp query 实现 GPU 时间戳查询。

#### Scenario: D3D12 时间戳查询
- **WHEN** 调用 `write_timestamp(cmd_buffer, stage)`
- **THEN** 执行 `ID3D12GraphicsCommandList::EndQuery(heap, D3D12_QUERY_TYPE_TIMESTAMP, index)`

#### Scenario: D3D12 时间戳读回
- **WHEN** 帧结束调用 `fetch_results(results, count)`
- **THEN** 通过 `ID3D12CommandQueue::GetTimestampFrequency` 转换为纳秒

### Requirement: GPUProfilerMetal

系统 SHALL 提供 GPUProfilerMetal 类（继承 GPUTimestampQuery），基于 Metal 命令缓冲区时间实现 GPU 时间戳查询。

#### Scenario: Metal 时间戳
- **WHEN** 命令缓冲区完成执行
- **THEN** 通过 `MTLCommandBuffer.gpuStartTime/gpuEndTime` 获取 GPU 时间

### Requirement: RenderingDevice GPU 桥接

系统 SHALL 在 RenderingDevice 帧结束回调中，将已收集的 GPU 时间戳数据上报到 Insights 系统。

#### Scenario: 帧结束 GPU 数据收集
- **WHEN** RenderingDevice::_end_frame() 执行
- **AND** Insights 正在录制且 GPUChannel 已启用
- **THEN** 遍历 `get_captured_timestamps_count()` 个时间戳，对每个时间戳调用 GPUChannel::on_gpu_timestamp()

#### Scenario: GPU-CPU 关联
- **WHEN** GPU 时间戳被记录
- **THEN** 同时记录 `get_captured_timestamp_cpu_time()` 作为 submit_ns，实现 GPU Zone 与 CPU Zone 的关联

### Requirement: InsightsCapture GPU 接口扩展

系统 SHALL 在 InsightsCapture 抽象基类中新增 `on_gpu_zone` 纯虚方法，NativeCapture 实现该方法。

#### Scenario: GPU Zone 事件入队
- **WHEN** GPUChannel 记录一个 GPU Zone
- **THEN** NativeCapture 将其作为 GPU_ZONE 类型事件入队到双缓冲区

### Requirement: NativeCapture GPU 事件扩展

系统 SHALL 在 NativeCapture::EventType 枚举中新增 GPU_ZONE 事件类型，并在 TraceEvent 联合体中添加 gpu_zone 数据结构。

#### Scenario: GPU Zone 事件处理
- **WHEN** 消费者线程处理 GPU_ZONE 事件
- **THEN** 调用 InsightsDatabase::insert_gpu_zone() 写入 GPU Zone 记录

### Requirement: Phase 3 单元测试

系统 SHALL 提供 5 个单元测试（`tests/test_insights_phase3.h`），覆盖 GPU Profiling 的核心功能，使用 doctest 框架的 `TEST_CASE` 宏。

#### Scenario: GPU 时间戳 RDD 接口测试
- **WHEN** RenderingDevice 已初始化且 Insights 模块已启用
- **THEN** 验证 `RenderingDevice` 的 `get_captured_timestamps_count()` 接口存在且可调用
- **AND** 验证写入 GPU 时间戳不会崩溃
- **AND** 验证 GPUChannel 已在 InsightsManager 中注册

#### Scenario: Vulkan 时间戳精度测试
- **WHEN** GPUProfilerVulkan 实例被创建
- **THEN** 验证 `get_timestamp_period()` 返回值 > 0 且 < 1000
- **AND** 验证 `get_max_queries()` 返回 256
- **AND** 验证 `is_supported()` 在未初始化时返回 false

#### Scenario: GPU Zone 端到端时间测量测试
- **WHEN** GPUChannel 记录 GPU Zone 数据
- **THEN** 验证 GPUZoneRecord 的 `start_ns < end_ns`
- **AND** 验证 GPU Zone 持续时间 > 0 且 < 1s（纳秒单位）
- **AND** 验证 `get_gpu_zones_in_range()` 能正确查询到记录

#### Scenario: GPU-CPU 关联测试
- **WHEN** InsightsDatabase 中同时存在 CPU Zone 和 GPU Zone 记录
- **THEN** 验证 `query_gpu_zones_for_cpu_zone()` 返回关联的 GPU Zone
- **AND** 验证 GPU Zone 的 `submit_ns` 在 CPU Zone 的时间范围内
- **AND** 验证 `query_gpu_zones_in_range()` 返回正确结果

#### Scenario: 多后端兼容性测试
- **WHEN** 系统根据当前平台创建对应的 GPU Profiler 后端
- **THEN** 验证 GPUProfilerVulkan、GPUProfilerD3D12、GPUProfilerMetal 均可实例化
- **AND** 验证各后端的 `is_supported()` 方法可调用且不崩溃
- **AND** 验证各后端继承自 GPUTimestampQuery 抽象基类

## MODIFIED Requirements

### Requirement: InsightsManager Channel 注册

Phase 2 已实现 InsightsManager 自动注册 MemoryChannel/LogChannel/ScriptChannel/LoadingChannel。Phase 3 中 InsightsManager SHALL 在 start_capture 时同时自动注册 GPUChannel，在 stop_capture 时自动注销。

### Requirement: InsightsDatabase GPUZoneRecord

Phase 1 已定义 GPUZoneRecord 结构体和 insert_gpu_zone 方法。Phase 3 SHALL 确保 query_gpu_zones_for_cpu_zone 方法完整可用，支持按 CPU Zone 查询关联的 GPU Zone。

## REMOVED Requirements

无。
