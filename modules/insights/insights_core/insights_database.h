/**************************************************************************/
/*  insights_database.h                                                   */
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

#pragma once

#include "core/io/file_access.h"
#include "core/object/ref_counted.h"
#include "core/string/ustring.h"
#include "core/templates/local_vector.h"
#include "core/templates/hash_map.h"

class InsightsDatabase : public RefCounted {
	GDCLASS(InsightsDatabase, RefCounted);

public:
	struct ZoneRecord {
		String name;
		String file;
		String function;
		int line = 0;
		String channel;
		uint64_t thread_id = 0;
		uint64_t start_ns = 0;
		uint64_t end_ns = 0;
		int depth = 0;
		int parent_zone_id = -1;
	};

	struct FrameMarker {
		int frame_index = 0;
		uint64_t start_ns = 0;
		uint64_t end_ns = 0;
	};

	struct AllocationRecord {
		uint64_t ptr = 0;
		uint64_t size = 0;
		int site_zone_id = -1;
		uint64_t alloc_ns = 0;
		uint64_t free_ns = 0;
		uint64_t thread_id = 0;
	};

	struct GPUZoneRecord {
		String name;
		uint64_t queue_id = 0;
		uint64_t submit_ns = 0;
		uint64_t start_ns = 0;
		uint64_t end_ns = 0;
		int context_id = 0;
	};

	struct ResourceLoadRecord {
		String path;
		String loader;
		uint64_t start_ns = 0;
		uint64_t end_ns = 0;
		uint64_t size_bytes = 0;
		String parent_path;
		uint64_t thread_id = 0;
	};

	struct MessageRecord {
		int level = 0;
		String text;
		uint64_t timestamp_ns = 0;
		int zone_id = -1;
	};

private:
	String file_path;
	bool is_open = false;

	LocalVector<ZoneRecord> zones;
	LocalVector<FrameMarker> frame_markers;
	LocalVector<AllocationRecord> allocations;
	LocalVector<GPUZoneRecord> gpu_zones;
	LocalVector<ResourceLoadRecord> resource_loads;
	LocalVector<MessageRecord> messages;

	HashMap<String, LocalVector<uint32_t>> zone_name_index;

protected:
	static void _bind_methods();

public:
	Error open(const String &p_path);
	void close();
	bool is_database_open() const;

	void create_tables(); // No-op for in-memory, kept for API compatibility.

	void insert_zone(const String &p_name, const String &p_file, int p_line, const String &p_function, const String &p_channel, uint64_t p_thread_id, uint64_t p_start_ns, uint64_t p_end_ns, int p_depth, int p_parent_zone_id);
	void insert_frame_marker(int p_frame_index, uint64_t p_start_ns, uint64_t p_end_ns);
	void insert_allocation(uint64_t p_ptr, uint64_t p_size, int p_site_zone_id, uint64_t p_alloc_ns, uint64_t p_free_ns, uint64_t p_thread_id);
	void insert_gpu_zone(const String &p_name, uint64_t p_queue_id, uint64_t p_submit_ns, uint64_t p_start_ns, uint64_t p_end_ns, int p_context_id);
	void insert_resource_load(const String &p_path, const String &p_loader, uint64_t p_start_ns, uint64_t p_end_ns, uint64_t p_size_bytes, const String &p_parent_path, uint64_t p_thread_id);
	void insert_message(int p_level, const String &p_text, uint64_t p_timestamp_ns, int p_zone_id);

	Array query_zone(const String &p_name, uint64_t p_start_ns, uint64_t p_end_ns) const;
	Array query_zones_in_range(uint64_t p_start_ns, uint64_t p_end_ns) const;
	Array query_frame_markers(uint64_t p_start_ns, uint64_t p_end_ns) const;
	Array query_allocations_in_range(uint64_t p_start_ns, uint64_t p_end_ns) const;
	Array query_resource_loads_in_range(uint64_t p_start_ns, uint64_t p_end_ns) const;
	Array query_gpu_zones_for_cpu_zone(uint32_t p_cpu_zone_id) const;
	Array query_gpu_zones_in_range(uint64_t p_start_ns, uint64_t p_end_ns) const;

	Array query_zones_by_depth(int p_depth) const;
	Array query_allocations_by_size(uint64_t p_min_size) const;
	Array query_resource_loads() const;
	Array query_resource_dependencies(const String &p_path) const;
	uint64_t get_peak_memory() const;
	Array get_leaked_allocations() const;
	uint64_t get_total_duration_ns() const;

	uint32_t get_zone_count() const;
	uint32_t get_frame_marker_count() const;
	uint32_t get_allocation_count() const;

	Error save_to_file(const String &p_path) const;
	Error load_from_file(const String &p_path);

	void clear();

	InsightsDatabase();
	~InsightsDatabase();
};
