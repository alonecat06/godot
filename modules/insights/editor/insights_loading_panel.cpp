/**************************************************************************/
/*  insights_loading_panel.cpp                                            */
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

#include "insights_loading_panel.h"

void InsightsLoadingPanel::_load_data() {
	load_nodes.clear();

	if (database.is_null()) {
		return;
	}

	Array loads = database->query_resource_loads();
	for (int i = 0; i < loads.size(); i++) {
		Dictionary dict = loads[i];
		LoadNode node;
		node.id = i;
		node.path = dict["path"];
		node.start_ns = (uint64_t)(int64_t)dict["start_ns"];
		node.end_ns = (uint64_t)(int64_t)dict["end_ns"];
		node.memory_size = (uint64_t)(int64_t)dict["size_bytes"];
		node.parent_path = dict["parent_path"];
		load_nodes.push_back(node);
	}
}

void InsightsLoadingPanel::_notification(int p_what) {
	if (p_what == NOTIFICATION_DRAW) {
		// TODO: Draw dependency tree.
	}
}

void InsightsLoadingPanel::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_database", "db"), &InsightsLoadingPanel::set_database);
	ClassDB::bind_method(D_METHOD("get_database"), &InsightsLoadingPanel::get_database);

	ClassDB::bind_method(D_METHOD("get_root_loads"), &InsightsLoadingPanel::get_root_loads);
	ClassDB::bind_method(D_METHOD("get_dependencies", "path"), &InsightsLoadingPanel::get_dependencies);
	ClassDB::bind_method(D_METHOD("get_bottleneck"), &InsightsLoadingPanel::get_bottleneck);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "database", PROPERTY_HINT_RESOURCE_TYPE, "InsightsDatabase"), "set_database", "get_database");
}

void InsightsLoadingPanel::set_database(const Ref<InsightsDatabase> &p_db) {
	database = p_db;
	_load_data();
	queue_redraw();
}

Ref<InsightsDatabase> InsightsLoadingPanel::get_database() const {
	return database;
}

Array InsightsLoadingPanel::get_root_loads() const {
	Array result;
	for (int i = 0; i < load_nodes.size(); i++) {
		const LoadNode &node = load_nodes[i];
		if (node.parent_path.is_empty()) {
			Dictionary dict;
			dict["id"] = node.id;
			dict["path"] = node.path;
			dict["start_ns"] = (int64_t)node.start_ns;
			dict["end_ns"] = (int64_t)node.end_ns;
			dict["memory_size"] = (int64_t)node.memory_size;
			dict["parent_path"] = node.parent_path;
			result.push_back(dict);
		}
	}
	return result;
}

Array InsightsLoadingPanel::get_dependencies(const String &p_path) const {
	Array result;
	for (int i = 0; i < load_nodes.size(); i++) {
		const LoadNode &node = load_nodes[i];
		if (node.parent_path == p_path) {
			Dictionary dict;
			dict["id"] = node.id;
			dict["path"] = node.path;
			dict["start_ns"] = (int64_t)node.start_ns;
			dict["end_ns"] = (int64_t)node.end_ns;
			dict["memory_size"] = (int64_t)node.memory_size;
			dict["parent_path"] = node.parent_path;
			result.push_back(dict);
		}
	}
	return result;
}

Dictionary InsightsLoadingPanel::get_bottleneck() const {
	Dictionary result;
	if (load_nodes.is_empty()) {
		return result;
	}

	int bottleneck_idx = 0;
	uint64_t max_memory = load_nodes[0].memory_size;
	uint64_t max_duration = load_nodes[0].end_ns - load_nodes[0].start_ns;
	bool use_duration = (max_memory == 0);

	for (int i = 1; i < load_nodes.size(); i++) {
		const LoadNode &node = load_nodes[i];
		uint64_t duration = node.end_ns - node.start_ns;

		if (use_duration) {
			if (duration > max_duration) {
				max_duration = duration;
				bottleneck_idx = i;
			}
		} else {
			if (node.memory_size > max_memory) {
				max_memory = node.memory_size;
				bottleneck_idx = i;
			}
		}
	}

	const LoadNode &bottleneck = load_nodes[bottleneck_idx];
	result["id"] = bottleneck.id;
	result["path"] = bottleneck.path;
	result["start_ns"] = (int64_t)bottleneck.start_ns;
	result["end_ns"] = (int64_t)bottleneck.end_ns;
	result["memory_size"] = (int64_t)bottleneck.memory_size;
	result["parent_path"] = bottleneck.parent_path;

	return result;
}

InsightsLoadingPanel::InsightsLoadingPanel() {
	set_clip_contents(true);
	set_custom_minimum_size(Size2(640, 300));
}
