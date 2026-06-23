/**************************************************************************/
/*  insights_flamegraph.cpp                                               */
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

#include "modules/insights/editor/insights_flamegraph.h"

#include "core/math/color.h"

InsightsFlamegraph::InsightsFlamegraph() {
	set_clip_contents(true);
	set_custom_minimum_size(Size2(640, 400));
}

void InsightsFlamegraph::set_database(const Ref<InsightsDatabase> &p_db) {
	database = p_db;
	_build_flame_tree();
	queue_redraw();
}

Ref<InsightsDatabase> InsightsFlamegraph::get_database() const {
	return database;
}

void InsightsFlamegraph::_build_flame_tree() {
	flame_nodes.clear();
	stack_depth = 0;

	if (database.is_null()) {
		return;
	}

	Array zones = database->query_zones_in_range(0, UINT64_MAX);

	// Build all FlameNode entries from zone records.
	for (int i = 0; i < zones.size(); i++) {
		Dictionary zone_dict = zones[i];
		FlameNode node;
		node.id = i;
		node.name = zone_dict["name"];
		node.start_ns = (uint64_t)(int64_t)zone_dict["start_ns"];
		node.end_ns = (uint64_t)(int64_t)zone_dict["end_ns"];
		node.depth = zone_dict["depth"];

		if (node.depth > stack_depth) {
			stack_depth = node.depth;
		}

		flame_nodes.push_back(node);
	}

	// Build parent-child relationships.
	// Root nodes: parent_zone_id == -1.
	// Children: zones whose parent_zone_id matches a parent's index.
	for (int i = 0; i < zones.size(); i++) {
		Dictionary zone_dict = zones[i];
		int parent_zone_id = zone_dict["parent_zone_id"];
		if (parent_zone_id == -1) {
			// Root node, no parent to register with.
			continue;
		}
		// Register this node as a child of its parent.
		if (parent_zone_id >= 0 && parent_zone_id < flame_nodes.size()) {
			flame_nodes.write[parent_zone_id].children_ids.push_back(i);
		}
	}
}

Array InsightsFlamegraph::get_root_nodes() const {
	Array result;
	for (int i = 0; i < flame_nodes.size(); i++) {
		if (flame_nodes[i].depth == 0) {
			Dictionary dict;
			dict["id"] = flame_nodes[i].id;
			dict["name"] = flame_nodes[i].name;
			dict["start_ns"] = (int64_t)flame_nodes[i].start_ns;
			dict["end_ns"] = (int64_t)flame_nodes[i].end_ns;
			dict["depth"] = flame_nodes[i].depth;
			result.push_back(dict);
		}
	}
	return result;
}

Array InsightsFlamegraph::get_children(int p_node_id) const {
	Array result;
	if (p_node_id < 0 || p_node_id >= flame_nodes.size()) {
		return result;
	}

	const FlameNode &parent = flame_nodes[p_node_id];
	for (int i = 0; i < parent.children_ids.size(); i++) {
		int child_id = parent.children_ids[i];
		if (child_id >= 0 && child_id < flame_nodes.size()) {
			const FlameNode &child = flame_nodes[child_id];
			Dictionary dict;
			dict["id"] = child.id;
			dict["name"] = child.name;
			dict["start_ns"] = (int64_t)child.start_ns;
			dict["end_ns"] = (int64_t)child.end_ns;
			dict["depth"] = child.depth;
			result.push_back(dict);
		}
	}
	return result;
}

void InsightsFlamegraph::set_search_query(const String &p_query) {
	if (search_query == p_query) {
		return;
	}
	search_query = p_query;
	queue_redraw();
}

String InsightsFlamegraph::get_search_query() const {
	return search_query;
}

Array InsightsFlamegraph::get_filtered_nodes() const {
	Array result;
	String query_lower = search_query.to_lower();

	for (int i = 0; i < flame_nodes.size(); i++) {
		if (search_query.is_empty()) {
			Dictionary dict;
			dict["id"] = flame_nodes[i].id;
			dict["name"] = flame_nodes[i].name;
			dict["start_ns"] = (int64_t)flame_nodes[i].start_ns;
			dict["end_ns"] = (int64_t)flame_nodes[i].end_ns;
			dict["depth"] = flame_nodes[i].depth;
			result.push_back(dict);
		} else if (flame_nodes[i].name.to_lower().find(query_lower) != -1) {
			Dictionary dict;
			dict["id"] = flame_nodes[i].id;
			dict["name"] = flame_nodes[i].name;
			dict["start_ns"] = (int64_t)flame_nodes[i].start_ns;
			dict["end_ns"] = (int64_t)flame_nodes[i].end_ns;
			dict["depth"] = flame_nodes[i].depth;
			result.push_back(dict);
		}
	}
	return result;
}

Dictionary InsightsFlamegraph::get_selected_node() const {
	return selected_node;
}

void InsightsFlamegraph::_notification(int p_what) {
	if (p_what == NOTIFICATION_DRAW) {
		if (flame_nodes.is_empty()) {
			return;
		}

		// Determine the global time range across all nodes.
		uint64_t min_ns = flame_nodes[0].start_ns;
		uint64_t max_ns = flame_nodes[0].end_ns;
		for (int i = 1; i < flame_nodes.size(); i++) {
			if (flame_nodes[i].start_ns < min_ns) {
				min_ns = flame_nodes[i].start_ns;
			}
			if (flame_nodes[i].end_ns > max_ns) {
				max_ns = flame_nodes[i].end_ns;
			}
		}

		uint64_t total_ns = max_ns - min_ns;
		if (total_ns == 0) {
			return;
		}

		Size2i size = get_size();
		int row_height = 20;
		int total_rows = stack_depth + 1;

		// Depth-based color palette.
		const Color depth_colors[] = {
			Color(0.96, 0.42, 0.23),
			Color(0.90, 0.62, 0.26),
			Color(0.85, 0.78, 0.30),
			Color(0.40, 0.78, 0.47),
			Color(0.27, 0.67, 0.78),
			Color(0.39, 0.47, 0.83),
			Color(0.62, 0.38, 0.78),
			Color(0.83, 0.35, 0.55),
		};
		const int num_colors = sizeof(depth_colors) / sizeof(depth_colors[0]);

		String query_lower = search_query.to_lower();

		for (int i = 0; i < flame_nodes.size(); i++) {
			const FlameNode &node = flame_nodes[i];

			real_t x_ratio_start = (real_t)(node.start_ns - min_ns) / (real_t)total_ns;
			real_t x_ratio_end = (real_t)(node.end_ns - min_ns) / (real_t)total_ns;

			real_t x = x_ratio_start * size.width;
			real_t w = (x_ratio_end - x_ratio_start) * size.width;
			real_t y = size.height - (node.depth + 1) * row_height;

			if (w < 1.0) {
				continue; // Too narrow to draw.
			}

			Color color = depth_colors[node.depth % num_colors];

			// Dim nodes that don't match search query.
			if (!search_query.is_empty() && node.name.to_lower().find(query_lower) == -1) {
				color = color.darkened(0.6);
				color.a = 0.4;
			}

			draw_rect(Rect2(x, y, w, row_height - 1), color);

			// Draw label if the rect is wide enough.
			if (w > 30.0) {
				draw_string(get_theme_font(SNAME("font")), Point2(x + 2, y + row_height - 5), node.name, HORIZONTAL_ALIGNMENT_LEFT, w - 4, get_theme_font_size(SNAME("font_size")), Color(1, 1, 1));
			}
		}
	}
}

void InsightsFlamegraph::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_database", "db"), &InsightsFlamegraph::set_database);
	ClassDB::bind_method(D_METHOD("get_database"), &InsightsFlamegraph::get_database);

	ClassDB::bind_method(D_METHOD("get_root_nodes"), &InsightsFlamegraph::get_root_nodes);
	ClassDB::bind_method(D_METHOD("get_children", "node_id"), &InsightsFlamegraph::get_children);

	ClassDB::bind_method(D_METHOD("set_search_query", "query"), &InsightsFlamegraph::set_search_query);
	ClassDB::bind_method(D_METHOD("get_search_query"), &InsightsFlamegraph::get_search_query);

	ClassDB::bind_method(D_METHOD("get_filtered_nodes"), &InsightsFlamegraph::get_filtered_nodes);
	ClassDB::bind_method(D_METHOD("get_selected_node"), &InsightsFlamegraph::get_selected_node);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "database", PROPERTY_HINT_RESOURCE_TYPE, "InsightsDatabase"), "set_database", "get_database");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "search_query", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_EDITOR), "set_search_query", "get_search_query");
}
