/**************************************************************************/
/*  insights_network_panel.cpp                                            */
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

#include "insights_network_panel.h"

#include "core/variant/dictionary.h"
#include "scene/resources/style_box.h"

void InsightsNetworkPanel::_load_data() {
	network_events.clear();
	if (database.is_null()) {
		return;
	}

	Array zones = database->query_zones_in_range(0, UINT64_MAX);
	for (int i = 0; i < zones.size(); i++) {
		Dictionary zone = zones[i];
		String name = zone["name"];
		if (name.begins_with("godot:network")) {
			network_events.push_back(zone);
		}
	}
}

void InsightsNetworkPanel::_notification(int p_what) {
	if (p_what == NOTIFICATION_DRAW) {
		Size2i size = get_size();

		// Draw background.
		draw_rect(Rect2i(0, 0, size.width, size.height), Color(0.1, 0.1, 0.12));

		if (network_events.size() == 0) {
			return;
		}

		// Draw network event bars as a timeline stub.
		int bar_height = 16;
		int row_spacing = 4;
		int y_offset = 10;

		for (int i = 0; i < network_events.size(); i++) {
			Dictionary evt = network_events[i];
			int64_t start_ns = evt["start_ns"];
			int64_t end_ns = evt["end_ns"];

			float x_start = (float)(start_ns % (uint64_t)size.width * 1000) / 1000.0;
			float x_end = (float)(end_ns % (uint64_t)size.width * 1000) / 1000.0;
			if (x_end <= x_start) {
				x_end = x_start + 4.0;
			}

			int y = y_offset + i * (bar_height + row_spacing);
			if (y + bar_height > size.height) {
				break;
			}

			draw_rect(Rect2(x_start, y, x_end - x_start, bar_height), Color(0.3, 0.6, 1.0));
		}
	}
}

void InsightsNetworkPanel::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_database", "db"), &InsightsNetworkPanel::set_database);
	ClassDB::bind_method(D_METHOD("get_database"), &InsightsNetworkPanel::get_database);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "database", PROPERTY_HINT_RESOURCE_TYPE, "InsightsDatabase"), "set_database", "get_database");
}

void InsightsNetworkPanel::set_database(const Ref<InsightsDatabase> &p_db) {
	database = p_db;
	_load_data();
	queue_redraw();
}

Ref<InsightsDatabase> InsightsNetworkPanel::get_database() const {
	return database;
}

InsightsNetworkPanel::InsightsNetworkPanel() {
	set_clip_contents(true);
	set_custom_minimum_size(Size2(640, 200));
}
