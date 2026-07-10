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

#include "modules/insights/editor/insights_memory_panel.h"

#include "modules/insights/insights_core/insights_database.h"

InsightsMemoryPanel::InsightsMemoryPanel() {
	mem_tree = memnew(Tree);
	mem_tree->set_columns(5);
	mem_tree->set_column_titles_visible(true);
	mem_tree->set_column_title(0, TTR("Address"));
	mem_tree->set_column_title(1, TTR("Size"));
	mem_tree->set_column_title(2, TTR("Alloc Time (ms)"));
	mem_tree->set_column_title(3, TTR("Free Time (ms)"));
	mem_tree->set_column_title(4, TTR("Thread"));
	mem_tree->set_h_size_flags(SIZE_EXPAND_FILL);
	mem_tree->set_v_size_flags(SIZE_EXPAND_FILL);
	add_child(mem_tree);
}

InsightsMemoryPanel::~InsightsMemoryPanel() {
}

void InsightsMemoryPanel::update_data(const Ref<InsightsDatabase> &p_db) {
	mem_tree->clear();
	TreeItem *root = mem_tree->create_item();
	filtered_count = 0;

	if (p_db.is_null()) {
		return;
	}

	Array allocations = p_db->query_allocations_in_range(0, UINT64_MAX);
	for (int i = 0; i < allocations.size(); i++) {
		Dictionary d = allocations[i];

		uint64_t ptr = (uint64_t)(int64_t)d["ptr"];
		uint64_t size = (uint64_t)(int64_t)d["size"];
		uint64_t alloc_ns = (uint64_t)(int64_t)d["alloc_ns"];
		uint64_t free_ns = (uint64_t)(int64_t)d["free_ns"];
		uint64_t thread_id = (uint64_t)(int64_t)d["thread_id"];

		TreeItem *item = mem_tree->create_item(root);

		// Address: formatted as "0x%016llx".
		item->set_text(0, vformat("0x%016llx", (unsigned long long)ptr));

		// Size: human-readable (B/KB/MB).
		String size_str;
		if (size >= 1024 * 1024) {
			size_str = vformat("%.2f MB", (double)size / (1024.0 * 1024.0));
		} else if (size >= 1024) {
			size_str = vformat("%.2f KB", (double)size / 1024.0);
		} else {
			size_str = vformat("%d B", (int64_t)size);
		}
		item->set_text(1, size_str);

		// Alloc Time: ns to ms.
		double alloc_ms = (double)alloc_ns / 1e6;
		item->set_text(2, vformat("%.3f", alloc_ms));

		// Free Time: ns to ms, "Active" if 0.
		if (free_ns == 0) {
			item->set_text(3, TTR("Active"));
		} else {
			double free_ms = (double)free_ns / 1e6;
			item->set_text(3, vformat("%.3f", free_ms));
		}

		// Thread.
		item->set_text(4, vformat("%d", (int64_t)thread_id));

		filtered_count++;
	}
}

void InsightsMemoryPanel::_bind_methods() {
	ClassDB::bind_method(D_METHOD("update_data", "db"), &InsightsMemoryPanel::update_data);
}
