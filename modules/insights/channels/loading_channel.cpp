/**************************************************************************/
/*  loading_channel.cpp                                                   */
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

#include "modules/insights/channels/loading_channel.h"

#include "core/object/class_db.h"
#include "core/variant/typed_array.h"
#include "core/os/os.h"
#include "core/os/thread.h"

void LoadingChannel::on_load_begin(const String &p_path, const String &p_loader, uint64_t p_timestamp_ns) {
	// If already at capacity, remove the oldest event.
	if (load_events.size() >= max_events) {
		// Remove from active_loads if it references the oldest index.
		const LoadEvent &oldest = load_events[0];
		active_loads.erase(oldest.path);
		load_events.remove_at(0);
		// Re-index active_loads (all indices shifted down by 1).
		for (KeyValue<String, uint32_t> &E : active_loads) {
			E.value -= 1;
		}
	}

	LoadEvent ev;
	ev.path = p_path;
	ev.loader = p_loader;
	ev.start_ns = (p_timestamp_ns == 0) ? (uint64_t)OS::get_singleton()->get_ticks_usec() * 1000 : p_timestamp_ns;
	ev.thread_id = (uint64_t)Thread::get_caller_id();

	uint32_t idx = load_events.size();
	load_events.push_back(ev);
	active_loads[p_path] = idx;
}

void LoadingChannel::on_load_end(const String &p_path, uint64_t p_size_bytes, uint64_t p_timestamp_ns) {
	if (!active_loads.has(p_path)) {
		return;
	}
	uint32_t idx = active_loads[p_path];
	LoadEvent &ev = load_events[idx];
	ev.end_ns = (p_timestamp_ns == 0) ? (uint64_t)OS::get_singleton()->get_ticks_usec() * 1000 : p_timestamp_ns;
	ev.size_bytes = p_size_bytes;
	active_loads.erase(p_path);
}

void LoadingChannel::on_load_fail(const String &p_path, const String &p_error, uint64_t p_timestamp_ns) {
	if (!active_loads.has(p_path)) {
		return;
	}
	uint32_t idx = active_loads[p_path];
	LoadEvent &ev = load_events[idx];
	ev.failed = true;
	ev.error = p_error;
	ev.end_ns = (p_timestamp_ns == 0) ? (uint64_t)OS::get_singleton()->get_ticks_usec() * 1000 : p_timestamp_ns;
	active_loads.erase(p_path);
}

TypedArray<Dictionary> LoadingChannel::get_load_events_in_range(uint64_t p_start_ns, uint64_t p_end_ns) const {
	TypedArray<Dictionary> result;
	for (uint32_t i = 0; i < load_events.size(); i++) {
		const LoadEvent &ev = load_events[i];
		// Include events whose time range overlaps with the query range.
		if (ev.start_ns <= p_end_ns && (ev.end_ns == 0 || ev.end_ns >= p_start_ns)) {
			Dictionary d;
			d["path"] = ev.path;
			d["loader"] = ev.loader;
			d["start_ns"] = ev.start_ns;
			d["end_ns"] = ev.end_ns;
			d["size_bytes"] = ev.size_bytes;
			d["parent_path"] = ev.parent_path;
			d["thread_id"] = ev.thread_id;
			d["failed"] = ev.failed;
			d["error"] = ev.error;
			result.push_back(d);
		}
	}
	return result;
}

TypedArray<Dictionary> LoadingChannel::get_active_loads() const {
	TypedArray<Dictionary> result;
	for (const KeyValue<String, uint32_t> &E : active_loads) {
		uint32_t idx = E.value;
		if (idx < load_events.size()) {
			const LoadEvent &ev = load_events[idx];
			Dictionary d;
			d["path"] = ev.path;
			d["loader"] = ev.loader;
			d["start_ns"] = ev.start_ns;
			d["parent_path"] = ev.parent_path;
			d["thread_id"] = ev.thread_id;
			result.push_back(d);
		}
	}
	return result;
}

uint32_t LoadingChannel::get_event_count() const {
	return load_events.size();
}

void LoadingChannel::set_max_events(uint32_t p_max) {
	max_events = p_max;
}

uint32_t LoadingChannel::get_max_events() const {
	return max_events;
}

void LoadingChannel::on_event(const Dictionary &p_event_data) {
	String type = p_event_data.get("type", "");
	if (type == "load_begin") {
		String path = p_event_data.get("path", "");
		String loader = p_event_data.get("loader", "");
		uint64_t timestamp = p_event_data.get("timestamp_ns", (uint64_t)0);
		on_load_begin(path, loader, timestamp);
	} else if (type == "load_end") {
		String path = p_event_data.get("path", "");
		uint64_t size_bytes = p_event_data.get("size_bytes", (uint64_t)0);
		uint64_t timestamp = p_event_data.get("timestamp_ns", (uint64_t)0);
		on_load_end(path, size_bytes, timestamp);
	} else if (type == "load_fail") {
		String path = p_event_data.get("path", "");
		String error = p_event_data.get("error", "");
		uint64_t timestamp = p_event_data.get("timestamp_ns", (uint64_t)0);
		on_load_fail(path, error, timestamp);
	}
}

Dictionary LoadingChannel::serialize() {
	Dictionary dict = InsightsChannel::serialize();
	dict["event_count"] = load_events.size();
	dict["active_load_count"] = active_loads.size();
	return dict;
}

void LoadingChannel::_bind_methods() {
	ClassDB::bind_method(D_METHOD("on_load_begin", "path", "loader", "timestamp_ns"), &LoadingChannel::on_load_begin, DEFVAL((uint64_t)0));
	ClassDB::bind_method(D_METHOD("on_load_end", "path", "size_bytes", "timestamp_ns"), &LoadingChannel::on_load_end, DEFVAL((uint64_t)0));
	ClassDB::bind_method(D_METHOD("on_load_fail", "path", "error", "timestamp_ns"), &LoadingChannel::on_load_fail, DEFVAL((uint64_t)0));

	ClassDB::bind_method(D_METHOD("get_load_events_in_range", "start_ns", "end_ns"), &LoadingChannel::get_load_events_in_range);
	ClassDB::bind_method(D_METHOD("get_active_loads"), &LoadingChannel::get_active_loads);
	ClassDB::bind_method(D_METHOD("get_event_count"), &LoadingChannel::get_event_count);

	ClassDB::bind_method(D_METHOD("set_max_events", "max"), &LoadingChannel::set_max_events);
	ClassDB::bind_method(D_METHOD("get_max_events"), &LoadingChannel::get_max_events);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "max_events"), "set_max_events", "get_max_events");
}

LoadingChannel::LoadingChannel() {
	set_category(CHANNEL_CATEGORY_LOADING);
	set_color(Color(0xE5 / 255.0f, 0xC7 / 255.0f, 0x7A / 255.0f));
	set_name("loading");
}

LoadingChannel::~LoadingChannel() {
}
