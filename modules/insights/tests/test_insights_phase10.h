/**************************************************************************/
/*  test_insights_phase10.h                                              */
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
#include "modules/insights/insights_core/insights_database.h"

namespace TestInsightsPhase10 {

// 1. get_lock_events 无 Worker 时返回空数组
TEST_CASE("[Insights][TracyBridge] get_lock_events without Worker returns empty array") {
	Ref<InsightsTracyBridge> bridge;
	bridge.instantiate();

	Array result = bridge->get_lock_events();
	CHECK(result.size() == 0);
}

// 2. get_callstack 无 Worker 时返回空数组
TEST_CASE("[Insights][TracyBridge] get_callstack without Worker returns empty array") {
	Ref<InsightsTracyBridge> bridge;
	bridge.instantiate();

	Array result = bridge->get_callstack(0);
	CHECK(result.size() == 0);
}

// 3. InsightsDatabase insert_plot_point 和查询
TEST_CASE("[Insights][TracyBridge] InsightsDatabase insert_plot_point and query") {
	Ref<InsightsDatabase> db;
	db.instantiate();
	db->open("test_plot_points");

	db->insert_plot_point("fps", 1000, 60.0);
	db->insert_plot_point("fps", 2000, 55.5);
	db->insert_plot_point("fps", 3000, 62.3);

	CHECK(db->get_plot_point_count() == 3);

	Array names = db->get_plot_names();
	CHECK(names.has("fps"));

	Array result = db->query_plot_points("fps", 0, UINT64_MAX);
	CHECK(result.size() == 3);

	db->close();
}

// 4. InsightsDatabase insert_lock_event 和查询
TEST_CASE("[Insights][TracyBridge] InsightsDatabase insert_lock_event and query") {
	Ref<InsightsDatabase> db;
	db.instantiate();
	db->open("test_lock_events");

	db->insert_lock_event(1, 10, 1000, 0, 100); // wait
	db->insert_lock_event(1, 10, 2000, 1, 100); // acquire

	CHECK(db->get_lock_event_count() == 2);

	Array result = db->query_lock_events(0, UINT64_MAX);
	CHECK(result.size() == 2);

	db->close();
}

// 5. InsightsDatabase insert_message 和查询
TEST_CASE("[Insights][TracyBridge] InsightsDatabase insert_message and query") {
	Ref<InsightsDatabase> db;
	db.instantiate();
	db->open("test_messages");

	db->insert_message(0, "Hello", 1000, -1);
	db->insert_message(1, "Warning", 2000, -1);
	db->insert_message(2, "Error", 3000, -1);

	CHECK(db->get_message_count() == 3);

	Array result = db->query_messages();
	CHECK(result.size() == 3);

	db->close();
}

// 6. InsightsDatabase GPU zone count
TEST_CASE("[Insights][TracyBridge] InsightsDatabase GPU zone count") {
	Ref<InsightsDatabase> db;
	db.instantiate();
	db->open("test_gpu_zones");

	db->insert_gpu_zone("GPU::Draw", 0, 5000, 6000, 7000, 0);
	db->insert_gpu_zone("GPU::Compute", 1, 8000, 9000, 10000, 1);

	CHECK(db->get_gpu_zone_count() == 2);

	db->close();
}

// 7. InsightsDatabase save/load round-trip with new data types
TEST_CASE("[Insights][TracyBridge] InsightsDatabase save/load round-trip with new data types") {
	Ref<InsightsDatabase> db;
	db.instantiate();
	db->open("test_roundtrip");

	// Insert all data types
	db->insert_zone("Update", "main.cpp", 42, "update()", "", 1, 1000, 2000, 0, -1);
	db->insert_plot_point("fps", 1500, 60.0);
	db->insert_lock_event(1, 10, 1200, 0, 100);
	db->insert_message(0, "Frame start", 1000, -1);
	db->insert_gpu_zone("GPU::Draw", 0, 5000, 6000, 7000, 0);

	// Save
	Error save_err = db->save_to_file("test_phase10_roundtrip.db");
	CHECK(save_err == OK);

	// Load into new db
	Ref<InsightsDatabase> db2;
	db2.instantiate();
	db2->open("test_roundtrip_load");

	Error load_err = db2->load_from_file("test_phase10_roundtrip.db");
	CHECK(load_err == OK);

	// Verify all counts match
	CHECK(db2->get_zone_count() == db->get_zone_count());
	CHECK(db2->get_plot_point_count() == db->get_plot_point_count());
	CHECK(db2->get_lock_event_count() == db->get_lock_event_count());
	CHECK(db2->get_message_count() == db->get_message_count());
	CHECK(db2->get_gpu_zone_count() == db->get_gpu_zone_count());

	db->close();
	db2->close();
}

// 8. populate_database 无 Worker 时所有新数据类型为空
TEST_CASE("[Insights][TracyBridge] populate_database without Worker leaves new data types empty") {
	Ref<InsightsTracyBridge> bridge;
	bridge.instantiate();

	Ref<InsightsDatabase> db;
	db.instantiate();
	db->open("test_populate_new_types");

	// populate_database will fail without Worker
	bridge->populate_database(db);

	CHECK(db->get_message_count() == 0);
	CHECK(db->get_plot_point_count() == 0);
	CHECK(db->get_lock_event_count() == 0);
	CHECK(db->get_gpu_zone_count() == 0);

	db->close();
}

} // namespace TestInsightsPhase10

#endif // TRACY_SERVER_ENABLED
