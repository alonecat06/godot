/**************************************************************************/
/*  test_insights_phase4.h                                               */
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

#include "modules/insights/insights_core/insights_database.h"
#include "modules/insights/insights_core/insights_comparator.h"
#include "modules/insights/insights_core/insights_replay.h"

#ifdef TOOLS_ENABLED
#include "modules/insights/editor/insights_editor_plugin.h"
#include "modules/insights/editor/insights_dock.h"
#include "modules/insights/editor/insights_timeline.h"
#include "modules/insights/editor/insights_flamegraph.h"
#include "modules/insights/editor/insights_memory_panel.h"
#include "modules/insights/editor/insights_loading_panel.h"
#include "modules/insights/editor/insights_compare_panel.h"
#endif

namespace TestInsightsPhase4 {

// 1. EditorPlugin 注册与生命周期测试
TEST_CASE("[Insights] EditorPlugin registration") {
#ifdef TOOLS_ENABLED
	InsightsEditorPlugin *plugin = memnew(InsightsEditorPlugin);
	CHECK(plugin != nullptr);
	String plugin_name = plugin->get_plugin_name();
	CHECK(plugin_name == "Insights");
	CHECK(plugin->has_main_screen() == true);

	InsightsDock *dock = plugin->get_bottom_dock();
	CHECK(dock != nullptr);

	memdelete(plugin);
#else
	// Editor plugin is only available in tools builds.
	CHECK(true);
#endif
}

// 2. Timeline 渲染测试 — 验证时间轴正确渲染 zone 数据
TEST_CASE("[Insights] Timeline rendering with zone data") {
#ifdef TOOLS_ENABLED
	Ref<InsightsDatabase> db;
	db.instantiate();
	db->open("res://test_timeline.gitracy");

	db->insert_zone("godot:physics/3d/step", "step.cpp", 20, "cpu", "cpu", 0, 1000, 5000, 0, -1);
	db->insert_zone("godot:rendering/forward", "forward.cpp", 30, "cpu", "cpu", 0, 5000, 12000, 0, -1);
	db->insert_frame_marker(0, 0, 16666666);

	InsightsTimeline timeline;
	timeline.set_database(db);

	CHECK(timeline.get_total_duration_ns() > 0);

	Rect2 visible = timeline.get_visible_range();
	CHECK(visible.size.x > 0);
	CHECK(visible.size.y > 0);

	db->close();
#else
	CHECK(true);
#endif
}

// 3. Flamegraph 测试 — 验证火焰图正确构建层级
TEST_CASE("[Insights] Flamegraph hierarchy construction") {
#ifdef TOOLS_ENABLED
	Ref<InsightsDatabase> db;
	db.instantiate();
	db->open("res://test_flame.gitracy");

	// Build nested zones.
	db->insert_zone("godot:main/iteration", "main.cpp", 10, "cpu", "cpu", 0, 0, 16000, 0, -1);
	db->insert_zone("godot:physics/3d/step", "step.cpp", 20, "cpu", "cpu", 0, 1000, 8000, 1, 0);
	db->insert_zone("godot:physics/3d/broadphase", "bp.cpp", 30, "cpu", "cpu", 0, 2000, 5000, 2, 1);
	db->insert_zone("godot:rendering/forward", "fwd.cpp", 40, "cpu", "cpu", 0, 8000, 15000, 1, 0);

	InsightsFlamegraph flame;
	flame.set_database(db);

	Array roots = flame.get_root_nodes();
	CHECK(roots.size() >= 1);

	db->close();
#else
	CHECK(true);
#endif
}

// 4. Memory Panel 测试 — 验证内存瀑布图正确显示分配生命周期
TEST_CASE("[Insights] Memory panel allocation lifecycle") {
#ifdef TOOLS_ENABLED
	Ref<InsightsDatabase> db;
	db.instantiate();
	db->open("res://test_memory.gitracy");

	// Simulate allocations: freed, leaked, freed.
	db->insert_allocation(0x1000, 1024, -1, 1000, 5000, 0); // freed
	db->insert_allocation(0x2000, 2048, -1, 2000, 0, 0);   // leaked (free_ns == 0)
	db->insert_allocation(0x3000, 512, -1, 3000, 8000, 0);  // freed

	InsightsMemoryPanel panel;
	panel.set_database(db);

	// Verify peak memory.
	CHECK(panel.get_peak_memory() > 0);

	// Verify leak list.
	Array leaks = panel.get_leaked_allocations();
	CHECK(leaks.size() == 1);

	// Verify size filter.
	panel.set_size_filter(1024);
	Array filtered = panel.get_filtered_allocations();
	CHECK(filtered.size() >= 1);

	db->close();
#else
	CHECK(true);
#endif
}

// 5. Loading Panel 测试 — 验证依赖树正确构建
TEST_CASE("[Insights] Loading panel dependency tree") {
#ifdef TOOLS_ENABLED
	Ref<InsightsDatabase> db;
	db.instantiate();
	db->open("res://test_loading.gitracy");

	db->insert_resource_load("res://scene.tscn", "SceneLoader", 1000, 5000, 4096, "", 0);
	db->insert_resource_load("res://player.tres", "TextLoader", 1500, 4000, 2048, "res://scene.tscn", 0);
	db->insert_resource_load("res://tex.png", "TextureLoader", 2000, 3500, 102400, "res://player.tres", 0);

	InsightsLoadingPanel panel;
	panel.set_database(db);

	// Verify root loads.
	Array roots = panel.get_root_loads();
	CHECK(roots.size() == 1);

	// Verify dependencies.
	Array deps = panel.get_dependencies("res://scene.tscn");
	CHECK(deps.size() == 1);

	// Verify bottleneck detection.
	Dictionary bottleneck = panel.get_bottleneck();
	CHECK(bottleneck.has("path"));

	db->close();
#else
	CHECK(true);
#endif
}

// 6. Compare Panel 测试 — 验证 Diff 正确识别回归
TEST_CASE("[Insights] Compare panel diff and regression detection") {
	// Create two trace databases.
	Ref<InsightsDatabase> db_baseline;
	Ref<InsightsDatabase> db_current;
	db_baseline.instantiate();
	db_current.instantiate();
	db_baseline->open("res://baseline.gitracy");
	db_current->open("res://current.gitracy");

	// baseline: physics step takes 2ms.
	db_baseline->insert_zone("godot:physics/3d/step", "step.cpp", 20, "cpu", "cpu", 0, 0, 2000000, 0, -1);
	// current: physics step takes 5ms (regression).
	db_current->insert_zone("godot:physics/3d/step", "step.cpp", 20, "cpu", "cpu", 0, 0, 5000000, 0, -1);
	// current: new zone.
	db_current->insert_zone("godot:physics/3d/narrowphase", "np.cpp", 30, "cpu", "cpu", 0, 1000000, 4000000, 1, -1);

	Ref<InsightsComparator> comparator;
	comparator.instantiate();

	Dictionary diff = comparator->compute_diff(db_baseline, db_current);

	// Verify regression detection.
	Array regressions = diff["regressions"];
	CHECK(regressions.size() == 1);

	// Verify new zone detection.
	Array new_zones = diff["new_zones"];
	CHECK(new_zones.size() == 1);

	db_baseline->close();
	db_current->close();
}

} // namespace TestInsightsPhase4
