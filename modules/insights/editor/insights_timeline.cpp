/**************************************************************************/
/*  insights_timeline.cpp                                                 */
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

#include "modules/insights/editor/insights_timeline.h"

#include "core/string/ustring.h"

InsightsTimeline::InsightsTimeline() {
	set_clip_contents(true);
	set_custom_minimum_size(Size2(640, 300));
}

void InsightsTimeline::set_database(const Ref<InsightsDatabase> &p_db) {
	database = p_db;
	_load_data();
	queue_redraw();
}

Ref<InsightsDatabase> InsightsTimeline::get_database() const {
	return database;
}

double InsightsTimeline::get_current_scale() const {
	return current_scale;
}

void InsightsTimeline::set_current_scale(double p_scale) {
	current_scale = p_scale;
	queue_redraw();
}

double InsightsTimeline::get_scroll_x() const {
	return scroll_x;
}

void InsightsTimeline::set_scroll_x(double p_scroll) {
	scroll_x = p_scroll;
	queue_redraw();
}

uint64_t InsightsTimeline::get_total_duration_ns() const {
	if (database.is_valid()) {
		return database->get_total_duration_ns();
	}
	return 0;
}

Rect2 InsightsTimeline::get_visible_range() const {
	return Rect2(scroll_x, 0, get_size().x / current_scale, get_size().y);
}

Dictionary InsightsTimeline::get_selected_zone() const {
	return selected_zone;
}

void InsightsTimeline::zoom_to_zone(const Dictionary &p_zone) {
	if (p_zone.has("start_ns") && p_zone.has("end_ns")) {
		uint64_t start_ns = (uint64_t)(int64_t)p_zone["start_ns"];
		uint64_t end_ns = (uint64_t)(int64_t)p_zone["end_ns"];
		uint64_t duration = end_ns - start_ns;
		if (duration == 0) {
			duration = 1;
		}
		double width = get_size().x;
		if (width <= 0) {
			width = 640.0;
		}
		current_scale = width / (double)duration;
		scroll_x = (double)start_ns;
		queue_redraw();
	}
}

void InsightsTimeline::_load_data() {
	cpu_zones.clear();
	gpu_zones.clear();
	load_zones.clear();
	net_zones.clear();
	frame_markers_data.clear();

	if (database.is_valid()) {
		Array all_zones = database->query_zones_in_range(0, UINT64_MAX);
		for (int i = 0; i < all_zones.size(); i++) {
			Dictionary zone = all_zones[i];
			String channel = zone["channel"];
			if (channel.begins_with("godot:gpu")) {
				gpu_zones.push_back(zone);
			} else if (channel.begins_with("godot:loading")) {
				load_zones.push_back(zone);
			} else if (channel.begins_with("godot:network")) {
				net_zones.push_back(zone);
			} else {
				cpu_zones.push_back(zone);
			}
		}
		frame_markers_data = database->query_frame_markers(0, UINT64_MAX);
	}
}

void InsightsTimeline::_notification(int p_what) {
	if (p_what == NOTIFICATION_DRAW) {
		_draw_timeline();
	}
}

void InsightsTimeline::_draw_timeline() {
	Size2i size = get_size();

	// Background.
	draw_rect(Rect2(Point2(), size), Color(0.11, 0.11, 0.14));

	// Draw frame markers as vertical lines.
	for (int i = 0; i < frame_markers_data.size(); i++) {
		Dictionary fm = frame_markers_data[i];
		uint64_t start_ns = (uint64_t)(int64_t)fm["start_ns"];
		double x = ((double)start_ns - scroll_x) * current_scale;
		if (x >= 0 && x < size.x) {
			draw_line(Point2(x, 0), Point2(x, size.y), Color(0.3, 0.3, 0.3, 0.5));
		}
	}

	// Track layout.
	int track_height = 20;
	int track_y = 0;

	// CPU zones track.
	draw_string(get_theme_font(SNAME("font")), Point2(4, track_y + 14), "CPU", HORIZONTAL_ALIGNMENT_LEFT, -1, get_theme_font_size(SNAME("font_size")), Color(0.8, 0.8, 0.8));
	track_y += track_height;
	for (int i = 0; i < cpu_zones.size(); i++) {
		Dictionary zone = cpu_zones[i];
		uint64_t z_start = (uint64_t)(int64_t)zone["start_ns"];
		uint64_t z_end = (uint64_t)(int64_t)zone["end_ns"];
		int depth = zone["depth"];
		double x1 = ((double)z_start - scroll_x) * current_scale;
		double x2 = ((double)z_end - scroll_x) * current_scale;
		double y = track_y + (double)(depth * track_height);
		if (x2 >= 0 && x1 < size.x && y >= 0 && y < size.y) {
			Rect2 rect(Point2(x1, y), Size2(MAX(x2 - x1, 1.0), track_height - 2));
			draw_rect(rect, Color(0.27, 0.56, 0.83));
			if (rect.size.x > 30) {
				draw_string(get_theme_font(SNAME("font")), Point2(x1 + 2, y + track_height - 5), zone["name"], HORIZONTAL_ALIGNMENT_LEFT, -1, get_theme_font_size(SNAME("font_size")), Color(1, 1, 1));
			}
		}
	}
	track_y += 6 * track_height; // Reserve space for depth lanes.

	// GPU zones track.
	draw_string(get_theme_font(SNAME("font")), Point2(4, track_y + 14), "GPU", HORIZONTAL_ALIGNMENT_LEFT, -1, get_theme_font_size(SNAME("font_size")), Color(0.8, 0.8, 0.8));
	track_y += track_height;
	for (int i = 0; i < gpu_zones.size(); i++) {
		Dictionary zone = gpu_zones[i];
		uint64_t z_start = (uint64_t)(int64_t)zone["start_ns"];
		uint64_t z_end = (uint64_t)(int64_t)zone["end_ns"];
		int depth = zone["depth"];
		double x1 = ((double)z_start - scroll_x) * current_scale;
		double x2 = ((double)z_end - scroll_x) * current_scale;
		double y = track_y + (double)(depth * track_height);
		if (x2 >= 0 && x1 < size.x && y >= 0 && y < size.y) {
			Rect2 rect(Point2(x1, y), Size2(MAX(x2 - x1, 1.0), track_height - 2));
			draw_rect(rect, Color(0.76, 0.35, 0.83));
			if (rect.size.x > 30) {
				draw_string(get_theme_font(SNAME("font")), Point2(x1 + 2, y + track_height - 5), zone["name"], HORIZONTAL_ALIGNMENT_LEFT, -1, get_theme_font_size(SNAME("font_size")), Color(1, 1, 1));
			}
		}
	}
	track_y += 4 * track_height;

	// Loading zones track.
	draw_string(get_theme_font(SNAME("font")), Point2(4, track_y + 14), "Loading", HORIZONTAL_ALIGNMENT_LEFT, -1, get_theme_font_size(SNAME("font_size")), Color(0.8, 0.8, 0.8));
	track_y += track_height;
	for (int i = 0; i < load_zones.size(); i++) {
		Dictionary zone = load_zones[i];
		uint64_t z_start = (uint64_t)(int64_t)zone["start_ns"];
		uint64_t z_end = (uint64_t)(int64_t)zone["end_ns"];
		int depth = zone["depth"];
		double x1 = ((double)z_start - scroll_x) * current_scale;
		double x2 = ((double)z_end - scroll_x) * current_scale;
		double y = track_y + (double)(depth * track_height);
		if (x2 >= 0 && x1 < size.x && y >= 0 && y < size.y) {
			Rect2 rect(Point2(x1, y), Size2(MAX(x2 - x1, 1.0), track_height - 2));
			draw_rect(rect, Color(0.83, 0.66, 0.27));
			if (rect.size.x > 30) {
				draw_string(get_theme_font(SNAME("font")), Point2(x1 + 2, y + track_height - 5), zone["name"], HORIZONTAL_ALIGNMENT_LEFT, -1, get_theme_font_size(SNAME("font_size")), Color(1, 1, 1));
			}
		}
	}
	track_y += 4 * track_height;

	// Network zones track.
	draw_string(get_theme_font(SNAME("font")), Point2(4, track_y + 14), "Network", HORIZONTAL_ALIGNMENT_LEFT, -1, get_theme_font_size(SNAME("font_size")), Color(0.8, 0.8, 0.8));
	track_y += track_height;
	for (int i = 0; i < net_zones.size(); i++) {
		Dictionary zone = net_zones[i];
		uint64_t z_start = (uint64_t)(int64_t)zone["start_ns"];
		uint64_t z_end = (uint64_t)(int64_t)zone["end_ns"];
		int depth = zone["depth"];
		double x1 = ((double)z_start - scroll_x) * current_scale;
		double x2 = ((double)z_end - scroll_x) * current_scale;
		double y = track_y + (double)(depth * track_height);
		if (x2 >= 0 && x1 < size.x && y >= 0 && y < size.y) {
			Rect2 rect(Point2(x1, y), Size2(MAX(x2 - x1, 1.0), track_height - 2));
			draw_rect(rect, Color(0.27, 0.83, 0.56));
			if (rect.size.x > 30) {
				draw_string(get_theme_font(SNAME("font")), Point2(x1 + 2, y + track_height - 5), zone["name"], HORIZONTAL_ALIGNMENT_LEFT, -1, get_theme_font_size(SNAME("font_size")), Color(1, 1, 1));
			}
		}
	}
}

void InsightsTimeline::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_database", "db"), &InsightsTimeline::set_database);
	ClassDB::bind_method(D_METHOD("get_database"), &InsightsTimeline::get_database);
	ClassDB::bind_method(D_METHOD("get_current_scale"), &InsightsTimeline::get_current_scale);
	ClassDB::bind_method(D_METHOD("set_current_scale", "scale"), &InsightsTimeline::set_current_scale);
	ClassDB::bind_method(D_METHOD("get_scroll_x"), &InsightsTimeline::get_scroll_x);
	ClassDB::bind_method(D_METHOD("set_scroll_x", "scroll"), &InsightsTimeline::set_scroll_x);
	ClassDB::bind_method(D_METHOD("get_total_duration_ns"), &InsightsTimeline::get_total_duration_ns);
	ClassDB::bind_method(D_METHOD("get_visible_range"), &InsightsTimeline::get_visible_range);
	ClassDB::bind_method(D_METHOD("get_selected_zone"), &InsightsTimeline::get_selected_zone);
	ClassDB::bind_method(D_METHOD("zoom_to_zone", "zone"), &InsightsTimeline::zoom_to_zone);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "database", PROPERTY_HINT_RESOURCE_TYPE, "InsightsDatabase"), "set_database", "get_database");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "current_scale"), "set_current_scale", "get_current_scale");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "scroll_x"), "set_scroll_x", "get_scroll_x");
}
