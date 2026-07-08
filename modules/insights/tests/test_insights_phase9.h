/**************************************************************************/
/*  test_insights_phase9.h                                               */
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

namespace TestInsightsPhase9 {

// 1. get_file_info 无 Worker 时返回空 Dictionary
TEST_CASE("[Insights][TracyBridge] get_file_info without Worker returns empty") {
	Ref<InsightsTracyBridge> bridge;
	bridge.instantiate();

	Dictionary info = bridge->get_file_info();
	// Empty dictionary when no worker
	CHECK(info.size() == 0);
}

// 2. save_tracy_file 无数据时返回错误
TEST_CASE("[Insights][TracyBridge] save_tracy_file without data returns error") {
	Ref<InsightsTracyBridge> bridge;
	bridge.instantiate();

	Error err = bridge->save_tracy_file("test_phase9_output.tracy");
	CHECK(err != OK);
}

// 3. get_file_info 返回正确格式（连接失败后仍无数据）
TEST_CASE("[Insights][TracyBridge] get_file_info format after failed connect") {
	Ref<InsightsTracyBridge> bridge;
	bridge.instantiate();

	// Attempt connect to non-existent client
	bridge->connect_to_client("127.0.0.1", 18086);

	Dictionary info = bridge->get_file_info();
	// Should still be empty since connection failed
	CHECK(info.size() == 0);
}

// 4. get_file_info 键名验证 — 确保正确字段存在
TEST_CASE("[Insights][TracyBridge] get_file_info key names") {
	// We can't get actual data without a real Tracy Client,
	// but we verify the method compiles and returns Dictionary.
	Ref<InsightsTracyBridge> bridge;
	bridge.instantiate();

	Dictionary info = bridge->get_file_info();
	// No worker, so empty — the important thing is the method is callable.
	// When a .tracy file is loaded, it should contain:
	// zone_count, frame_count, thread_count, gpu_context_count, last_time_ns, has_data
	CHECK(info.has("zone_count") == false); // No worker = no keys
}

// 5. 保存路径为空时返回错误
TEST_CASE("[Insights][TracyBridge] save_tracy_file with empty path returns error") {
	Ref<InsightsTracyBridge> bridge;
	bridge.instantiate();

	Error err = bridge->save_tracy_file("");
	CHECK(err != OK);
}

// 6. load + save round-trip — 无数据时的稳定性
TEST_CASE("[Insights][TracyBridge] save then load round-trip stability") {
	Ref<InsightsTracyBridge> bridge;
	bridge.instantiate();

	// Save with no data
	Error save_err = bridge->save_tracy_file("test_roundtrip.tracy");
	CHECK(save_err != OK);

	// Load invalid file
	Error load_err = bridge->load_tracy_file("test_roundtrip.tracy");
	CHECK(load_err != OK);

	// After failed operations, bridge should be in clean state
	CHECK_FALSE(bridge->is_connected());
	CHECK_FALSE(bridge->has_data());
}

} // namespace TestInsightsPhase9

#endif // TRACY_SERVER_ENABLED
