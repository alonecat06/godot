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
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "tracy_converter.h"

#include "modules/insights/insights_core/insights_database.h"
#include "core/io/file_access.h"

bool TracyConverter::is_tracy_file(const String &p_path) const {
	// Tracy magic bytes: 0x89 0x54 0x52 0x41 0x43 0x59 0x0D 0x0A ("\x89TRACY\r\n")
	static const uint8_t tracy_magic[8] = { 0x89, 0x54, 0x52, 0x41, 0x43, 0x59, 0x0D, 0x0A };

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

	for (int i = 0; i < 8; i++) {
		if (header[i] != tracy_magic[i]) {
			return false;
		}
	}

	return true;
}

Error TracyConverter::gitracy_to_tracy(const String &p_gitracy_path, const String &p_tracy_path) const {
	Ref<InsightsDatabase> db;
	db.instantiate();
	Error err = db->load_from_file(p_gitracy_path);
	if (err != OK) {
		return err;
	}

	Ref<FileAccess> f = FileAccess::open(p_tracy_path, FileAccess::WRITE);
	if (f.is_null()) {
		return ERR_FILE_CANT_OPEN;
	}

	// Write Tracy magic header.
	static const uint8_t tracy_magic[8] = { 0x89, 0x54, 0x52, 0x41, 0x43, 0x59, 0x0D, 0x0A };
	f->store_buffer(tracy_magic, 8);

	// Write version (stub: version 0).
	f->store_32(0);

	// Write zone count.
	uint32_t zone_count = db->get_zone_count();
	f->store_32(zone_count);

	// Write zone data (stub: each zone as name length + name + start_ns + end_ns).
	Array zones = db->query_zones_in_range(0, UINT64_MAX);
	for (int i = 0; i < zones.size(); i++) {
		Dictionary zone = zones[i];
		String name = zone["name"];
		CharString name_utf8 = name.utf8();
		f->store_32(name_utf8.length());
		f->store_buffer((const uint8_t *)name_utf8.get_data(), name_utf8.length());
		f->store_64((uint64_t)zone["start_ns"]);
		f->store_64((uint64_t)zone["end_ns"]);
	}

	f->close();
	return OK;
}

Error TracyConverter::tracy_to_gitracy(const String &p_tracy_path, const String &p_gitracy_path) const {
	if (!is_tracy_file(p_tracy_path)) {
		return ERR_FILE_CORRUPT;
	}

	Ref<FileAccess> f = FileAccess::open(p_tracy_path, FileAccess::READ);
	if (f.is_null()) {
		return ERR_FILE_CANT_OPEN;
	}

	// Skip magic header (8 bytes).
	f->seek(8);

	// Read version.
	uint32_t version = f->get_32();

	// Read zone count.
	uint32_t zone_count = f->get_32();

	Ref<InsightsDatabase> db;
	db.instantiate();

	// Read zone data and insert into database.
	for (uint32_t i = 0; i < zone_count; i++) {
		uint32_t name_len = f->get_32();
		Vector<uint8_t> name_buf;
		name_buf.resize(name_len);
		f->get_buffer(name_buf.ptrw(), name_len);
		String name = String::utf8((const char *)name_buf.ptr(), name_len);
		uint64_t start_ns = f->get_64();
		uint64_t end_ns = f->get_64();

		db->insert_zone(name, "", 0, "", "", 0, start_ns, end_ns, 0, -1);
	}

	f->close();

	Error err = db->save_to_file(p_gitracy_path);
	return err;
}

void TracyConverter::_bind_methods() {
	ClassDB::bind_method(D_METHOD("gitracy_to_tracy", "gitracy_path", "tracy_path"), &TracyConverter::gitracy_to_tracy);
	ClassDB::bind_method(D_METHOD("tracy_to_gitracy", "tracy_path", "gitracy_path"), &TracyConverter::tracy_to_gitracy);
	ClassDB::bind_method(D_METHOD("is_tracy_file", "path"), &TracyConverter::is_tracy_file);
}
