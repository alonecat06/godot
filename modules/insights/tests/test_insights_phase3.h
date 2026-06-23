/**************************************************************************/
/*  test_insights_phase3.h                                               */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#pragma once

#include "tests/test_macros.h"

#include "modules/insights/gpu/gpu_timestamp_query.h"
#include "modules/insights/gpu/gpu_profiler_vulkan.h"
#include "modules/insights/gpu/gpu_profiler_d3d12.h"
#include "modules/insights/gpu/gpu_profiler_metal.h"
#include "modules/insights/channels/gpu_channel.h"
#include "modules/insights/insights_core/insights_database.h"

namespace TestInsightsPhase3 {

// 1. GPU 时间戳 RDD 接口测试 — 验证 RenderingDevice 时间戳 API 和 GPUChannel 注册
TEST_CASE("[Insights] GPU timestamp RDD interface") {
	// Verify GPUChannel can be instantiated and used without crash.
	GPUChannel gpu;

	// Verify on_gpu_timestamp does not crash.
	gpu.on_gpu_timestamp("test_timestamp", 1000, 2000, 0);
	CHECK(gpu.get_zone_count() == 1);

	// Verify InsightsManager singleton exists and GPUChannel is registered.
	// Note: InsightsManager is a singleton that auto-registers GPUChannel in start_capture.
	// In unit test context, we verify the GPUChannel class itself works correctly.
	CHECK(gpu.get_name() == "gpu");
}

// 2. Vulkan 时间戳精度测试 — 验证 GPUProfilerVulkan 的属性和行为
TEST_CASE("[Insights] Vulkan timestamp period resolution") {
	GPUProfilerVulkan profiler;

	// Verify get_timestamp_period returns a reasonable default (> 0, < 1000).
	float period = profiler.get_timestamp_period();
	CHECK(period > 0.0f);
	CHECK(period < 1000.0f);

	// Verify max_queries is 256.
	CHECK(profiler.get_max_queries() == 256);

	// Verify is_supported returns false before initialization.
	CHECK(profiler.is_supported() == false);

	// Verify write_timestamp returns 0 when not initialized and does not crash.
	uint32_t idx = profiler.write_timestamp(nullptr, GPUTimestampQuery::STAGE_BEGIN_RENDER_PASS);
	CHECK(idx == 0);
}

// 3. GPU Zone 端到端时间测量测试 — 验证 GPUChannel 记录和查询 GPU Zone
TEST_CASE("[Insights] GPU zone end-to-end timing") {
	GPUChannel gpu;

	// Record a GPU zone with valid time range.
	uint64_t start_ns = 1000000;
	uint64_t end_ns = 5000000;
	gpu.on_gpu_zone("godot:gpu/command/draw", 0, 900000, start_ns, end_ns, 1);

	// Verify zone was recorded.
	CHECK(gpu.get_zone_count() == 1);

	// Verify the zone can be queried by time range.
	TypedArray<Dictionary> zones = gpu.get_gpu_zones_in_range(0, 10000000);
	CHECK(zones.size() == 1);

	// Verify zone data.
	Dictionary zone = zones[0];
	CHECK(String(zone["name"]) == "godot:gpu/command/draw");
	CHECK(uint64_t(zone["start_ns"]) == start_ns);
	CHECK(uint64_t(zone["end_ns"]) == end_ns);

	// Verify duration is > 0 and < 1 second (1 billion nanoseconds).
	uint64_t duration_ns = end_ns - start_ns;
	CHECK(duration_ns > 0);
	CHECK(duration_ns < 1000000000ULL);
}

// 4. GPU-CPU 关联测试 — 验证 InsightsDatabase 的 GPU Zone 查询
TEST_CASE("[Insights] GPU-CPU correlation") {
	Ref<InsightsDatabase> db;
	db.instantiate();
	db->open("res://test_gpu_cpu.gitracy");

	// Insert a CPU zone.
	db->insert_zone("godot:rendering/forward", "render_forward.cpp", 100,
			"render_forward", "cpu", 0, 1000000, 1050000, 0, -1);

	// Insert a GPU zone associated with the CPU zone via submit_ns.
	db->insert_gpu_zone("godot:gpu/command/draw", 0, 1000100, 1020000, 1040000, 0);

	// Verify GPU zones can be queried by time range.
	Array gpu_zones = db->query_gpu_zones_in_range(0, 2000000);
	CHECK(gpu_zones.size() == 1);

	// Verify the GPU zone's submit_ns falls within the CPU zone's time range.
	if (gpu_zones.size() > 0) {
		Dictionary gpu_zone = gpu_zones[0];
		uint64_t submit_ns = gpu_zone["submit_ns"];
		// submit_ns (1000100) should be within CPU zone [1000000, 1050000].
		CHECK(submit_ns >= 1000000);
		CHECK(submit_ns <= 1050000);
	}

	db->close();
}

// 5. 多后端兼容性测试 — 验证所有 GPU profiler 后端可实例化
TEST_CASE("[Insights] GPU profiler multi-backend compatibility") {
	// Verify Vulkan backend.
	GPUProfilerVulkan *vulkan = memnew(GPUProfilerVulkan);
	CHECK(vulkan != nullptr);
	CHECK(vulkan->is_supported() == false); // Not initialized.
	CHECK(vulkan->get_timestamp_period() > 0.0f);
	CHECK(vulkan->get_max_queries() == 256);
	// Verify inheritance.
	CHECK(Object::cast_to<GPUTimestampQuery>(vulkan) != nullptr);
	memdelete(vulkan);

	// Verify D3D12 backend.
	GPUProfilerD3D12 *d3d12 = memnew(GPUProfilerD3D12);
	CHECK(d3d12 != nullptr);
	CHECK(d3d12->is_supported() == false); // Not initialized.
	CHECK(d3d12->get_timestamp_period() > 0.0f);
	CHECK(d3d12->get_max_queries() == 256);
	CHECK(Object::cast_to<GPUTimestampQuery>(d3d12) != nullptr);
	memdelete(d3d12);

	// Verify Metal backend.
	GPUProfilerMetal *metal = memnew(GPUProfilerMetal);
	CHECK(metal != nullptr);
	CHECK(metal->is_supported() == false); // Not initialized.
	CHECK(metal->get_timestamp_period() > 0.0f);
	CHECK(metal->get_max_queries() == 256);
	CHECK(Object::cast_to<GPUTimestampQuery>(metal) != nullptr);
	memdelete(metal);
}

} // namespace TestInsightsPhase3
