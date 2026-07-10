/**************************************************************************/
/*  insights_message_panel.cpp                                            */
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

#include "modules/insights/editor/insights_message_panel.h"

#include "modules/insights/insights_core/insights_database.h"

InsightsMessagePanel::InsightsMessagePanel() {
	msg_list = memnew(ItemList);
	msg_list->set_same_column_width(true);
	msg_list->set_max_columns(1);
	msg_list->set_h_size_flags(SIZE_EXPAND_FILL);
	msg_list->set_v_size_flags(SIZE_EXPAND_FILL);
	add_child(msg_list);
}

InsightsMessagePanel::~InsightsMessagePanel() {
}

void InsightsMessagePanel::update_data(const Ref<InsightsDatabase> &p_db) {
	msg_list->clear();

	if (p_db.is_null()) {
		return;
	}

	Array messages = p_db->query_messages();
	for (int i = 0; i < messages.size(); i++) {
		Dictionary d = messages[i];
		uint64_t timestamp_ns = (uint64_t)(int64_t)d["timestamp_ns"];
		double time_ms = (double)timestamp_ns / 1e6;
		String text = d["text"];
		msg_list->add_item(vformat("[%.3f] %s", time_ms, text));
	}
}

void InsightsMessagePanel::_bind_methods() {
	ClassDB::bind_method(D_METHOD("update_data", "db"), &InsightsMessagePanel::update_data);
}
