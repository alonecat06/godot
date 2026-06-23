/**************************************************************************/
/*  insights_flamegraph.h                                                 */
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

#include "scene/gui/control.h"
#include "modules/insights/insights_core/insights_database.h"

class InsightsFlamegraph : public Control {
	GDCLASS(InsightsFlamegraph, Control);

public:
	struct FlameNode {
		int id = 0;
		String name;
		uint64_t start_ns = 0;
		uint64_t end_ns = 0;
		int depth = 0;
		Vector<int> children_ids;
	};

private:
	Ref<InsightsDatabase> database;
	Vector<FlameNode> flame_nodes;
	int stack_depth = 0;
	bool color_by_channel = true;
	String search_query;
	Dictionary selected_node;

	void _build_flame_tree();

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	void set_database(const Ref<InsightsDatabase> &p_db);
	Ref<InsightsDatabase> get_database() const;

	Array get_root_nodes() const;
	Array get_children(int p_node_id) const;

	void set_search_query(const String &p_query);
	String get_search_query() const;

	Array get_filtered_nodes() const;
	Dictionary get_selected_node() const;

	InsightsFlamegraph();
};
