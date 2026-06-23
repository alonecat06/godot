/**************************************************************************/
/*  log_channel.cpp                                                       */
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

#include "modules/insights/channels/log_channel.h"

#include "core/object/class_db.h"
#include "core/variant/binder_common.h"
#include "core/variant/typed_array.h"
#include "core/os/os.h"

void LogChannel::log_message(Severity p_severity, const String &p_text, const String &p_file, int p_line, uint64_t p_timestamp_ns, int p_zone_id) {
	if (p_severity < severity_threshold) {
		return;
	}

	LogEntry entry;
	entry.severity = p_severity;
	entry.text = p_text;
	entry.file = p_file;
	entry.line = p_line;
	entry.timestamp_ns = (p_timestamp_ns == 0) ? (uint64_t)OS::get_singleton()->get_ticks_usec() * 1000 : p_timestamp_ns;
	entry.zone_id = p_zone_id;

	entries.push_back(entry);

	while (entries.size() > max_entries) {
		entries.remove_at(0);
	}
}

void LogChannel::set_severity_threshold(Severity p_threshold) {
	severity_threshold = p_threshold;
}

LogChannel::Severity LogChannel::get_severity_threshold() const {
	return severity_threshold;
}

void LogChannel::set_max_entries(uint32_t p_max) {
	max_entries = p_max;

	while (entries.size() > max_entries) {
		entries.remove_at(0);
	}
}

uint32_t LogChannel::get_max_entries() const {
	return max_entries;
}

TypedArray<Dictionary> LogChannel::get_messages_in_range(uint64_t p_start_ns, uint64_t p_end_ns, Severity p_min_severity) const {
	TypedArray<Dictionary> result;

	for (uint32_t i = 0; i < entries.size(); i++) {
		const LogEntry &entry = entries[i];
		if (entry.timestamp_ns < p_start_ns || entry.timestamp_ns > p_end_ns) {
			continue;
		}
		if (entry.severity < p_min_severity) {
			continue;
		}

		Dictionary dict;
		dict["severity"] = entry.severity;
		dict["text"] = entry.text;
		dict["file"] = entry.file;
		dict["line"] = entry.line;
		dict["timestamp_ns"] = entry.timestamp_ns;
		dict["zone_id"] = entry.zone_id;

		result.push_back(dict);
	}

	return result;
}

uint32_t LogChannel::get_entry_count() const {
	return entries.size();
}

void LogChannel::on_event(const Dictionary &p_event_data) {
	if (p_event_data.get("type", String()) == "log_message") {
		Severity severity = (Severity)(int)p_event_data.get("severity", SEVERITY_INFO);
		String text = p_event_data.get("text", String());
		String file = p_event_data.get("file", String());
		int line = (int)p_event_data.get("line", 0);
		uint64_t timestamp_ns = (uint64_t)p_event_data.get("timestamp_ns", (uint64_t)0);
		int zone_id = (int)p_event_data.get("zone_id", -1);

		log_message(severity, text, file, line, timestamp_ns, zone_id);
	}
}

Dictionary LogChannel::serialize() {
	Dictionary dict;
	dict["entry_count"] = get_entry_count();
	dict["severity_threshold"] = severity_threshold;
	return dict;
}

void LogChannel::_bind_methods() {
	ClassDB::bind_method(D_METHOD("log_message", "severity", "text", "file", "line", "timestamp_ns", "zone_id"), &LogChannel::log_message, DEFVAL(-1));
	ClassDB::bind_method(D_METHOD("set_severity_threshold", "threshold"), &LogChannel::set_severity_threshold);
	ClassDB::bind_method(D_METHOD("get_severity_threshold"), &LogChannel::get_severity_threshold);
	ClassDB::bind_method(D_METHOD("set_max_entries", "max"), &LogChannel::set_max_entries);
	ClassDB::bind_method(D_METHOD("get_max_entries"), &LogChannel::get_max_entries);
	ClassDB::bind_method(D_METHOD("get_messages_in_range", "start_ns", "end_ns", "min_severity"), &LogChannel::get_messages_in_range, DEFVAL(SEVERITY_VERBOSE));
	ClassDB::bind_method(D_METHOD("get_entry_count"), &LogChannel::get_entry_count);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "severity_threshold", PROPERTY_HINT_ENUM, "Verbose,Debug,Info,Warning,Error"), "set_severity_threshold", "get_severity_threshold");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "max_entries", PROPERTY_HINT_RANGE, "1,1000000"), "set_max_entries", "get_max_entries");

	BIND_ENUM_CONSTANT(SEVERITY_VERBOSE);
	BIND_ENUM_CONSTANT(SEVERITY_DEBUG);
	BIND_ENUM_CONSTANT(SEVERITY_INFO);
	BIND_ENUM_CONSTANT(SEVERITY_WARNING);
	BIND_ENUM_CONSTANT(SEVERITY_ERROR);
}

LogChannel::LogChannel() {
	set_category(CHANNEL_CATEGORY_LOG);
	set_color(Color(0xC0 / 255.0f, 0xC0 / 255.0f, 0xC0 / 255.0f));
	set_name("log");
}

LogChannel::~LogChannel() {
}

VARIANT_ENUM_CAST(LogChannel::Severity);
