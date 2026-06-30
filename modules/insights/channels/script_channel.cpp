/**************************************************************************/
/*  script_channel.cpp                                                    */
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

#include "modules/insights/channels/script_channel.h"

#include "core/object/class_db.h"
#include "core/variant/binder_common.h"
#include "core/variant/typed_array.h"
#include "core/os/os.h"

void ScriptChannel::enter_function(const String &p_name, const String &p_file, int p_line, Language p_language) {
	if (call_records.size() >= max_records) {
		call_records.remove_at(0);
	}

	CallRecord rec;
	rec.function_name = p_name;
	rec.file = p_file;
	rec.line = p_line;
	rec.language = p_language;
	rec.start_ns = OS::get_singleton()->get_ticks_usec() * 1000;
	rec.end_ns = 0;
	rec.depth = current_depth;

	call_records.push_back(rec);
	current_depth++;
}

void ScriptChannel::leave_function() {
	if (current_depth > 0) {
		current_depth--;
	}
	if (!call_records.is_empty() && call_records[call_records.size() - 1].end_ns == 0) {
		call_records[call_records.size() - 1].end_ns = OS::get_singleton()->get_ticks_usec() * 1000;
	}
}

void ScriptChannel::suspend_function(const String &p_name) {
	if (call_records.is_empty()) {
		return;
	}
	// Mark the most recent call record for this function as suspended.
	for (int i = call_records.size() - 1; i >= 0; i--) {
		if (call_records[i].function_name == p_name && call_records[i].end_ns == 0) {
			call_records[i].was_suspended = true;
			call_records[i].suspend_ns = OS::get_singleton()->get_ticks_usec() * 1000;
			break;
		}
	}
}

void ScriptChannel::resume_function(const String &p_name) {
	if (call_records.is_empty()) {
		return;
	}
	for (int i = call_records.size() - 1; i >= 0; i--) {
		if (call_records[i].function_name == p_name && call_records[i].was_suspended && call_records[i].resume_ns == 0) {
			call_records[i].resume_ns = OS::get_singleton()->get_ticks_usec() * 1000;
			break;
		}
	}
}

void ScriptChannel::on_gc_event(int p_generation, int p_objects_collected, uint64_t p_timestamp_ns) {
	GCEvent ev;
	ev.generation = p_generation;
	ev.objects_collected = p_objects_collected;
	ev.timestamp_ns = (p_timestamp_ns == 0) ? OS::get_singleton()->get_ticks_usec() * 1000 : p_timestamp_ns;
	gc_events.push_back(ev);
}

TypedArray<Dictionary> ScriptChannel::get_call_records() const {
	TypedArray<Dictionary> arr;
	arr.resize(call_records.size());
	for (uint32_t i = 0; i < call_records.size(); i++) {
		Dictionary d;
		d["function_name"] = call_records[i].function_name;
		d["file"] = call_records[i].file;
		d["line"] = call_records[i].line;
		d["language"] = call_records[i].language;
		d["start_ns"] = call_records[i].start_ns;
		d["end_ns"] = call_records[i].end_ns;
		d["depth"] = call_records[i].depth;
		d["was_suspended"] = call_records[i].was_suspended;
		d["suspend_ns"] = (int64_t)call_records[i].suspend_ns;
		d["resume_ns"] = (int64_t)call_records[i].resume_ns;
		arr[i] = d;
	}
	return arr;
}

TypedArray<Dictionary> ScriptChannel::get_gc_events() const {
	TypedArray<Dictionary> arr;
	arr.resize(gc_events.size());
	for (uint32_t i = 0; i < gc_events.size(); i++) {
		Dictionary d;
		d["timestamp_ns"] = gc_events[i].timestamp_ns;
		d["generation"] = gc_events[i].generation;
		d["objects_collected"] = gc_events[i].objects_collected;
		arr[i] = d;
	}
	return arr;
}

void ScriptChannel::set_max_records(uint32_t p_max) {
	max_records = p_max;
}

uint32_t ScriptChannel::get_max_records() const {
	return max_records;
}

int ScriptChannel::get_current_depth() const {
	return current_depth;
}

void ScriptChannel::on_event(const Dictionary &p_event_data) {
	if (!p_event_data.has("type")) {
		return;
	}
	String type = p_event_data["type"];
	if (type == "enter_function") {
		String fn_name = p_event_data.get("function_name", "");
		String file = p_event_data.get("file", "");
		int line = p_event_data.get("line", 0);
		Language lang = (Language)(int)p_event_data.get("language", LANGUAGE_GDSCRIPT);
		enter_function(fn_name, file, line, lang);
	} else if (type == "leave_function") {
		leave_function();
	} else if (type == "gc_event") {
		int gen = p_event_data.get("generation", 0);
		int collected = p_event_data.get("objects_collected", 0);
		uint64_t ts = p_event_data.get("timestamp_ns", (uint64_t)0);
		on_gc_event(gen, collected, ts);
	}
}

Dictionary ScriptChannel::serialize() {
	Dictionary dict;
	dict["call_record_count"] = (int)call_records.size();
	dict["gc_event_count"] = (int)gc_events.size();
	dict["current_depth"] = current_depth;
	return dict;
}

void ScriptChannel::_bind_methods() {
	ClassDB::bind_method(D_METHOD("enter_function", "name", "file", "line", "language"), &ScriptChannel::enter_function, DEFVAL(LANGUAGE_GDSCRIPT));
	ClassDB::bind_method(D_METHOD("leave_function"), &ScriptChannel::leave_function);
	ClassDB::bind_method(D_METHOD("suspend_function", "name"), &ScriptChannel::suspend_function);
	ClassDB::bind_method(D_METHOD("resume_function", "name"), &ScriptChannel::resume_function);
	ClassDB::bind_method(D_METHOD("on_gc_event", "generation", "objects_collected", "timestamp_ns"), &ScriptChannel::on_gc_event, DEFVAL((uint64_t)0));
	ClassDB::bind_method(D_METHOD("get_call_records"), &ScriptChannel::get_call_records);
	ClassDB::bind_method(D_METHOD("get_gc_events"), &ScriptChannel::get_gc_events);
	ClassDB::bind_method(D_METHOD("set_max_records", "max"), &ScriptChannel::set_max_records);
	ClassDB::bind_method(D_METHOD("get_max_records"), &ScriptChannel::get_max_records);
	ClassDB::bind_method(D_METHOD("get_current_depth"), &ScriptChannel::get_current_depth);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "max_records"), "set_max_records", "get_max_records");

	BIND_ENUM_CONSTANT(LANGUAGE_GDSCRIPT);
	BIND_ENUM_CONSTANT(LANGUAGE_C_SHARP);
}

ScriptChannel::ScriptChannel() {
	set_category(CHANNEL_CATEGORY_SCRIPT);
	set_color(Color(0x47 / 255.0f, 0x8C / 255.0f, 0xBF / 255.0f));
	set_name("script");
}

ScriptChannel::~ScriptChannel() {
}

VARIANT_ENUM_CAST(ScriptChannel::Language);
