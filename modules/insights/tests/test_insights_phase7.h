/**************************************************************************/
/*  test_insights_phase7.h                                               */
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

#ifdef TRACY_SERVER_ENABLED

#include "modules/insights/insights_tracy_bridge.h"

namespace TestInsightsPhase7 {

// 1. 初始状态 — 新实例不应连接也不应有数据
TEST_CASE("[Insights][TracyBridge] Initial state") {
	Ref<InsightsTracyBridge> bridge;
	bridge.instantiate();
	CHECK(bridge.is_valid());

	CHECK_FALSE(bridge->is_connected());
	CHECK_FALSE(bridge->has_data());
	CHECK(bridge->get_thread_list().size() == 0);
	CHECK(bridge->get_frame_sets().size() == 0);
	CHECK(bridge->get_gpu_context_list().size() == 0);
	CHECK(bridge->get_memory_stats().size() == 0);
	CHECK(bridge->get_messages().size() == 0);
	CHECK(bridge->get_plots().size() == 0);
	CHECK(bridge->get_last_time() == 0);
	CHECK(bridge->get_frame_count() == 0);
}

// 2. 连接失败 — 无 Tracy Client 运行时应返回错误
TEST_CASE("[Insights][TracyBridge] Connect to non-existent client fails") {
	Ref<InsightsTracyBridge> bridge;
	bridge.instantiate();

	// Connecting to a port with no Tracy client should fail.
	Error err = bridge->connect_to_client("127.0.0.1", 18086);
	CHECK(err != OK);
	CHECK_FALSE(bridge->is_connected());
}

// 3. 加载无效文件 — 应返回文件错误
TEST_CASE("[Insights][TracyBridge] Load invalid file returns error") {
	Ref<InsightsTracyBridge> bridge;
	bridge.instantiate();

	Error err = bridge->load_tracy_file("nonexistent_file.tracy");
	CHECK(err != OK);
	CHECK_FALSE(bridge->has_data());
}

// 4. 保存无数据 — 没有 Worker 数据时应返回错误
TEST_CASE("[Insights][TracyBridge] Save without data returns error") {
	Ref<InsightsTracyBridge> bridge;
	bridge.instantiate();

	Error err = bridge->save_tracy_file("test_output.tracy");
	CHECK(err != OK);
}

// 5. 断开连接 — disconnect 后状态应重置
TEST_CASE("[Insights][TracyBridge] Disconnect resets state") {
	Ref<InsightsTracyBridge> bridge;
	bridge.instantiate();

	// Even after a failed connect, disconnect should work cleanly.
	bridge->connect_to_client("127.0.0.1", 18086);
	bridge->disconnect();

	CHECK_FALSE(bridge->is_connected());
	CHECK_FALSE(bridge->has_data());
}

// 6. 查询 API 空数据 — 所有查询方法应返回空容器
TEST_CASE("[Insights][TracyBridge] Query APIs return empty without data") {
	Ref<InsightsTracyBridge> bridge;
	bridge.instantiate();

	Array threads = bridge->get_thread_list();
	CHECK(threads.size() == 0);

	Array frames = bridge->get_frame_sets();
	CHECK(frames.size() == 0);

	Array gpu = bridge->get_gpu_context_list();
	CHECK(gpu.size() == 0);

	Dictionary mem = bridge->get_memory_stats();
	CHECK(mem.size() == 0);

	Array msgs = bridge->get_messages();
	CHECK(msgs.size() == 0);

	Array plots = bridge->get_plots();
	CHECK(plots.size() == 0);

	Array zones = bridge->get_thread_zones(0);
	CHECK(zones.size() == 0);

	Array gpu_zones = bridge->get_gpu_zones(0);
	CHECK(gpu_zones.size() == 0);
}

// 7. 查询 API 时间范围过滤 — 无数据时带过滤参数也应返回空
TEST_CASE("[Insights][TracyBridge] Query APIs with time range filter return empty") {
	Ref<InsightsTracyBridge> bridge;
	bridge.instantiate();

	Array zones = bridge->get_thread_zones(0, 0, 1000000);
	CHECK(zones.size() == 0);

	Array gpu_zones = bridge->get_gpu_zones(0, 0, 1000000);
	CHECK(gpu_zones.size() == 0);

	Array msgs = bridge->get_messages(0, 1000000);
	CHECK(msgs.size() == 0);
}

// 8. 生命周期 — 多次 connect/disconnect 不应崩溃
TEST_CASE("[Insights][TracyBridge] Multiple connect/disconnect cycles") {
	Ref<InsightsTracyBridge> bridge;
	bridge.instantiate();

	for (int i = 0; i < 3; i++) {
		bridge->connect_to_client("127.0.0.1", 18086);
		bridge->disconnect();
	}

	CHECK_FALSE(bridge->is_connected());
	CHECK_FALSE(bridge->has_data());
}

} // namespace TestInsightsPhase7

#endif // TRACY_SERVER_ENABLED
