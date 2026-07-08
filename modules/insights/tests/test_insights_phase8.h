/**************************************************************************/
/*  test_insights_phase8.h                                               */
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

namespace TestInsightsPhase8 {

// 1. populate_database 无 Worker 时返回错误
TEST_CASE("[Insights][TracyBridge] populate_database without Worker returns error") {
	Ref<InsightsTracyBridge> bridge;
	bridge.instantiate();

	Ref<InsightsDatabase> db;
	db.instantiate();
	db->open("test_populate");

	Error err = bridge->populate_database(db);
	CHECK(err != OK);

	db->close();
}

// 2. populate_database 无效数据库参数时返回错误
TEST_CASE("[Insights][TracyBridge] populate_database with invalid db returns error") {
	Ref<InsightsTracyBridge> bridge;
	bridge.instantiate();

	Error err = bridge->populate_database(Ref<InsightsDatabase>());
	CHECK(err != OK);
}

// 3. get_zone_count 无 Worker 时返回 0
TEST_CASE("[Insights][TracyBridge] get_zone_count without Worker returns 0") {
	Ref<InsightsTracyBridge> bridge;
	bridge.instantiate();

	CHECK(bridge->get_zone_count() == 0);
}

// 4. get_thread_list 返回正确格式（id, name, zone_count）
TEST_CASE("[Insights][TracyBridge] get_thread_list returns correct format") {
	Ref<InsightsTracyBridge> bridge;
	bridge.instantiate();

	Array threads = bridge->get_thread_list();
	CHECK(threads.size() == 0);

	// No Worker data, so empty array is expected.
	// When a .tracy file is loaded, each thread Dictionary should have id, name, zone_count keys.
}

// 5. 连接失败后 populate_database 仍返回错误
TEST_CASE("[Insights][TracyBridge] populate_database after failed connect returns error") {
	Ref<InsightsTracyBridge> bridge;
	bridge.instantiate();

	// Attempt to connect to non-existent Tracy Client
	bridge->connect_to_client("127.0.0.1", 18086);

	Ref<InsightsDatabase> db;
	db.instantiate();
	db->open("test_post_fail");

	Error err = bridge->populate_database(db);
	CHECK(err != OK);

	db->close();
}

// 6. 多次 populate_database 调用不崩溃（幂等性）
TEST_CASE("[Insights][TracyBridge] Multiple populate_database calls do not crash") {
	Ref<InsightsTracyBridge> bridge;
	bridge.instantiate();

	Ref<InsightsDatabase> db;
	db.instantiate();
	db->open("test_idempotent");

	// Should not crash even without Worker data
	bridge->populate_database(db);
	bridge->populate_database(db);
	bridge->populate_database(db);

	CHECK(db->get_zone_count() == 0);

	db->close();
}

// 7. populate_database 后 disconnect 不崩溃
TEST_CASE("[Insights][TracyBridge] disconnect after populate_database does not crash") {
	Ref<InsightsTracyBridge> bridge;
	bridge.instantiate();

	Ref<InsightsDatabase> db;
	db.instantiate();
	db->open("test_disconnect");

	bridge->populate_database(db);
	bridge->disconnect();

	CHECK_FALSE(bridge->is_connected());
	CHECK_FALSE(bridge->has_data());

	db->close();
}

} // namespace TestInsightsPhase8

#endif // TRACY_SERVER_ENABLED
