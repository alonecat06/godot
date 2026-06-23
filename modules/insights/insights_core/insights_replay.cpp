/**************************************************************************/
/*  insights_replay.cpp                                                   */
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

#include "insights_replay.h"

void InsightsReplay::set_database(const Ref<InsightsDatabase> &p_db) {
	database = p_db;
}

Ref<InsightsDatabase> InsightsReplay::get_database() const {
	return database;
}

void InsightsReplay::play() {
	is_playing = true;
}

void InsightsReplay::pause() {
	is_playing = false;
}

void InsightsReplay::seek(uint64_t p_time_ns) {
	current_time_ns = p_time_ns;
}

void InsightsReplay::set_speed(double p_scalar) {
	speed_scalar = p_scalar;
}

double InsightsReplay::get_speed() const {
	return speed_scalar;
}

bool InsightsReplay::get_is_playing() const {
	return is_playing;
}

uint64_t InsightsReplay::get_current_time_ns() const {
	return current_time_ns;
}

void InsightsReplay::tick(double p_delta) {
	if (!is_playing) {
		return;
	}
	current_time_ns += (uint64_t)(p_delta * speed_scalar * 1e9);
}

Array InsightsReplay::get_zones_at(uint64_t p_time_ns) const {
	Array result;
	if (database.is_null()) {
		return result;
	}

	Array candidates = database->query_zones_in_range(0, p_time_ns);
	for (int i = 0; i < candidates.size(); i++) {
		Dictionary zone = candidates[i];
		uint64_t start_ns = (uint64_t)(int64_t)zone["start_ns"];
		uint64_t end_ns = (uint64_t)(int64_t)zone["end_ns"];
		if (start_ns <= p_time_ns && end_ns >= p_time_ns) {
			result.push_back(zone);
		}
	}
	return result;
}

void InsightsReplay::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_database", "db"), &InsightsReplay::set_database);
	ClassDB::bind_method(D_METHOD("get_database"), &InsightsReplay::get_database);

	ClassDB::bind_method(D_METHOD("play"), &InsightsReplay::play);
	ClassDB::bind_method(D_METHOD("pause"), &InsightsReplay::pause);
	ClassDB::bind_method(D_METHOD("seek", "time_ns"), &InsightsReplay::seek);

	ClassDB::bind_method(D_METHOD("set_speed", "scalar"), &InsightsReplay::set_speed);
	ClassDB::bind_method(D_METHOD("get_speed"), &InsightsReplay::get_speed);

	ClassDB::bind_method(D_METHOD("get_is_playing"), &InsightsReplay::get_is_playing);
	ClassDB::bind_method(D_METHOD("get_current_time_ns"), &InsightsReplay::get_current_time_ns);

	ClassDB::bind_method(D_METHOD("tick", "delta"), &InsightsReplay::tick);
	ClassDB::bind_method(D_METHOD("get_zones_at", "time_ns"), &InsightsReplay::get_zones_at);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "database", PROPERTY_HINT_RESOURCE_TYPE, "InsightsDatabase"), "set_database", "get_database");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "speed", PROPERTY_HINT_RANGE, "0.0,100.0,0.1"), "set_speed", "get_speed");
}
