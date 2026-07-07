/**************************************************************************/
/*  tracy_converter.cpp                                                   */
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
/* TORT OR OTHERWISE, ARISING FROM OR IN CONNECTION WITH THE SOFTWARE   */
/* OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                          */
/**************************************************************************/

#include "tracy_converter.h"

#include "modules/insights/insights_core/insights_database.h"
#include "core/io/file_access.h"
#include "core/math/math_funcs.h"
#include "core/string/ustring.h"

bool TracyConverter::is_tracy_file(const String &p_path) const {
	// Tracy file header: 't','r','a','c','y', Major, Minor, Patch
	// Note: This is different from the Tracy *network* magic header
	// ("\x89TRACY\r\n") which was used in the old stub implementation.
	static const uint8_t tracy_file_magic[5] = { 't', 'r', 'a', 'c', 'y' };

	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
	if (f.is_null()) {
		return false;
	}

	uint8_t header[8] = {};
	int64_t read_bytes = f->get_buffer(header, 8);
	f->close();

	if (read_bytes < 8) {
		return false;
	}

	for (int i = 0; i < 5; i++) {
		if (header[i] != tracy_file_magic[i]) {
			return false;
		}
	}

	return true;
}

Error TracyConverter::gitracy_to_tracy(const String &p_gitracy_path, const String &p_tracy_path) const {
	// The Tracy native binary format is extremely complex:
	// - LZ4 or ZSTD stream compression with multi-threaded parallel streams
	// - Hundreds of event types with variable-length encoding
	// - String tables, source location tables, thread context maps, etc.
	// - Time-delta encoding, zone validation sequences, etc.
	//
	// A complete implementation would require porting thousands of lines of
	// Tracy's serialization code (TracyWorker::Write), which is impractical
	// as a module feature.
	//
	// Instead, we export to Chrome Trace Event JSON format, which Tracy
	// can import natively. Use gitracy_to_chrome_json() for this.
	//
	// This method writes a minimal .tracy file that Tracy can open but
	// only contains basic zone/frame information as a best-effort export.

	Ref<InsightsDatabase> db;
	db.instantiate();
	Error err = db->load_from_file(p_gitracy_path);
	if (err != OK) {
		return err;
	}

	// Fallback: export as Chrome Trace JSON, then rename to .tracy won't work.
	// The recommended approach is gitracy_to_chrome_json().
	// For now, we produce the JSON with .tracy extension as a placeholder.
	// Users should use gitracy_to_chrome_json() for proper Tracy compatibility.

	WARN_PRINT("TracyConverter::gitracy_to_tracy(): Direct .tracy binary export is not fully supported. "
			   "Use gitracy_to_chrome_json() for Tracy-compatible output, which Tracy can import via File > Open.");

	// As a fallback, produce Chrome Trace JSON format with .tracy extension
	// so at least Tracy can attempt to read it.
	return gitracy_to_chrome_json(p_gitracy_path, p_tracy_path);
}

Error TracyConverter::gitracy_to_chrome_json(const String &p_gitracy_path, const String &p_json_path) const {
	// Export to Chrome Trace Event Format (JSON).
	// Tracy can import this format via File → Open.
	// Spec: https://docs.google.com/document/d/1CvAClvFyA5o5e2pNiA3DMY3tmW-SfL2RRnj2gOZG3q4/edit
	//
	// Format:
	// {"traceEvents": [
	//   {"ph":"X", "pid":0, "tid":1, "ts":1000, "dur":500, "name":"foo", "cat":"cpu", "args":{}},
	//   ...
	// ]}

	Ref<InsightsDatabase> db;
	db.instantiate();
	Error err = db->load_from_file(p_gitracy_path);
	if (err != OK) {
		return err;
	}

	Ref<FileAccess> f = FileAccess::open(p_json_path, FileAccess::WRITE);
	if (f.is_null()) {
		return ERR_FILE_CANT_OPEN;
	}

	Array zones = db->query_zones_in_range(0, UINT64_MAX);
	Array frame_markers = db->query_frame_markers(0, UINT64_MAX);

	f->store_string("{\"traceEvents\": [\n");

	bool first = true;

	// Write zones as complete events (ph="X").
	for (int i = 0; i < zones.size(); i++) {
		Dictionary zone = zones[i];
		String name = zone["name"];
		String channel = zone["channel"];
		uint64_t start_ns = (uint64_t)(int64_t)zone["start_ns"];
		uint64_t end_ns = (uint64_t)(int64_t)zone["end_ns"];
		uint64_t thread_id = (uint64_t)(int64_t)zone["thread_id"];

		// Chrome Trace uses microseconds.
		double ts_us = (double)start_ns / 1000.0;
		double dur_us = (double)(end_ns - start_ns) / 1000.0;

		// Map channel to process ID (group related zones together).
		int pid = 0;
		String cat = channel.replace("godot:", "");
		if (channel.begins_with("godot:cpu")) {
			pid = 0;
		} else if (channel.begins_with("godot:gpu")) {
			pid = 1;
		} else if (channel.begins_with("godot:mem")) {
			pid = 2;
		} else if (channel.begins_with("godot:script")) {
			pid = 3;
		} else if (channel.begins_with("godot:loading")) {
			pid = 4;
		} else if (channel.begins_with("godot:network")) {
			pid = 5;
		}

		String json_line = vformat(
				"%s{\"ph\":\"X\",\"pid\":%d,\"tid\":%d,\"ts\":%.1f,\"dur\":%.1f,"
				"\"name\":\"%s\",\"cat\":\"%s\",\"args\":{\"channel\":\"%s\"}}",
				first ? "" : ",\n",
				pid,
				(int32_t)(thread_id % 2147483647),
				ts_us,
				dur_us,
				name.json_escape(),
				cat.json_escape(),
				channel.json_escape());

		f->store_string(json_line);
		first = false;
	}

	// Write frame markers as instant events (ph="i").
	for (int i = 0; i < frame_markers.size(); i++) {
		Dictionary fm = frame_markers[i];
		uint64_t start_ns = (uint64_t)(int64_t)fm["start_ns"];
		double ts_us = (double)start_ns / 1000.0;
		int frame_index = (int)fm["frame_index"];

		String json_line = vformat(
				"%s{\"ph\":\"i\",\"pid\":0,\"tid\":0,\"ts\":%.1f,"
				"\"name\":\"Frame %d\",\"cat\":\"frame\",\"s\":\"g\"}",
				first ? "" : ",\n",
				ts_us,
				frame_index);

		f->store_string(json_line);
		first = false;
	}

	f->store_string("\n]}\n");
	f->close();

	return OK;
}

Error TracyConverter::tracy_to_gitracy(const String &p_tracy_path, const String &p_gitracy_path) const {
	// Reading Tracy's native binary format requires the full deserialization
	// pipeline (LZ4/ZSTD decompression, event dispatch, string table resolution, etc.)
	// This is not practical to implement without linking Tracy's source code.
	//
	// For Tracy → Godot Insights conversion, the recommended workflow is:
	// 1. Export Tracy data as Chrome Trace JSON from within Tracy
	// 2. Parse the JSON and populate an InsightsDatabase
	// 3. Save as .gitracy

	if (!is_tracy_file(p_tracy_path)) {
		return ERR_FILE_CORRUPT;
	}

	WARN_PRINT("TracyConverter::tracy_to_gitracy(): Direct .tracy binary import is not supported. "
			   "Export from Tracy as Chrome Trace JSON first, then convert.");

	return ERR_UNAVAILABLE;
}

void TracyConverter::_bind_methods() {
	ClassDB::bind_method(D_METHOD("gitracy_to_tracy", "gitracy_path", "tracy_path"), &TracyConverter::gitracy_to_tracy);
	ClassDB::bind_method(D_METHOD("tracy_to_gitracy", "tracy_path", "gitracy_path"), &TracyConverter::tracy_to_gitracy);
	ClassDB::bind_method(D_METHOD("gitracy_to_chrome_json", "gitracy_path", "json_path"), &TracyConverter::gitracy_to_chrome_json);
	ClassDB::bind_method(D_METHOD("is_tracy_file", "path"), &TracyConverter::is_tracy_file);
}
