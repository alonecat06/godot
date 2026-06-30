/**************************************************************************/
/*  cpu_channel.cpp                                                       */
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

#include "cpu_channel.h"

#include "core/object/class_db.h"
#include "core/variant/binder_common.h"
#include "core/variant/typed_array.h"
#include "core/os/os.h"

void CPUChannel::enable_contention_tracking() {
	contention_tracking_enabled = true;
}

void CPUChannel::disable_contention_tracking() {
	contention_tracking_enabled = false;
	contention_events.clear();
	active_locks.clear();
	pending_attempts.clear();
}

bool CPUChannel::is_contention_tracking_enabled() const {
	return contention_tracking_enabled;
}

void CPUChannel::on_lock_acquire(const String &p_lock_name, uint64_t p_thread_id, uint64_t p_timestamp_ns) {
	if (!contention_tracking_enabled) {
		return;
	}

	uint64_t timestamp_ns = p_timestamp_ns;
	if (timestamp_ns == 0) {
		timestamp_ns = OS::get_singleton()->get_ticks_usec() * 1000;
	}

	ActiveLock lock;
	lock.lock_name = p_lock_name;
	lock.owner_thread = p_thread_id;
	lock.acquire_ns = timestamp_ns;
	active_locks.push_back(lock);

	// Check pending_attempts for matching lock_name.
	for (uint32_t i = 0; i < pending_attempts.size(); i++) {
		if (pending_attempts[i].lock_name == p_lock_name) {
			if (contention_events.size() < max_contention_events) {
				ContentionEvent event;
				event.lock_name = p_lock_name;
				event.owner_thread = p_thread_id;
				event.waiter_thread = pending_attempts[i].waiter_thread;
				event.wait_time_ns = timestamp_ns - pending_attempts[i].attempt_ns;
				event.acquire_ns = timestamp_ns;
				contention_events.push_back(event);
			}
			pending_attempts.remove_at(i);
			break;
		}
	}
}

void CPUChannel::on_lock_attempt(const String &p_lock_name, uint64_t p_thread_id, uint64_t p_timestamp_ns) {
	if (!contention_tracking_enabled) {
		return;
	}

	uint64_t timestamp_ns = p_timestamp_ns;
	if (timestamp_ns == 0) {
		timestamp_ns = OS::get_singleton()->get_ticks_usec() * 1000;
	}

	PendingAttempt attempt;
	attempt.lock_name = p_lock_name;
	attempt.waiter_thread = p_thread_id;
	attempt.attempt_ns = timestamp_ns;
	pending_attempts.push_back(attempt);
}

void CPUChannel::on_lock_release(const String &p_lock_name, uint64_t p_thread_id, uint64_t p_timestamp_ns) {
	if (!contention_tracking_enabled) {
		return;
	}

	uint64_t timestamp_ns = p_timestamp_ns;
	if (timestamp_ns == 0) {
		timestamp_ns = OS::get_singleton()->get_ticks_usec() * 1000;
	}

	// Remove from active_locks (find matching lock_name + thread_id).
	for (uint32_t i = 0; i < active_locks.size(); i++) {
		if (active_locks[i].lock_name == p_lock_name && active_locks[i].owner_thread == p_thread_id) {
			active_locks.remove_at(i);
			break;
		}
	}
}

TypedArray<Dictionary> CPUChannel::get_contention_events() const {
	TypedArray<Dictionary> result;

	for (uint32_t i = 0; i < contention_events.size(); i++) {
		const ContentionEvent &event = contention_events[i];
		Dictionary dict;
		dict["lock_name"] = event.lock_name;
		dict["owner_thread"] = event.owner_thread;
		dict["waiter_thread"] = event.waiter_thread;
		dict["wait_time_ns"] = event.wait_time_ns;
		dict["acquire_ns"] = event.acquire_ns;
		dict["release_ns"] = event.release_ns;
		result.push_back(dict);
	}

	return result;
}

void CPUChannel::on_event(const Dictionary &p_event_data) {
	if (!p_event_data.has("type")) {
		return;
	}

	String type = p_event_data["type"];

	if (type == "lock_acquire") {
		String lock_name = p_event_data.get("lock_name", "");
		uint64_t thread_id = p_event_data.get("thread_id", (uint64_t)0);
		uint64_t timestamp_ns = p_event_data.get("timestamp_ns", (uint64_t)0);
		on_lock_acquire(lock_name, thread_id, timestamp_ns);
	} else if (type == "lock_attempt") {
		String lock_name = p_event_data.get("lock_name", "");
		uint64_t thread_id = p_event_data.get("thread_id", (uint64_t)0);
		uint64_t timestamp_ns = p_event_data.get("timestamp_ns", (uint64_t)0);
		on_lock_attempt(lock_name, thread_id, timestamp_ns);
	} else if (type == "lock_release") {
		String lock_name = p_event_data.get("lock_name", "");
		uint64_t thread_id = p_event_data.get("thread_id", (uint64_t)0);
		uint64_t timestamp_ns = p_event_data.get("timestamp_ns", (uint64_t)0);
		on_lock_release(lock_name, thread_id, timestamp_ns);
	}
}

Dictionary CPUChannel::serialize() {
	Dictionary dict;
	dict["contention_event_count"] = contention_events.size();
	return dict;
}

void CPUChannel::_bind_methods() {
	ClassDB::bind_method(D_METHOD("enable_contention_tracking"), &CPUChannel::enable_contention_tracking);
	ClassDB::bind_method(D_METHOD("disable_contention_tracking"), &CPUChannel::disable_contention_tracking);
	ClassDB::bind_method(D_METHOD("is_contention_tracking_enabled"), &CPUChannel::is_contention_tracking_enabled);

	ClassDB::bind_method(D_METHOD("on_lock_acquire", "lock_name", "thread_id", "timestamp_ns"), &CPUChannel::on_lock_acquire, DEFVAL((uint64_t)0));
	ClassDB::bind_method(D_METHOD("on_lock_attempt", "lock_name", "thread_id", "timestamp_ns"), &CPUChannel::on_lock_attempt, DEFVAL((uint64_t)0));
	ClassDB::bind_method(D_METHOD("on_lock_release", "lock_name", "thread_id", "timestamp_ns"), &CPUChannel::on_lock_release, DEFVAL((uint64_t)0));

	ClassDB::bind_method(D_METHOD("get_contention_events"), &CPUChannel::get_contention_events);
}

CPUChannel::CPUChannel() {
	set_category(CHANNEL_CATEGORY_CPU);
	set_color(Color(0x7A / 255.0f, 0xC0 / 255.0f, 0xE5 / 255.0f));
	set_name("cpu");
}

CPUChannel::~CPUChannel() {
}
