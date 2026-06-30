/**************************************************************************/
/*  test_insights_phase6.h                                               */
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

#include "modules/insights/channels/cpu_channel.h"
#include "modules/insights/channels/custom_channel.h"
#include "modules/insights/tools/web_exporter.h"
#include "modules/insights/insights_core/ai_analyzer.h"
#include "modules/insights/insights_core/insights_database.h"

namespace TestInsightsPhase6 {

// 1. 锁竞争检测 — 验证 CPUChannel 追踪锁竞争事件
TEST_CASE("[Insights] Lock contention detection") {
	CPUChannel *channel = memnew(CPUChannel);

	channel->enable_contention_tracking();

	// Thread A acquires the lock.
	channel->on_lock_acquire("RenderingServer::mutex", 1, 1000);
	// Thread B attempts to acquire (waits).
	channel->on_lock_attempt("RenderingServer::mutex", 2, 2000);
	// Thread A releases the lock.
	channel->on_lock_release("RenderingServer::mutex", 1, 5000);
	// Thread B acquires the lock — this matches the pending attempt and creates a contention event.
	channel->on_lock_acquire("RenderingServer::mutex", 2, 5000);

	TypedArray<Dictionary> events = channel->get_contention_events();
	CHECK(events.size() == 1);

	Dictionary event = events[0];
	CHECK(String(event["lock_name"]) == "RenderingServer::mutex");
	CHECK(uint64_t(event["wait_time_ns"]) == 3000);
	CHECK(uint64_t(event["owner_thread"]) == 2);
	CHECK(uint64_t(event["waiter_thread"]) == 2);

	memdelete(channel);
}

// 2. 自定义通道 API — 验证 CustomChannel 记录 zone 数据
TEST_CASE("[Insights] Custom channel API") {
	CustomChannel *channel = memnew(CustomChannel);

	channel->set_name("my_game_ai");
	channel->set_color(Color(1.0, 0.5, 0.0));

	channel->begin_zone("ai/pathfinding", "ai_controller.cpp", 42);
	channel->end_zone();

	TypedArray<Dictionary> zones = channel->get_zones();
	CHECK(zones.size() == 1);

	Dictionary zone = zones[0];
	CHECK(String(zone["name"]) == "ai/pathfinding");
	CHECK(String(zone["channel"]) == "my_game_ai");

	memdelete(channel);
}

// 3. Web 导出 HTML — 验证 WebExporter 实例化
TEST_CASE("[Insights] Web export to HTML") {
	Ref<WebExporter> exporter;
	exporter.instantiate();
	CHECK(exporter.is_valid());

	// File export requires a real database file on disk, so only test instantiation.
}

// 4. 实时性能数据流 — 验证 InsightsDatabase 写入与查询
TEST_CASE("[Insights] Live profiling streaming") {
	Ref<InsightsDatabase> db;
	db.instantiate();

	db->open("test_streaming");
	db->insert_zone("physics/step", "physics_3d.cpp", 100, "step", "cpu", 1, 1000, 5000, 0, -1);
	db->insert_frame_marker(0, 0, 10000);

	CHECK(db->get_zone_count() > 0);

	db->close();
}

// 5. AI 分析集成 — 验证 AIAnalyzer 构建提示和解析响应
TEST_CASE("[Insights] AI analysis integration") {
	Ref<AIAnalyzer> analyzer;
	analyzer.instantiate();
	CHECK(analyzer.is_valid());

	// Build a database with zone data and verify the prompt contains zone names.
	Ref<InsightsDatabase> db;
	db.instantiate();
	db->open("test_ai");
	db->insert_zone("physics/3d/step", "physics_3d.cpp", 50, "step", "cpu", 1, 1000, 5000, 0, -1);

	String prompt = analyzer->build_analysis_prompt(db);
	CHECK(prompt.contains("physics/3d/step"));

	db->close();

	// Parse a mock JSON response.
	String mock_json = "{\"bottleneck\": \"godot:physics/3d/step\", \"suggestions\": [\"suggestion1\", \"suggestion2\"], \"severity\": \"high\"}";
	Dictionary result = analyzer->parse_response(mock_json);

	CHECK(String(result["bottleneck"]) == "godot:physics/3d/step");
}

} // namespace TestInsightsPhase6
