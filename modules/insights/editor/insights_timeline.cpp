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
/* TORT OR OTHERWISE, ARISING OUT OF OR IN CONNECTION WITH THE SOFTWARE   */
/* OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                          */
/**************************************************************************/

#include "modules/insights/editor/insights_timeline.h"

#include "core/string/ustring.h"

InsightsTimeline::InsightsTimeline() {
	set_clip_contents(true);
	set_custom_minimum_size(Size2(640, 300));
	set_focus_mode(FOCUS_CLICK);
}

void InsightsTimeline::set_database(const Ref<InsightsDatabase> &p_db) {
	database = p_db;
	_load_data();
	queue_redraw();
}

Ref<InsightsDatabase> InsightsTimeline::get_database() const {
	return database;
}

void InsightsTimeline::set_channel_filter(ChannelFilter p_filter) {
	channel_filter = p_filter;
	_load_data();
	queue_redraw();
}

InsightsTimeline::ChannelFilter InsightsTimeline::get_channel_filter() const {
	return channel_filter;
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

void InsightsTimeline::zoom_in(double p_factor) {
	double width = get_size().x;
	if (width <= 0) {
		return;
	}
	double center_ns = scroll_x + (width / current_scale) * 0.5;
	current_scale *= p_factor;
	scroll_x = center_ns - (width / current_scale) * 0.5;
	queue_redraw();
}

void InsightsTimeline::zoom_out(double p_factor) {
	double width = get_size().x;
	if (width <= 0) {
		return;
	}
	double center_ns = scroll_x + (width / current_scale) * 0.5;
	current_scale /= p_factor;
	scroll_x = center_ns - (width / current_scale) * 0.5;
	queue_redraw();
}

void InsightsTimeline::zoom_fit() {
	_auto_fit();
	queue_redraw();
}

void InsightsTimeline::_load_data() {
	filtered_zones.clear();
	frame_markers_data.clear();

	if (database.is_valid()) {
		Array all_zones = database->query_zones_in_range(0, UINT64_MAX);

		for (int i = 0; i < all_zones.size(); i++) {
			Dictionary zone = all_zones[i];
			String channel = zone["channel"];

			bool include = false;
			switch (channel_filter) {
				case CHANNEL_ALL:
					include = true;
					break;
				case CHANNEL_CPU:
					include = !channel.begins_with("godot:gpu") && !channel.begins_with("godot:loading") && !channel.begins_with("godot:network") && !channel.begins_with("godot:mem") && !channel.begins_with("godot:script");
					break;
				case CHANNEL_GPU:
					include = channel.begins_with("godot:gpu");
					break;
				case CHANNEL_MEMORY:
					include = channel.begins_with("godot:mem");
					break;
				case CHANNEL_LOADING:
					include = channel.begins_with("godot:loading");
					break;
				case CHANNEL_NETWORK:
					include = channel.begins_with("godot:network");
					break;
				case CHANNEL_SCRIPT:
					include = channel.begins_with("godot:script");
					break;
			}
			if (include) {
				filtered_zones.push_back(zone);
			}
		}
		frame_markers_data = database->query_frame_markers(0, UINT64_MAX);

		_auto_fit();
	}
}

void InsightsTimeline::_auto_fit() {
	if (database.is_null()) {
		return;
	}

	uint64_t total_duration = database->get_total_duration_ns();
	if (total_duration == 0) {
		return;
	}

	double width = get_size().x;
	if (width <= 0) {
		width = 640.0;
	}

	current_scale = width / (double)total_duration;

	uint64_t min_start = UINT64_MAX;

	for (int i = 0; i < filtered_zones.size(); i++) {
		Dictionary d = filtered_zones[i];
		uint64_t s = (uint64_t)(int64_t)d["start_ns"];
		if (s < min_start) {
			min_start = s;
		}
	}
	for (int i = 0; i < frame_markers_data.size(); i++) {
		Dictionary d = frame_markers_data[i];
		uint64_t s = (uint64_t)(int64_t)d["start_ns"];
		if (s < min_start) {
			min_start = s;
		}
	}

	if (min_start != UINT64_MAX) {
		scroll_x = (double)min_start;
		min_start_ns = scroll_x;
	} else {
		min_start_ns = 0;
	}
}

void InsightsTimeline::_notification(int p_what) {
	if (p_what == NOTIFICATION_DRAW) {
		_draw_timeline();
	} else if (p_what == NOTIFICATION_RESIZED) {
		_auto_fit();
		queue_redraw();
	}
}

void InsightsTimeline::gui_input(const Ref<InputEvent> &p_event) {
	Ref<InputEventMouseButton> mb = p_event;
	if (mb.is_valid()) {
		if (mb->get_button_index() == MouseButton::WHEEL_UP) {
			double mouse_ns = scroll_x + (mb->get_position().x / current_scale);
			current_scale *= 1.3;
			scroll_x = mouse_ns - (mb->get_position().x / current_scale);
			queue_redraw();
			accept_event();
		} else if (mb->get_button_index() == MouseButton::WHEEL_DOWN) {
			double mouse_ns = scroll_x + (mb->get_position().x / current_scale);
			current_scale /= 1.3;
			scroll_x = mouse_ns - (mb->get_position().x / current_scale);
			queue_redraw();
			accept_event();
		} else if (mb->get_button_index() == MouseButton::LEFT) {
			if (mb->is_pressed()) {
				is_dragging = true;
				drag_start_x = mb->get_position().x;
				drag_start_scroll = scroll_x;
			} else {
				is_dragging = false;
			}
			accept_event();
		} else if (mb->get_button_index() == MouseButton::MIDDLE) {
			if (mb->is_pressed()) {
				is_dragging = true;
				drag_start_x = mb->get_position().x;
				drag_start_scroll = scroll_x;
			} else {
				is_dragging = false;
			}
			accept_event();
		}
	}

	Ref<InputEventMouseMotion> mm = p_event;
	if (mm.is_valid() && is_dragging) {
		double dx = mm->get_position().x - drag_start_x;
		scroll_x = drag_start_scroll - dx / current_scale;
		queue_redraw();
		accept_event();
	}
}

void InsightsTimeline::_draw_zone_track(const String &p_label, const Array &p_zones, const Color &p_color, int &r_track_y, int p_track_y_start, int p_max_depth, const Size2i &p_size, int p_track_height) {
	draw_string(get_theme_font(SNAME("font")), Point2(4, r_track_y + 14), p_label, HORIZONTAL_ALIGNMENT_LEFT, -1, get_theme_font_size(SNAME("font_size")), Color(0.8, 0.8, 0.8));
	r_track_y += p_track_height;

	for (int i = 0; i < p_zones.size(); i++) {
		Dictionary zone = p_zones[i];
		uint64_t z_start = (uint64_t)(int64_t)zone["start_ns"];
		uint64_t z_end = (uint64_t)(int64_t)zone["end_ns"];
		int depth = zone["depth"];
		String channel = zone["channel"];
		String name = zone["name"];

		double x1 = ((double)z_start - scroll_x) * current_scale;
		double x2 = ((double)z_end - scroll_x) * current_scale;
		double y = r_track_y + (double)(depth * p_track_height);

		if (x2 >= 0 && x1 < p_size.x && y >= p_track_y_start && y < p_size.y) {
			Rect2 rect(Point2(x1, y), Size2(MAX(x2 - x1, 1.0), p_track_height - 2));
			draw_rect(rect, p_color);

			String zone_label;
			if (rect.size.x > 20) {
				String ch_short = channel.replace("godot:", "");
				zone_label = ch_short + " > " + name;
			}
			if (rect.size.x > 80 && z_end > z_start) {
				double dur_ms = (double)(z_end - z_start) / 1000000.0;
				zone_label += vformat(" (%.2fms)", dur_ms);
			}
			if (!zone_label.is_empty() && rect.size.x > 20) {
				draw_string(get_theme_font(SNAME("font")), Point2(x1 + 2, y + p_track_height - 5), zone_label, HORIZONTAL_ALIGNMENT_LEFT, -1, get_theme_font_size(SNAME("font_size")), Color(1, 1, 1));
			}
		}
	}
	r_track_y += p_max_depth * p_track_height;
}

void InsightsTimeline::_draw_timeline() {
	Size2i size = get_size();
	if (size.x <= 0 || size.y <= 0) {
		return;
	}

	draw_rect(Rect2(Point2(), size), Color(0.11, 0.11, 0.14));

	// Time ruler.
	int ruler_height = 24;
	draw_rect(Rect2(Point2(0, 0), Size2(size.x, ruler_height)), Color(0.08, 0.08, 0.10));

	double visible_duration = (double)size.x / current_scale;
	double visible_start = scroll_x;
	double tick_intervals[] = { 1000.0, 10000.0, 100000.0, 1000000.0, 10000000.0, 100000000.0, 1000000000.0, 10000000000.0, 60000000000.0 };
	double tick_interval = tick_intervals[sizeof(tick_intervals) / sizeof(tick_intervals[0]) - 1];
	for (int i = 0; i < (int)(sizeof(tick_intervals) / sizeof(tick_intervals[0])); i++) {
		if (visible_duration / tick_intervals[i] < 20) {
			tick_interval = tick_intervals[i];
			break;
		}
	}
	uint64_t first_tick = (uint64_t)(visible_start / tick_interval) * (uint64_t)tick_interval;
	for (uint64_t t = first_tick; t < (uint64_t)(visible_start + visible_duration); t += (uint64_t)tick_interval) {
		double x = ((double)t - scroll_x) * current_scale;
		if (x >= 0 && x < size.x) {
			draw_line(Point2(x, ruler_height - 6), Point2(x, ruler_height), Color(0.5, 0.5, 0.5));
			double ms = (double)t / 1000000.0;
			String label;
			if (ms >= 1000.0) {
				label = vformat("%.1fs", ms / 1000.0);
			} else if (ms >= 1.0) {
				label = vformat("%.1fms", ms);
			} else {
				label = vformat("%.0fus", ms * 1000.0);
			}
			draw_string(get_theme_font(SNAME("font")), Point2(x + 2, 14), label, HORIZONTAL_ALIGNMENT_LEFT, -1, get_theme_font_size(SNAME("font_size")), Color(0.6, 0.6, 0.6));
		}
	}

	// Frame markers as vertical bands.
	int track_y_start = ruler_height;
	for (int i = 0; i < frame_markers_data.size(); i++) {
		Dictionary fm = frame_markers_data[i];
		uint64_t fm_start = (uint64_t)(int64_t)fm["start_ns"];
		uint64_t fm_end = (uint64_t)(int64_t)fm["end_ns"];
		double x1 = ((double)fm_start - scroll_x) * current_scale;
		double x2 = ((double)fm_end - scroll_x) * current_scale;
		if (x2 >= 0 && x1 < size.x) {
			Color band_color = (i % 2 == 0) ? Color(0.15, 0.15, 0.18) : Color(0.13, 0.13, 0.16);
			draw_rect(Rect2(Point2(x1, track_y_start), Size2(MAX(x2 - x1, 1.0), size.y - track_y_start)), band_color);
			draw_line(Point2(x1, track_y_start), Point2(x1, size.y), Color(0.3, 0.3, 0.35, 0.8));
		}
	}

	// Separate filtered_zones into track groups.
	Array cpu_zones, gpu_zones, mem_zones, load_zones, net_zones, script_zones;
	for (int i = 0; i < filtered_zones.size(); i++) {
		Dictionary zone = filtered_zones[i];
		String channel = zone["channel"];
		if (channel.begins_with("godot:gpu")) {
			gpu_zones.push_back(zone);
		} else if (channel.begins_with("godot:loading")) {
			load_zones.push_back(zone);
		} else if (channel.begins_with("godot:network")) {
			net_zones.push_back(zone);
		} else if (channel.begins_with("godot:mem")) {
			mem_zones.push_back(zone);
		} else if (channel.begins_with("godot:script")) {
			script_zones.push_back(zone);
		} else {
			cpu_zones.push_back(zone);
		}
	}

	int track_height = 20;
	int track_y = track_y_start;

	if (channel_filter == CHANNEL_ALL || channel_filter == CHANNEL_CPU) {
		_draw_zone_track("CPU", cpu_zones, Color(0.27, 0.56, 0.83), track_y, track_y_start, 6, size, track_height);
	}
	if (channel_filter == CHANNEL_ALL || channel_filter == CHANNEL_GPU) {
		_draw_zone_track("GPU", gpu_zones, Color(0.76, 0.35, 0.83), track_y, track_y_start, 4, size, track_height);
	}
	if (channel_filter == CHANNEL_ALL || channel_filter == CHANNEL_MEMORY) {
		_draw_zone_track("Memory", mem_zones, Color(0.83, 0.27, 0.56), track_y, track_y_start, 4, size, track_height);
	}
	if (channel_filter == CHANNEL_ALL || channel_filter == CHANNEL_LOADING) {
		_draw_zone_track("Loading", load_zones, Color(0.83, 0.66, 0.27), track_y, track_y_start, 4, size, track_height);
	}
	if (channel_filter == CHANNEL_ALL || channel_filter == CHANNEL_NETWORK) {
		_draw_zone_track("Network", net_zones, Color(0.27, 0.83, 0.56), track_y, track_y_start, 4, size, track_height);
	}
	if (channel_filter == CHANNEL_ALL || channel_filter == CHANNEL_SCRIPT) {
		_draw_zone_track("Script", script_zones, Color(0.56, 0.83, 0.27), track_y, track_y_start, 4, size, track_height);
	}

	// Summary overlay.
	if (database.is_valid()) {
		String filter_name;
		switch (channel_filter) {
			case CHANNEL_ALL: filter_name = "All"; break;
			case CHANNEL_CPU: filter_name = "CPU"; break;
			case CHANNEL_GPU: filter_name = "GPU"; break;
			case CHANNEL_MEMORY: filter_name = "Memory"; break;
			case CHANNEL_LOADING: filter_name = "Loading"; break;
			case CHANNEL_NETWORK: filter_name = "Network"; break;
			case CHANNEL_SCRIPT: filter_name = "Script"; break;
		}
		String summary = vformat("[%s] Zones: %d  Frames: %d  Duration: %.2fms  |  Scroll=Zoom, Drag=Pan",
				filter_name,
				filtered_zones.size(),
				frame_markers_data.size(),
				database->get_total_duration_ns() / 1000000.0);
		draw_string(get_theme_font(SNAME("font")), Point2(size.x - 500, 14), summary, HORIZONTAL_ALIGNMENT_LEFT, -1, get_theme_font_size(SNAME("font_size")), Color(0.7, 0.7, 0.7));
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
	ClassDB::bind_method(D_METHOD("zoom_in", "factor"), &InsightsTimeline::zoom_in, DEFVAL(1.5));
	ClassDB::bind_method(D_METHOD("zoom_out", "factor"), &InsightsTimeline::zoom_out, DEFVAL(1.5));
	ClassDB::bind_method(D_METHOD("zoom_fit"), &InsightsTimeline::zoom_fit);
	ClassDB::bind_method(D_METHOD("set_channel_filter", "filter"), &InsightsTimeline::set_channel_filter);
	ClassDB::bind_method(D_METHOD("get_channel_filter"), &InsightsTimeline::get_channel_filter);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "database", PROPERTY_HINT_RESOURCE_TYPE, "InsightsDatabase"), "set_database", "get_database");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "current_scale"), "set_current_scale", "get_current_scale");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "scroll_x"), "set_scroll_x", "get_scroll_x");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "channel_filter", PROPERTY_HINT_ENUM, "All,CPU,GPU,Memory,Loading,Network,Script"), "set_channel_filter", "get_channel_filter");

	BIND_ENUM_CONSTANT(CHANNEL_ALL);
	BIND_ENUM_CONSTANT(CHANNEL_CPU);
	BIND_ENUM_CONSTANT(CHANNEL_GPU);
	BIND_ENUM_CONSTANT(CHANNEL_MEMORY);
	BIND_ENUM_CONSTANT(CHANNEL_LOADING);
	BIND_ENUM_CONSTANT(CHANNEL_NETWORK);
	BIND_ENUM_CONSTANT(CHANNEL_SCRIPT);
}
