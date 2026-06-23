/**************************************************************************/
/*  insights_memory_panel.cpp                                             */
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

#include "insights_memory_panel.h"

#include "core/variant/dictionary.h"

InsightsMemoryPanel::InsightsMemoryPanel() {
	set_clip_contents(true);
	set_custom_minimum_size(Size2(640, 300));
}

void InsightsMemoryPanel::set_database(const Ref<InsightsDatabase> &p_db) {
	database = p_db;
	_load_data();
	queue_redraw();
}

Ref<InsightsDatabase> InsightsMemoryPanel::get_database() const {
	return database;
}

void InsightsMemoryPanel::_load_data() {
	allocations.clear();
	leaked_objects.clear();
	peak_memory = 0;

	if (database.is_null()) {
		return;
	}

	Array raw = database->query_allocations_in_range(0, UINT64_MAX);
	for (int i = 0; i < raw.size(); i++) {
		Dictionary d = raw[i];
		AllocEntry entry;
		entry.ptr = (uint64_t)(int64_t)d["ptr"];
		entry.size = (uint64_t)(int64_t)d["size"];
		entry.alloc_site = d.has("site_zone_id") ? String::num_int64((int64_t)d["site_zone_id"]) : String();
		entry.start_ns = (uint64_t)(int64_t)d["alloc_ns"];
		entry.end_ns = (uint64_t)(int64_t)d["free_ns"];
		allocations.push_back(entry);

		if (entry.end_ns == 0) {
			LeakEntry leak;
			leak.ptr = entry.ptr;
			leak.size = entry.size;
			leak.alloc_site = entry.alloc_site;
			leaked_objects.push_back(leak);
		}
	}

	peak_memory = database->get_peak_memory();
}

uint64_t InsightsMemoryPanel::get_peak_memory() const {
	return peak_memory;
}

Array InsightsMemoryPanel::get_leaked_allocations() const {
	Array result;
	for (int i = 0; i < leaked_objects.size(); i++) {
		const LeakEntry &leak = leaked_objects[i];
		Dictionary dict;
		dict["ptr"] = (int64_t)leak.ptr;
		dict["size"] = (int64_t)leak.size;
		dict["alloc_site"] = leak.alloc_site;
		result.push_back(dict);
	}
	return result;
}

void InsightsMemoryPanel::set_size_filter(uint64_t p_min_size) {
	if (size_filter != p_min_size) {
		size_filter = p_min_size;
		queue_redraw();
	}
}

uint64_t InsightsMemoryPanel::get_size_filter() const {
	return size_filter;
}

Array InsightsMemoryPanel::get_filtered_allocations() const {
	Array result;
	for (int i = 0; i < allocations.size(); i++) {
		const AllocEntry &entry = allocations[i];
		if (entry.size >= size_filter) {
			Dictionary dict;
			dict["ptr"] = (int64_t)entry.ptr;
			dict["size"] = (int64_t)entry.size;
			dict["alloc_site"] = entry.alloc_site;
			dict["start_ns"] = (int64_t)entry.start_ns;
			dict["end_ns"] = (int64_t)entry.end_ns;
			result.push_back(dict);
		}
	}
	return result;
}

void InsightsMemoryPanel::_notification(int p_what) {
	if (p_what == NOTIFICATION_DRAW) {
		// Draw memory waterfall — stub: draw allocation bars.
		if (allocations.is_empty()) {
			return;
		}

		// Find time range.
		uint64_t min_ns = UINT64_MAX;
		uint64_t max_ns = 0;
		for (int i = 0; i < allocations.size(); i++) {
			const AllocEntry &entry = allocations[i];
			if (entry.start_ns < min_ns) {
				min_ns = entry.start_ns;
			}
			uint64_t end = entry.end_ns != 0 ? entry.end_ns : entry.start_ns;
			if (end > max_ns) {
				max_ns = end;
			}
		}

		uint64_t duration = max_ns - min_ns;
		if (duration == 0) {
			return;
		}

		Size2 panel_size = get_size();
		float bar_height = 6.0;
		float row_spacing = 2.0;
		int row = 0;

		for (int i = 0; i < allocations.size(); i++) {
			const AllocEntry &entry = allocations[i];
			if (size_filter > 0 && entry.size < size_filter) {
				continue;
			}

			float x_start = (float)(entry.start_ns - min_ns) / (float)duration * panel_size.width;
			uint64_t end = entry.end_ns != 0 ? entry.end_ns : entry.start_ns;
			float x_end = (float)(end - min_ns) / (float)duration * panel_size.width;
			float y = row * (bar_height + row_spacing);

			if (y + bar_height > panel_size.height) {
				break;
			}

			Rect2 rect(x_start, y, x_end - x_start, bar_height);
			Color color = entry.end_ns == 0 ? Color(1.0, 0.3, 0.3, 0.8) : Color(0.3, 0.6, 1.0, 0.8);
			draw_rect(rect, color);

			row++;
		}
	}
}

void InsightsMemoryPanel::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_database", "db"), &InsightsMemoryPanel::set_database);
	ClassDB::bind_method(D_METHOD("get_database"), &InsightsMemoryPanel::get_database);
	ClassDB::bind_method(D_METHOD("get_peak_memory"), &InsightsMemoryPanel::get_peak_memory);
	ClassDB::bind_method(D_METHOD("get_leaked_allocations"), &InsightsMemoryPanel::get_leaked_allocations);
	ClassDB::bind_method(D_METHOD("set_size_filter", "min_size"), &InsightsMemoryPanel::set_size_filter);
	ClassDB::bind_method(D_METHOD("get_size_filter"), &InsightsMemoryPanel::get_size_filter);
	ClassDB::bind_method(D_METHOD("get_filtered_allocations"), &InsightsMemoryPanel::get_filtered_allocations);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "database", PROPERTY_HINT_RESOURCE_TYPE, "InsightsDatabase"), "set_database", "get_database");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "size_filter"), "set_size_filter", "get_size_filter");
}
