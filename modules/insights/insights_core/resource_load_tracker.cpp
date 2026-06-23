/**************************************************************************/
/*  resource_load_tracker.cpp                                             */
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

#include "modules/insights/insights_core/resource_load_tracker.h"

#include "core/object/class_db.h"
#include "core/os/os.h"
#include "core/os/thread.h"
#include "core/variant/typed_array.h"

ResourceLoadTracker *ResourceLoadTracker::singleton = nullptr;

void ResourceLoadTracker::on_load_begin(const String &p_path, const String &p_loader) {
	LoadEvent ev;
	ev.path = p_path;
	ev.loader = p_loader;
	ev.start_ns = OS::get_singleton()->get_ticks_usec() * 1000;
	ev.thread_id = Thread::get_caller_id();

	List<LoadEvent>::Element *e = events.push_back(ev);
	active_loads[p_path] = e;
}

void ResourceLoadTracker::on_load_end(const String &p_path, uint64_t p_size) {
	HashMap<String, List<LoadEvent>::Element *>::Iterator it = active_loads.find(p_path);
	if (it != active_loads.end()) {
		List<LoadEvent>::Element *e = it->value;
		e->get().end_ns = OS::get_singleton()->get_ticks_usec() * 1000;
		e->get().memory_size = p_size;
		active_loads.erase(p_path);
	}
}

void ResourceLoadTracker::on_load_fail(const String &p_path, const String &p_error) {
	HashMap<String, List<LoadEvent>::Element *>::Iterator it = active_loads.find(p_path);
	if (it != active_loads.end()) {
		List<LoadEvent>::Element *e = it->value;
		e->get().failed = true;
		e->get().error = p_error;
		e->get().end_ns = OS::get_singleton()->get_ticks_usec() * 1000;
		active_loads.erase(p_path);
	}
}

TypedArray<Dictionary> ResourceLoadTracker::get_events_in_range(uint64_t p_start_ns, uint64_t p_end_ns) const {
	TypedArray<Dictionary> result;

	for (const List<LoadEvent>::Element *e = events.front(); e; e = e->next()) {
		const LoadEvent &ev = e->get();
		if (ev.start_ns >= p_start_ns && ev.start_ns <= p_end_ns) {
			Dictionary d;
			d["path"] = ev.path;
			d["loader"] = ev.loader;
			d["start_ns"] = ev.start_ns;
			d["end_ns"] = ev.end_ns;
			d["memory_size"] = ev.memory_size;
			d["parent_stack"] = ev.parent_stack;
			d["thread_id"] = ev.thread_id;
			d["failed"] = ev.failed;
			d["error"] = ev.error;
			result.push_back(d);
		}
	}

	return result;
}

TypedArray<Dictionary> ResourceLoadTracker::get_active_loads() const {
	TypedArray<Dictionary> result;

	for (const KeyValue<String, List<LoadEvent>::Element *> &E : active_loads) {
		const LoadEvent &ev = E.value->get();
		Dictionary d;
		d["path"] = ev.path;
		d["loader"] = ev.loader;
		d["start_ns"] = ev.start_ns;
		d["end_ns"] = ev.end_ns;
		d["memory_size"] = ev.memory_size;
		d["parent_stack"] = ev.parent_stack;
		d["thread_id"] = ev.thread_id;
		d["failed"] = ev.failed;
		d["error"] = ev.error;
		result.push_back(d);
	}

	return result;
}

void ResourceLoadTracker::clear() {
	events.clear();
	active_loads.clear();
}

void ResourceLoadTracker::_bind_methods() {
	ClassDB::bind_method(D_METHOD("on_load_begin", "path", "loader"), &ResourceLoadTracker::on_load_begin);
	ClassDB::bind_method(D_METHOD("on_load_end", "path", "size"), &ResourceLoadTracker::on_load_end);
	ClassDB::bind_method(D_METHOD("on_load_fail", "path", "error"), &ResourceLoadTracker::on_load_fail);

	ClassDB::bind_method(D_METHOD("get_events_in_range", "start_ns", "end_ns"), &ResourceLoadTracker::get_events_in_range);
	ClassDB::bind_method(D_METHOD("get_active_loads"), &ResourceLoadTracker::get_active_loads);

	ClassDB::bind_method(D_METHOD("clear"), &ResourceLoadTracker::clear);
}

ResourceLoadTracker::ResourceLoadTracker() {
	singleton = this;
}

ResourceLoadTracker::~ResourceLoadTracker() {
	if (singleton == this) {
		singleton = nullptr;
	}
}
