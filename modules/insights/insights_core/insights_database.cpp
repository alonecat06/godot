/**************************************************************************/
/*  insights_database.cpp                                                 */
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

#include "insights_database.h"

Error InsightsDatabase::open(const String &p_path) {
	file_path = p_path;
	is_open = true;
	return OK;
}

void InsightsDatabase::close() {
	is_open = false;
}

bool InsightsDatabase::is_database_open() const {
	return is_open;
}

void InsightsDatabase::create_tables() {
	// In-memory storage, no table creation needed.
}

void InsightsDatabase::insert_zone(const String &p_name, const String &p_file, int p_line, const String &p_function, const String &p_channel, uint64_t p_thread_id, uint64_t p_start_ns, uint64_t p_end_ns, int p_depth, int p_parent_zone_id) {
	ZoneRecord rec;
	rec.name = p_name;
	rec.file = p_file;
	rec.function = p_function;
	rec.line = p_line;
	rec.channel = p_channel;
	rec.thread_id = p_thread_id;
	rec.start_ns = p_start_ns;
	rec.end_ns = p_end_ns;
	rec.depth = p_depth;
	rec.parent_zone_id = p_parent_zone_id;

	uint32_t idx = zones.size();
	zones.push_back(rec);

	if (!zone_name_index.has(p_name)) {
		zone_name_index[p_name] = LocalVector<uint32_t>();
	}
	zone_name_index[p_name].push_back(idx);
}

void InsightsDatabase::update_zone_end(uint32_t p_zone_index, uint64_t p_end_ns) {
	if (p_zone_index < zones.size()) {
		zones[p_zone_index].end_ns = p_end_ns;
	}
}

void InsightsDatabase::insert_frame_marker(int p_frame_index, uint64_t p_start_ns, uint64_t p_end_ns) {
	FrameMarker rec;
	rec.frame_index = p_frame_index;
	rec.start_ns = p_start_ns;
	rec.end_ns = p_end_ns;
	frame_markers.push_back(rec);
}

void InsightsDatabase::insert_allocation(uint64_t p_ptr, uint64_t p_size, int p_site_zone_id, uint64_t p_alloc_ns, uint64_t p_free_ns, uint64_t p_thread_id) {
	AllocationRecord rec;
	rec.ptr = p_ptr;
	rec.size = p_size;
	rec.site_zone_id = p_site_zone_id;
	rec.alloc_ns = p_alloc_ns;
	rec.free_ns = p_free_ns;
	rec.thread_id = p_thread_id;
	allocations.push_back(rec);
}

void InsightsDatabase::insert_gpu_zone(const String &p_name, uint64_t p_queue_id, uint64_t p_submit_ns, uint64_t p_start_ns, uint64_t p_end_ns, int p_context_id) {
	GPUZoneRecord rec;
	rec.name = p_name;
	rec.queue_id = p_queue_id;
	rec.submit_ns = p_submit_ns;
	rec.start_ns = p_start_ns;
	rec.end_ns = p_end_ns;
	rec.context_id = p_context_id;
	gpu_zones.push_back(rec);
}

void InsightsDatabase::insert_resource_load(const String &p_path, const String &p_loader, uint64_t p_start_ns, uint64_t p_end_ns, uint64_t p_size_bytes, const String &p_parent_path, uint64_t p_thread_id) {
	ResourceLoadRecord rec;
	rec.path = p_path;
	rec.loader = p_loader;
	rec.start_ns = p_start_ns;
	rec.end_ns = p_end_ns;
	rec.size_bytes = p_size_bytes;
	rec.parent_path = p_parent_path;
	rec.thread_id = p_thread_id;
	resource_loads.push_back(rec);
}

void InsightsDatabase::insert_message(int p_level, const String &p_text, uint64_t p_timestamp_ns, int p_zone_id) {
	MessageRecord rec;
	rec.level = p_level;
	rec.text = p_text;
	rec.timestamp_ns = p_timestamp_ns;
	rec.zone_id = p_zone_id;
	messages.push_back(rec);
}

void InsightsDatabase::insert_plot_point(const String &p_plot_name, uint64_t p_time_ns, double p_value) {
	PlotPointRecord rec;
	rec.plot_name = p_plot_name;
	rec.time_ns = p_time_ns;
	rec.value = p_value;
	plot_points.push_back(rec);
}

void InsightsDatabase::insert_lock_event(uint64_t p_lock_id, int64_t p_srcloc, uint64_t p_time_ns, int p_type, uint64_t p_thread_id) {
	LockEventRecord rec;
	rec.lock_id = p_lock_id;
	rec.srcloc = p_srcloc;
	rec.time_ns = p_time_ns;
	rec.type = p_type;
	rec.thread_id = p_thread_id;
	lock_events.push_back(rec);
}

Array InsightsDatabase::query_zone(const String &p_name, uint64_t p_start_ns, uint64_t p_end_ns) const {
	Array result;
	HashMap<String, LocalVector<uint32_t>>::ConstIterator it = zone_name_index.find(p_name);
	if (it == zone_name_index.end()) {
		return result;
	}
	const LocalVector<uint32_t> &indices = it->value;
	for (uint32_t i = 0; i < indices.size(); i++) {
		const ZoneRecord &rec = zones[indices[i]];
		if (rec.start_ns >= p_start_ns && rec.start_ns <= p_end_ns) {
			Dictionary dict;
			dict["name"] = rec.name;
			dict["file"] = rec.file;
			dict["function"] = rec.function;
			dict["line"] = rec.line;
			dict["channel"] = rec.channel;
			dict["thread_id"] = (int64_t)rec.thread_id;
			dict["start_ns"] = (int64_t)rec.start_ns;
			dict["end_ns"] = (int64_t)rec.end_ns;
			dict["depth"] = rec.depth;
			dict["parent_zone_id"] = rec.parent_zone_id;
			result.push_back(dict);
		}
	}
	return result;
}

Array InsightsDatabase::query_zones_in_range(uint64_t p_start_ns, uint64_t p_end_ns) const {
	Array result;
	for (uint32_t i = 0; i < zones.size(); i++) {
		const ZoneRecord &rec = zones[i];
		if (rec.start_ns >= p_start_ns && rec.start_ns <= p_end_ns) {
			Dictionary dict;
			dict["name"] = rec.name;
			dict["file"] = rec.file;
			dict["function"] = rec.function;
			dict["line"] = rec.line;
			dict["channel"] = rec.channel;
			dict["thread_id"] = (int64_t)rec.thread_id;
			dict["start_ns"] = (int64_t)rec.start_ns;
			dict["end_ns"] = (int64_t)rec.end_ns;
			dict["depth"] = rec.depth;
			dict["parent_zone_id"] = rec.parent_zone_id;
			result.push_back(dict);
		}
	}
	return result;
}

Array InsightsDatabase::query_frame_markers(uint64_t p_start_ns, uint64_t p_end_ns) const {
	Array result;
	for (uint32_t i = 0; i < frame_markers.size(); i++) {
		const FrameMarker &rec = frame_markers[i];
		if (rec.start_ns >= p_start_ns && rec.end_ns <= p_end_ns) {
			Dictionary dict;
			dict["frame_index"] = rec.frame_index;
			dict["start_ns"] = (int64_t)rec.start_ns;
			dict["end_ns"] = (int64_t)rec.end_ns;
			result.push_back(dict);
		}
	}
	return result;
}

Array InsightsDatabase::query_allocations_in_range(uint64_t p_start_ns, uint64_t p_end_ns) const {
	Array result;
	for (uint32_t i = 0; i < allocations.size(); i++) {
		const AllocationRecord &rec = allocations[i];
		if (rec.alloc_ns >= p_start_ns && rec.alloc_ns <= p_end_ns) {
			Dictionary dict;
			dict["ptr"] = (int64_t)rec.ptr;
			dict["size"] = (int64_t)rec.size;
			dict["site_zone_id"] = rec.site_zone_id;
			dict["alloc_ns"] = (int64_t)rec.alloc_ns;
			dict["free_ns"] = (int64_t)rec.free_ns;
			dict["thread_id"] = (int64_t)rec.thread_id;
			result.push_back(dict);
		}
	}
	return result;
}

Array InsightsDatabase::query_resource_loads_in_range(uint64_t p_start_ns, uint64_t p_end_ns) const {
	Array result;
	for (uint32_t i = 0; i < resource_loads.size(); i++) {
		const ResourceLoadRecord &rec = resource_loads[i];
		if (rec.start_ns >= p_start_ns && rec.start_ns <= p_end_ns) {
			Dictionary dict;
			dict["path"] = rec.path;
			dict["loader"] = rec.loader;
			dict["start_ns"] = (int64_t)rec.start_ns;
			dict["end_ns"] = (int64_t)rec.end_ns;
			dict["size_bytes"] = (int64_t)rec.size_bytes;
			dict["parent_path"] = rec.parent_path;
			dict["thread_id"] = (int64_t)rec.thread_id;
			result.push_back(dict);
		}
	}
	return result;
}

Array InsightsDatabase::query_gpu_zones_for_cpu_zone(uint32_t p_cpu_zone_id) const {
	Array result;
	for (uint32_t i = 0; i < gpu_zones.size(); i++) {
		const GPUZoneRecord &rec = gpu_zones[i];
		if ((uint32_t)rec.context_id == p_cpu_zone_id) {
			Dictionary dict;
			dict["name"] = rec.name;
			dict["queue_id"] = (int64_t)rec.queue_id;
			dict["submit_ns"] = (int64_t)rec.submit_ns;
			dict["start_ns"] = (int64_t)rec.start_ns;
			dict["end_ns"] = (int64_t)rec.end_ns;
			dict["context_id"] = rec.context_id;
			result.push_back(dict);
		}
	}
	return result;
}

Array InsightsDatabase::query_gpu_zones_in_range(uint64_t p_start_ns, uint64_t p_end_ns) const {
	Array result;
	for (uint32_t i = 0; i < gpu_zones.size(); i++) {
		const GPUZoneRecord &rec = gpu_zones[i];
		if (rec.start_ns >= p_start_ns && rec.end_ns <= p_end_ns) {
			Dictionary dict;
			dict["name"] = rec.name;
			dict["queue_id"] = (int64_t)rec.queue_id;
			dict["submit_ns"] = (int64_t)rec.submit_ns;
			dict["start_ns"] = (int64_t)rec.start_ns;
			dict["end_ns"] = (int64_t)rec.end_ns;
			dict["context_id"] = rec.context_id;
			result.push_back(dict);
		}
	}
	return result;
}

uint32_t InsightsDatabase::get_zone_count() const {
	return zones.size();
}

uint32_t InsightsDatabase::get_frame_marker_count() const {
	return frame_markers.size();
}

uint32_t InsightsDatabase::get_allocation_count() const {
	return allocations.size();
}

Array InsightsDatabase::query_plot_points(const String &p_plot_name, uint64_t p_start_ns, uint64_t p_end_ns) const {
	Array result;
	for (uint32_t i = 0; i < plot_points.size(); i++) {
		const PlotPointRecord &rec = plot_points[i];
		if (rec.plot_name == p_plot_name && rec.time_ns >= p_start_ns && rec.time_ns <= p_end_ns) {
			Dictionary dict;
			dict["plot_name"] = rec.plot_name;
			dict["time_ns"] = (int64_t)rec.time_ns;
			dict["value"] = rec.value;
			result.push_back(dict);
		}
	}
	return result;
}

Array InsightsDatabase::query_lock_events(uint64_t p_start_ns, uint64_t p_end_ns) const {
	Array result;
	for (uint32_t i = 0; i < lock_events.size(); i++) {
		const LockEventRecord &rec = lock_events[i];
		if (rec.time_ns >= p_start_ns && rec.time_ns <= p_end_ns) {
			Dictionary dict;
			dict["lock_id"] = (int64_t)rec.lock_id;
			dict["srcloc"] = rec.srcloc;
			dict["time_ns"] = (int64_t)rec.time_ns;
			dict["type"] = rec.type;
			dict["thread_id"] = (int64_t)rec.thread_id;
			result.push_back(dict);
		}
	}
	return result;
}

Array InsightsDatabase::get_plot_names() const {
	Array result;
	HashSet<String> seen;
	for (uint32_t i = 0; i < plot_points.size(); i++) {
		const String &name = plot_points[i].plot_name;
		if (!seen.has(name)) {
			seen.insert(name);
			result.append(name);
		}
	}
	return result;
}

uint32_t InsightsDatabase::get_plot_point_count() const {
	return plot_points.size();
}

uint32_t InsightsDatabase::get_lock_event_count() const {
	return lock_events.size();
}

uint32_t InsightsDatabase::get_message_count() const {
	return messages.size();
}

uint32_t InsightsDatabase::get_gpu_zone_count() const {
	return gpu_zones.size();
}

Error InsightsDatabase::save_to_file(const String &p_path) const {
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::WRITE);
	if (f.is_null()) {
		return ERR_FILE_CANT_OPEN;
	}

	// Magic + version.
	f->store_buffer((const uint8_t *)"GDIN", 4);
	f->store_32(2); // version

	// Zones.
	f->store_32(zones.size());
	for (uint32_t i = 0; i < zones.size(); i++) {
		f->store_pascal_string(zones[i].name);
		f->store_pascal_string(zones[i].file);
		f->store_pascal_string(zones[i].function);
		f->store_32(zones[i].line);
		f->store_pascal_string(zones[i].channel);
		f->store_64(zones[i].thread_id);
		f->store_64(zones[i].start_ns);
		f->store_64(zones[i].end_ns);
		f->store_32(zones[i].depth);
		f->store_32(zones[i].parent_zone_id);
	}

	// Frame markers.
	f->store_32(frame_markers.size());
	for (uint32_t i = 0; i < frame_markers.size(); i++) {
		f->store_32(frame_markers[i].frame_index);
		f->store_64(frame_markers[i].start_ns);
		f->store_64(frame_markers[i].end_ns);
	}

	// Allocations.
	f->store_32(allocations.size());
	for (uint32_t i = 0; i < allocations.size(); i++) {
		f->store_64(allocations[i].ptr);
		f->store_64(allocations[i].size);
		f->store_32(allocations[i].site_zone_id);
		f->store_64(allocations[i].alloc_ns);
		f->store_64(allocations[i].free_ns);
		f->store_64(allocations[i].thread_id);
	}

	// GPU zones.
	f->store_32(gpu_zones.size());
	for (uint32_t i = 0; i < gpu_zones.size(); i++) {
		f->store_pascal_string(gpu_zones[i].name);
		f->store_64(gpu_zones[i].queue_id);
		f->store_64(gpu_zones[i].submit_ns);
		f->store_64(gpu_zones[i].start_ns);
		f->store_64(gpu_zones[i].end_ns);
		f->store_32(gpu_zones[i].context_id);
	}

	// Resource loads.
	f->store_32(resource_loads.size());
	for (uint32_t i = 0; i < resource_loads.size(); i++) {
		f->store_pascal_string(resource_loads[i].path);
		f->store_pascal_string(resource_loads[i].loader);
		f->store_64(resource_loads[i].start_ns);
		f->store_64(resource_loads[i].end_ns);
		f->store_64(resource_loads[i].size_bytes);
		f->store_pascal_string(resource_loads[i].parent_path);
		f->store_64(resource_loads[i].thread_id);
	}

	// Messages.
	f->store_32(messages.size());
	for (uint32_t i = 0; i < messages.size(); i++) {
		f->store_32(messages[i].level);
		f->store_pascal_string(messages[i].text);
		f->store_64(messages[i].timestamp_ns);
		f->store_32(messages[i].zone_id);
	}

	// Plot points.
	f->store_32(plot_points.size());
	for (uint32_t i = 0; i < plot_points.size(); i++) {
		f->store_pascal_string(plot_points[i].plot_name);
		f->store_64(plot_points[i].time_ns);
		f->store_double(plot_points[i].value);
	}

	// Lock events.
	f->store_32(lock_events.size());
	for (uint32_t i = 0; i < lock_events.size(); i++) {
		f->store_64(lock_events[i].lock_id);
		f->store_64(lock_events[i].srcloc);
		f->store_64(lock_events[i].time_ns);
		f->store_32(lock_events[i].type);
		f->store_64(lock_events[i].thread_id);
	}

	return OK;
}

Error InsightsDatabase::load_from_file(const String &p_path) {
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
	if (f.is_null()) {
		return ERR_FILE_CANT_OPEN;
	}

	// Read and verify magic.
	uint8_t magic[4];
	f->get_buffer(magic, 4);
	if (memcmp(magic, "GDIN", 4) != 0) {
		return ERR_FILE_CORRUPT;
	}

	uint32_t version = f->get_32();
	if (version < 1 || version > 2) {
		return ERR_FILE_CORRUPT;
	}

	clear();

	// Zones.
	uint32_t zone_count = f->get_32();
	zones.reserve(zone_count);
	for (uint32_t i = 0; i < zone_count; i++) {
		ZoneRecord rec;
		rec.name = f->get_pascal_string();
		rec.file = f->get_pascal_string();
		rec.function = f->get_pascal_string();
		rec.line = f->get_32();
		rec.channel = f->get_pascal_string();
		rec.thread_id = f->get_64();
		rec.start_ns = f->get_64();
		rec.end_ns = f->get_64();
		rec.depth = f->get_32();
		rec.parent_zone_id = f->get_32();

		uint32_t idx = zones.size();
		zones.push_back(rec);
		if (!zone_name_index.has(rec.name)) {
			zone_name_index[rec.name] = LocalVector<uint32_t>();
		}
		zone_name_index[rec.name].push_back(idx);
	}

	// Frame markers.
	uint32_t fm_count = f->get_32();
	frame_markers.reserve(fm_count);
	for (uint32_t i = 0; i < fm_count; i++) {
		FrameMarker rec;
		rec.frame_index = f->get_32();
		rec.start_ns = f->get_64();
		rec.end_ns = f->get_64();
		frame_markers.push_back(rec);
	}

	// Allocations.
	uint32_t alloc_count = f->get_32();
	allocations.reserve(alloc_count);
	for (uint32_t i = 0; i < alloc_count; i++) {
		AllocationRecord rec;
		rec.ptr = f->get_64();
		rec.size = f->get_64();
		rec.site_zone_id = f->get_32();
		rec.alloc_ns = f->get_64();
		rec.free_ns = f->get_64();
		rec.thread_id = f->get_64();
		allocations.push_back(rec);
	}

	// GPU zones.
	uint32_t gpu_count = f->get_32();
	gpu_zones.reserve(gpu_count);
	for (uint32_t i = 0; i < gpu_count; i++) {
		GPUZoneRecord rec;
		rec.name = f->get_pascal_string();
		rec.queue_id = f->get_64();
		rec.submit_ns = f->get_64();
		rec.start_ns = f->get_64();
		rec.end_ns = f->get_64();
		rec.context_id = f->get_32();
		gpu_zones.push_back(rec);
	}

	// Resource loads.
	uint32_t rl_count = f->get_32();
	resource_loads.reserve(rl_count);
	for (uint32_t i = 0; i < rl_count; i++) {
		ResourceLoadRecord rec;
		rec.path = f->get_pascal_string();
		rec.loader = f->get_pascal_string();
		rec.start_ns = f->get_64();
		rec.end_ns = f->get_64();
		rec.size_bytes = f->get_64();
		rec.parent_path = f->get_pascal_string();
		rec.thread_id = f->get_64();
		resource_loads.push_back(rec);
	}

	// Messages.
	uint32_t msg_count = f->get_32();
	messages.reserve(msg_count);
	for (uint32_t i = 0; i < msg_count; i++) {
		MessageRecord rec;
		rec.level = f->get_32();
		rec.text = f->get_pascal_string();
		rec.timestamp_ns = f->get_64();
		rec.zone_id = f->get_32();
		messages.push_back(rec);
	}

	// Plot points and lock events (version 2+).
	if (version >= 2) {
		uint32_t pp_count = f->get_32();
		plot_points.reserve(pp_count);
		for (uint32_t i = 0; i < pp_count; i++) {
			PlotPointRecord rec;
			rec.plot_name = f->get_pascal_string();
			rec.time_ns = f->get_64();
			rec.value = f->get_double();
			plot_points.push_back(rec);
		}

		uint32_t le_count = f->get_32();
		lock_events.reserve(le_count);
		for (uint32_t i = 0; i < le_count; i++) {
			LockEventRecord rec;
			rec.lock_id = f->get_64();
			rec.srcloc = (int64_t)f->get_64();
			rec.time_ns = f->get_64();
			rec.type = f->get_32();
			rec.thread_id = f->get_64();
			lock_events.push_back(rec);
		}
	}

	file_path = p_path;
	is_open = true;
	return OK;
}

void InsightsDatabase::clear() {
	zones.clear();
	frame_markers.clear();
	allocations.clear();
	gpu_zones.clear();
	resource_loads.clear();
	messages.clear();
	plot_points.clear();
	lock_events.clear();
	zone_name_index.clear();
}

Array InsightsDatabase::query_zones_by_depth(int p_depth) const {
	Array result;
	for (uint32_t i = 0; i < zones.size(); i++) {
		const ZoneRecord &rec = zones[i];
		if (rec.depth == p_depth) {
			Dictionary dict;
			dict["name"] = rec.name;
			dict["file"] = rec.file;
			dict["function"] = rec.function;
			dict["line"] = rec.line;
			dict["channel"] = rec.channel;
			dict["thread_id"] = (int64_t)rec.thread_id;
			dict["start_ns"] = (int64_t)rec.start_ns;
			dict["end_ns"] = (int64_t)rec.end_ns;
			dict["depth"] = rec.depth;
			dict["parent_zone_id"] = rec.parent_zone_id;
			result.push_back(dict);
		}
	}
	return result;
}

Array InsightsDatabase::query_allocations_by_size(uint64_t p_min_size) const {
	Array result;
	for (uint32_t i = 0; i < allocations.size(); i++) {
		const AllocationRecord &rec = allocations[i];
		if (rec.size >= p_min_size) {
			Dictionary dict;
			dict["ptr"] = (int64_t)rec.ptr;
			dict["size"] = (int64_t)rec.size;
			dict["site_zone_id"] = rec.site_zone_id;
			dict["alloc_ns"] = (int64_t)rec.alloc_ns;
			dict["free_ns"] = (int64_t)rec.free_ns;
			dict["thread_id"] = (int64_t)rec.thread_id;
			result.push_back(dict);
		}
	}
	return result;
}

Array InsightsDatabase::query_messages() const {
	Array result;
	for (uint32_t i = 0; i < messages.size(); i++) {
		const MessageRecord &rec = messages[i];
		Dictionary dict;
		dict["level"] = rec.level;
		dict["text"] = rec.text;
		dict["timestamp_ns"] = (int64_t)rec.timestamp_ns;
		dict["zone_id"] = rec.zone_id;
		result.push_back(dict);
	}
	return result;
}

Array InsightsDatabase::get_plot_data(const String &p_plot_name) const {
	Array result;

	if (p_plot_name == "Frame Time") {
		for (uint32_t i = 0; i < frame_markers.size(); i++) {
			const FrameMarker &rec = frame_markers[i];
			Dictionary point;
			point["x"] = (double)rec.frame_index;
			double frame_ms = (double)(rec.end_ns - rec.start_ns) / 1e6;
			point["y"] = frame_ms;
			result.push_back(point);
		}
	} else if (p_plot_name == "Memory Usage") {
		// Compute memory usage over time from allocation/free events.
		struct MemEvent {
			uint64_t time_ns;
			uint64_t size;
			bool is_alloc;
		};

		LocalVector<MemEvent> events;
		events.reserve(allocations.size() * 2);
		for (uint32_t i = 0; i < allocations.size(); i++) {
			const AllocationRecord &rec = allocations[i];
			events.push_back({ rec.alloc_ns, rec.size, true });
			if (rec.free_ns != 0) {
				events.push_back({ rec.free_ns, rec.size, false });
			}
		}

		// Sort by time (simple bubble sort for small data).
		for (uint32_t i = 0; i < events.size(); i++) {
			for (uint32_t j = i + 1; j < events.size(); j++) {
				if (events[j].time_ns < events[i].time_ns) {
					MemEvent tmp = events[i];
					events[i] = events[j];
					events[j] = tmp;
				}
			}
		}

		uint64_t current = 0;
		for (uint32_t i = 0; i < events.size(); i++) {
			if (events[i].is_alloc) {
				current += events[i].size;
			} else {
				current -= events[i].size;
			}
			Dictionary point;
			point["x"] = (double)events[i].time_ns / 1e6;
			point["y"] = (double)current;
			result.push_back(point);
		}
	}

	return result;
}

Array InsightsDatabase::query_resource_loads() const {
	Array result;
	for (uint32_t i = 0; i < resource_loads.size(); i++) {
		const ResourceLoadRecord &rec = resource_loads[i];
		Dictionary dict;
		dict["path"] = rec.path;
		dict["loader"] = rec.loader;
		dict["start_ns"] = (int64_t)rec.start_ns;
		dict["end_ns"] = (int64_t)rec.end_ns;
		dict["size_bytes"] = (int64_t)rec.size_bytes;
		dict["parent_path"] = rec.parent_path;
		dict["thread_id"] = (int64_t)rec.thread_id;
		result.push_back(dict);
	}
	return result;
}

Array InsightsDatabase::query_resource_dependencies(const String &p_path) const {
	Array result;
	for (uint32_t i = 0; i < resource_loads.size(); i++) {
		const ResourceLoadRecord &rec = resource_loads[i];
		if (rec.parent_path == p_path) {
			Dictionary dict;
			dict["path"] = rec.path;
			dict["loader"] = rec.loader;
			dict["start_ns"] = (int64_t)rec.start_ns;
			dict["end_ns"] = (int64_t)rec.end_ns;
			dict["size_bytes"] = (int64_t)rec.size_bytes;
			dict["parent_path"] = rec.parent_path;
			dict["thread_id"] = (int64_t)rec.thread_id;
			result.push_back(dict);
		}
	}
	return result;
}

uint64_t InsightsDatabase::get_peak_memory() const {
	if (allocations.size() == 0) {
		return 0;
	}

	// Collect all allocation/free events sorted by time.
	struct Event {
		uint64_t time_ns;
		uint64_t size;
		bool is_alloc;
	};

	LocalVector<Event> events;
	events.reserve(allocations.size() * 2);
	for (uint32_t i = 0; i < allocations.size(); i++) {
		const AllocationRecord &rec = allocations[i];
		events.push_back({ rec.alloc_ns, rec.size, true });
		if (rec.free_ns != 0) {
			events.push_back({ rec.free_ns, rec.size, false });
		}
	}

	// Sort by time.
	for (uint32_t i = 0; i < events.size(); i++) {
		for (uint32_t j = i + 1; j < events.size(); j++) {
			if (events[j].time_ns < events[i].time_ns) {
				Event tmp = events[i];
				events[i] = events[j];
				events[j] = tmp;
			}
		}
	}

	uint64_t current = 0;
	uint64_t peak = 0;
	for (uint32_t i = 0; i < events.size(); i++) {
		if (events[i].is_alloc) {
			current += events[i].size;
		} else {
			current -= events[i].size;
		}
		if (current > peak) {
			peak = current;
		}
	}
	return peak;
}

Array InsightsDatabase::get_leaked_allocations() const {
	Array result;
	for (uint32_t i = 0; i < allocations.size(); i++) {
		const AllocationRecord &rec = allocations[i];
		if (rec.free_ns == 0) {
			Dictionary dict;
			dict["ptr"] = (int64_t)rec.ptr;
			dict["size"] = (int64_t)rec.size;
			dict["alloc_ns"] = (int64_t)rec.alloc_ns;
			dict["thread_id"] = (int64_t)rec.thread_id;
			result.push_back(dict);
		}
	}
	return result;
}

uint64_t InsightsDatabase::get_total_duration_ns() const {
	uint64_t min_start = UINT64_MAX;
	uint64_t max_end = 0;

	// Check zones.
	for (uint32_t i = 0; i < zones.size(); i++) {
		if (zones[i].start_ns < min_start) {
			min_start = zones[i].start_ns;
		}
		if (zones[i].end_ns > max_end) {
			max_end = zones[i].end_ns;
		}
	}

	// Check frame markers.
	for (uint32_t i = 0; i < frame_markers.size(); i++) {
		if (frame_markers[i].start_ns < min_start) {
			min_start = frame_markers[i].start_ns;
		}
		if (frame_markers[i].end_ns > max_end) {
			max_end = frame_markers[i].end_ns;
		}
	}

	if (min_start == UINT64_MAX) {
		return 0;
	}
	return max_end - min_start;
}

void InsightsDatabase::_bind_methods() {
	ClassDB::bind_method(D_METHOD("open", "path"), &InsightsDatabase::open);
	ClassDB::bind_method(D_METHOD("close"), &InsightsDatabase::close);
	ClassDB::bind_method(D_METHOD("is_database_open"), &InsightsDatabase::is_database_open);
	ClassDB::bind_method(D_METHOD("create_tables"), &InsightsDatabase::create_tables);

	ClassDB::bind_method(D_METHOD("insert_zone", "name", "file", "line", "function", "channel", "thread_id", "start_ns", "end_ns", "depth", "parent_zone_id"), &InsightsDatabase::insert_zone);
	ClassDB::bind_method(D_METHOD("update_zone_end", "zone_index", "end_ns"), &InsightsDatabase::update_zone_end);
	ClassDB::bind_method(D_METHOD("insert_frame_marker", "frame_index", "start_ns", "end_ns"), &InsightsDatabase::insert_frame_marker);
	ClassDB::bind_method(D_METHOD("insert_allocation", "ptr", "size", "site_zone_id", "alloc_ns", "free_ns", "thread_id"), &InsightsDatabase::insert_allocation);
	ClassDB::bind_method(D_METHOD("insert_gpu_zone", "name", "queue_id", "submit_ns", "start_ns", "end_ns", "context_id"), &InsightsDatabase::insert_gpu_zone);
	ClassDB::bind_method(D_METHOD("insert_resource_load", "path", "loader", "start_ns", "end_ns", "size_bytes", "parent_path", "thread_id"), &InsightsDatabase::insert_resource_load);
	ClassDB::bind_method(D_METHOD("insert_message", "level", "text", "timestamp_ns", "zone_id"), &InsightsDatabase::insert_message);
	ClassDB::bind_method(D_METHOD("insert_plot_point", "plot_name", "time_ns", "value"), &InsightsDatabase::insert_plot_point);
	ClassDB::bind_method(D_METHOD("insert_lock_event", "lock_id", "srcloc", "time_ns", "type", "thread_id"), &InsightsDatabase::insert_lock_event);

	ClassDB::bind_method(D_METHOD("query_zone", "name", "start_ns", "end_ns"), &InsightsDatabase::query_zone);
	ClassDB::bind_method(D_METHOD("query_zones_in_range", "start_ns", "end_ns"), &InsightsDatabase::query_zones_in_range);
	ClassDB::bind_method(D_METHOD("query_frame_markers", "start_ns", "end_ns"), &InsightsDatabase::query_frame_markers);
	ClassDB::bind_method(D_METHOD("query_allocations_in_range", "start_ns", "end_ns"), &InsightsDatabase::query_allocations_in_range);
	ClassDB::bind_method(D_METHOD("query_resource_loads_in_range", "start_ns", "end_ns"), &InsightsDatabase::query_resource_loads_in_range);
	ClassDB::bind_method(D_METHOD("query_gpu_zones_for_cpu_zone", "cpu_zone_id"), &InsightsDatabase::query_gpu_zones_for_cpu_zone);
	ClassDB::bind_method(D_METHOD("query_gpu_zones_in_range", "start_ns", "end_ns"), &InsightsDatabase::query_gpu_zones_in_range);

	ClassDB::bind_method(D_METHOD("query_zones_by_depth", "depth"), &InsightsDatabase::query_zones_by_depth);
	ClassDB::bind_method(D_METHOD("query_allocations_by_size", "min_size"), &InsightsDatabase::query_allocations_by_size);
	ClassDB::bind_method(D_METHOD("query_resource_loads"), &InsightsDatabase::query_resource_loads);
	ClassDB::bind_method(D_METHOD("query_resource_dependencies", "path"), &InsightsDatabase::query_resource_dependencies);
	ClassDB::bind_method(D_METHOD("query_messages"), &InsightsDatabase::query_messages);
	ClassDB::bind_method(D_METHOD("get_peak_memory"), &InsightsDatabase::get_peak_memory);
	ClassDB::bind_method(D_METHOD("get_leaked_allocations"), &InsightsDatabase::get_leaked_allocations);
	ClassDB::bind_method(D_METHOD("get_total_duration_ns"), &InsightsDatabase::get_total_duration_ns);

	ClassDB::bind_method(D_METHOD("get_plot_names"), &InsightsDatabase::get_plot_names);
	ClassDB::bind_method(D_METHOD("get_plot_data", "plot_name"), &InsightsDatabase::get_plot_data);

	ClassDB::bind_method(D_METHOD("get_zone_count"), &InsightsDatabase::get_zone_count);
	ClassDB::bind_method(D_METHOD("get_frame_marker_count"), &InsightsDatabase::get_frame_marker_count);
	ClassDB::bind_method(D_METHOD("get_allocation_count"), &InsightsDatabase::get_allocation_count);

	ClassDB::bind_method(D_METHOD("query_plot_points", "plot_name", "start_ns", "end_ns"), &InsightsDatabase::query_plot_points);
	ClassDB::bind_method(D_METHOD("query_lock_events", "start_ns", "end_ns"), &InsightsDatabase::query_lock_events);
	ClassDB::bind_method(D_METHOD("get_plot_point_count"), &InsightsDatabase::get_plot_point_count);
	ClassDB::bind_method(D_METHOD("get_lock_event_count"), &InsightsDatabase::get_lock_event_count);
	ClassDB::bind_method(D_METHOD("get_message_count"), &InsightsDatabase::get_message_count);
	ClassDB::bind_method(D_METHOD("get_gpu_zone_count"), &InsightsDatabase::get_gpu_zone_count);

	ClassDB::bind_method(D_METHOD("save_to_file", "path"), &InsightsDatabase::save_to_file);
	ClassDB::bind_method(D_METHOD("load_from_file", "path"), &InsightsDatabase::load_from_file);
	ClassDB::bind_method(D_METHOD("clear"), &InsightsDatabase::clear);
}

InsightsDatabase::InsightsDatabase() {
}

InsightsDatabase::~InsightsDatabase() {
}
