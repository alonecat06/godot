/**************************************************************************/
/*  insights_contention_panel.cpp                                         */
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

#include "modules/insights/editor/insights_contention_panel.h"

#ifdef TOOLS_ENABLED

#include "core/object/class_db.h"

InsightsContentionPanel::InsightsContentionPanel() {
	contention_tree = memnew(Tree);
	contention_tree->set_columns(4);
	contention_tree->set_column_titles_visible(true);
	contention_tree->set_column_title(0, TTR("Lock Name"));
	contention_tree->set_column_title(1, TTR("Wait Time (ms)"));
	contention_tree->set_column_title(2, TTR("Owner Thread"));
	contention_tree->set_column_title(3, TTR("Waiter Thread"));
	contention_tree->set_h_size_flags(SIZE_EXPAND_FILL);
	contention_tree->set_v_size_flags(SIZE_EXPAND_FILL);
	add_child(contention_tree);

	top_contention_tree = memnew(Tree);
	top_contention_tree->set_columns(4);
	top_contention_tree->set_column_titles_visible(true);
	top_contention_tree->set_column_title(0, TTR("Lock Name"));
	top_contention_tree->set_column_title(1, TTR("Wait Time (ms)"));
	top_contention_tree->set_column_title(2, TTR("Owner Thread"));
	top_contention_tree->set_column_title(3, TTR("Waiter Thread"));
	top_contention_tree->set_h_size_flags(SIZE_EXPAND_FILL);
	top_contention_tree->set_v_size_flags(SIZE_EXPAND_FILL);
	add_child(top_contention_tree);
}

InsightsContentionPanel::~InsightsContentionPanel() {
}

void InsightsContentionPanel::_rebuild_contention_tree() {
	contention_tree->clear();
	TreeItem *root = contention_tree->create_item();

	if (database.is_null()) {
		return;
	}

	Array allocations = database->query_allocations_in_range(0, UINT64_MAX);
	for (int i = 0; i < allocations.size(); i++) {
		Dictionary d = allocations[i];
		TreeItem *item = contention_tree->create_item(root);
		item->set_text(0, vformat("Lock #%d", i));
		uint64_t alloc_ns = (uint64_t)(int64_t)d["alloc_ns"];
		uint64_t free_ns = (uint64_t)(int64_t)d["free_ns"];
		double wait_ms = 0.0;
		if (free_ns > alloc_ns) {
			wait_ms = (double)(free_ns - alloc_ns) / 1e6;
		}
		item->set_text(1, vformat("%.3f", wait_ms));
		item->set_text(2, vformat("Thread %d", (int64_t)d["thread_id"]));
		item->set_text(3, vformat("Thread (waiter)"));
	}
}

void InsightsContentionPanel::_rebuild_top_contention_tree() {
	top_contention_tree->clear();
	TreeItem *root = top_contention_tree->create_item();

	if (database.is_null()) {
		return;
	}

	Array allocations = database->query_allocations_in_range(0, UINT64_MAX);

	struct ContentionEntry {
		String lock_name;
		double wait_ms = 0.0;
		String owner_thread;
		String waiter_thread;
	};

	Vector<ContentionEntry> entries;
	for (int i = 0; i < allocations.size(); i++) {
		Dictionary d = allocations[i];
		ContentionEntry entry;
		entry.lock_name = vformat("Lock #%d", i);
		uint64_t alloc_ns = (uint64_t)(int64_t)d["alloc_ns"];
		uint64_t free_ns = (uint64_t)(int64_t)d["free_ns"];
		if (free_ns > alloc_ns) {
			entry.wait_ms = (double)(free_ns - alloc_ns) / 1e6;
		}
		entry.owner_thread = vformat("Thread %d", (int64_t)d["thread_id"]);
		entry.waiter_thread = vformat("Thread (waiter)");
		entries.push_back(entry);
	}

	struct ContentionEntryComparator {
		bool operator()(const ContentionEntry &a, const ContentionEntry &b) const {
			return a.wait_ms > b.wait_ms;
		}
	};
	entries.sort_custom<ContentionEntryComparator>();

	for (int i = 0; i < entries.size(); i++) {
		TreeItem *item = top_contention_tree->create_item(root);
		item->set_text(0, entries[i].lock_name);
		item->set_text(1, vformat("%.3f", entries[i].wait_ms));
		item->set_text(2, entries[i].owner_thread);
		item->set_text(3, entries[i].waiter_thread);
	}
}

void InsightsContentionPanel::set_database(const Ref<InsightsDatabase> &p_database) {
	database = p_database;
	_rebuild_contention_tree();
	_rebuild_top_contention_tree();
}

Ref<InsightsDatabase> InsightsContentionPanel::get_database() const {
	return database;
}

void InsightsContentionPanel::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_database", "database"), &InsightsContentionPanel::set_database);
	ClassDB::bind_method(D_METHOD("get_database"), &InsightsContentionPanel::get_database);
}

#endif // TOOLS_ENABLED
