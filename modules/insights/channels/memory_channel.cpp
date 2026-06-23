/**************************************************************************/
/*  memory_channel.cpp                                                     */
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

#include "memory_channel.h"

#include "core/object/class_db.h"
#include "core/variant/typed_array.h"

void MemoryChannel::on_alloc(uint64_t p_ptr, uint64_t p_size, uint64_t p_thread_id, uint64_t p_timestamp_ns) {
	if (p_ptr == 0) {
		return;
	}

	LeakInfo info;
	info.ptr = p_ptr;
	info.size = p_size;
	info.alloc_ns = p_timestamp_ns;
	info.thread_id = p_thread_id;

	active_allocations.insert(p_ptr, info);
	total_allocated += p_size;
	allocation_count++;
}

void MemoryChannel::on_free(uint64_t p_ptr, uint64_t p_thread_id, uint64_t p_timestamp_ns) {
	LeakInfo *info = active_allocations.getptr(p_ptr);
	if (info == nullptr) {
		return;
	}

	total_freed += info->size;
	allocation_count--;
	active_allocations.erase(p_ptr);
}

uint64_t MemoryChannel::get_total_allocated() const {
	return total_allocated;
}

uint64_t MemoryChannel::get_allocation_count() const {
	return allocation_count;
}

uint64_t MemoryChannel::get_total_freed() const {
	return total_freed;
}

TypedArray<Dictionary> MemoryChannel::detect_leaks() const {
	TypedArray<Dictionary> leaks;

	for (const KeyValue<uint64_t, LeakInfo> &E : active_allocations) {
		Dictionary dict;
		dict["ptr"] = E.value.ptr;
		dict["size"] = E.value.size;
		dict["alloc_ns"] = E.value.alloc_ns;
		dict["thread_id"] = E.value.thread_id;
		leaks.push_back(dict);
	}

	return leaks;
}

void MemoryChannel::on_event(const Dictionary &p_event_data) {
	if (!p_event_data.has("type")) {
		return;
	}

	String type = p_event_data["type"];

	if (type == "alloc") {
		uint64_t ptr = p_event_data.get("ptr", (uint64_t)0);
		uint64_t size = p_event_data.get("size", (uint64_t)0);
		uint64_t thread_id = p_event_data.get("thread_id", (uint64_t)0);
		uint64_t timestamp_ns = p_event_data.get("timestamp_ns", (uint64_t)0);
		on_alloc(ptr, size, thread_id, timestamp_ns);
	} else if (type == "free") {
		uint64_t ptr = p_event_data.get("ptr", (uint64_t)0);
		uint64_t thread_id = p_event_data.get("thread_id", (uint64_t)0);
		uint64_t timestamp_ns = p_event_data.get("timestamp_ns", (uint64_t)0);
		on_free(ptr, thread_id, timestamp_ns);
	}
}

Dictionary MemoryChannel::serialize() {
	Dictionary dict;
	dict["total_allocated"] = total_allocated;
	dict["allocation_count"] = allocation_count;
	dict["total_freed"] = total_freed;
	dict["leak_count"] = active_allocations.size();
	return dict;
}

void MemoryChannel::_bind_methods() {
	ClassDB::bind_method(D_METHOD("on_alloc", "ptr", "size", "thread_id", "timestamp_ns"), &MemoryChannel::on_alloc);
	ClassDB::bind_method(D_METHOD("on_free", "ptr", "thread_id", "timestamp_ns"), &MemoryChannel::on_free);

	ClassDB::bind_method(D_METHOD("get_total_allocated"), &MemoryChannel::get_total_allocated);
	ClassDB::bind_method(D_METHOD("get_allocation_count"), &MemoryChannel::get_allocation_count);
	ClassDB::bind_method(D_METHOD("get_total_freed"), &MemoryChannel::get_total_freed);
	ClassDB::bind_method(D_METHOD("detect_leaks"), &MemoryChannel::detect_leaks);
}

MemoryChannel::MemoryChannel() {
	set_category(CHANNEL_CATEGORY_MEMORY);
	set_color(Color(0x7A / 255.0f, 0xC0 / 255.0f, 0xE5 / 255.0f));
	set_name("memory");
}

MemoryChannel::~MemoryChannel() {
}
