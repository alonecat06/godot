/**************************************************************************/
/*  gpu_channel.cpp                                                       */
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

#include "gpu_channel.h"

#include "core/object/class_db.h"
#include "core/variant/typed_array.h"
#include "servers/rendering/rendering_device.h"

void GPUChannel::on_gpu_timestamp(const String &p_name, uint64_t p_gpu_time_ns, uint64_t p_cpu_time_ns, uint32_t p_frame_index) {
	if (gpu_zones.size() >= max_zones) {
		return;
	}

	GPUZoneRecord record;
	record.name = p_name;
	record.start_ns = p_gpu_time_ns;
	record.end_ns = p_gpu_time_ns + 1;
	record.submit_ns = p_cpu_time_ns;
	record.context_id = p_frame_index;

	gpu_zones.push_back(record);
}

void GPUChannel::on_gpu_zone(const String &p_name, uint32_t p_queue_id, uint64_t p_submit_ns, uint64_t p_start_ns, uint64_t p_end_ns, uint32_t p_context_id) {
	if (gpu_zones.size() >= max_zones) {
		return;
	}

	GPUZoneRecord record;
	record.name = p_name;
	record.queue_id = p_queue_id;
	record.submit_ns = p_submit_ns;
	record.start_ns = p_start_ns;
	record.end_ns = p_end_ns;
	record.context_id = p_context_id;

	gpu_zones.push_back(record);
}

void GPUChannel::collect_frame_timestamps(RenderingDevice *p_rd) {
	if (p_rd == nullptr) {
		return;
	}

	uint32_t count = p_rd->get_captured_timestamps_count();
	for (uint32_t i = 0; i < count; i++) {
		String name = p_rd->get_captured_timestamp_name(i);
		uint64_t gpu_time = p_rd->get_captured_timestamp_gpu_time(i);
		uint64_t cpu_time = p_rd->get_captured_timestamp_cpu_time(i);
		on_gpu_timestamp(name, gpu_time, cpu_time, 0);
	}
}

TypedArray<Dictionary> GPUChannel::get_gpu_zones_in_range(uint64_t p_start_ns, uint64_t p_end_ns) const {
	TypedArray<Dictionary> result;

	for (uint32_t i = 0; i < gpu_zones.size(); i++) {
		const GPUZoneRecord &zone = gpu_zones[i];
		if (zone.start_ns >= p_start_ns && zone.end_ns <= p_end_ns) {
			Dictionary dict;
			dict["name"] = zone.name;
			dict["queue_id"] = zone.queue_id;
			dict["submit_ns"] = zone.submit_ns;
			dict["start_ns"] = zone.start_ns;
			dict["end_ns"] = zone.end_ns;
			dict["context_id"] = zone.context_id;
			result.push_back(dict);
		}
	}

	return result;
}

TypedArray<Dictionary> GPUChannel::get_gpu_zones_for_cpu_zone(uint32_t p_cpu_zone_id) const {
	TypedArray<Dictionary> result;

	for (uint32_t i = 0; i < gpu_zones.size(); i++) {
		const GPUZoneRecord &zone = gpu_zones[i];
		if (zone.context_id == p_cpu_zone_id) {
			Dictionary dict;
			dict["name"] = zone.name;
			dict["queue_id"] = zone.queue_id;
			dict["submit_ns"] = zone.submit_ns;
			dict["start_ns"] = zone.start_ns;
			dict["end_ns"] = zone.end_ns;
			dict["context_id"] = zone.context_id;
			result.push_back(dict);
		}
	}

	return result;
}

uint32_t GPUChannel::get_zone_count() const {
	return gpu_zones.size();
}

void GPUChannel::on_event(const Dictionary &p_event_data) {
	if (!p_event_data.has("type")) {
		return;
	}

	String type = p_event_data["type"];

	if (type == "gpu_zone") {
		String name = p_event_data.get("name", "");
		uint32_t queue_id = p_event_data.get("queue_id", (uint32_t)0);
		uint64_t submit_ns = p_event_data.get("submit_ns", (uint64_t)0);
		uint64_t start_ns = p_event_data.get("start_ns", (uint64_t)0);
		uint64_t end_ns = p_event_data.get("end_ns", (uint64_t)0);
		uint32_t context_id = p_event_data.get("context_id", (uint32_t)0);
		on_gpu_zone(name, queue_id, submit_ns, start_ns, end_ns, context_id);
	} else if (type == "gpu_timestamp") {
		String name = p_event_data.get("name", "");
		uint64_t gpu_time = p_event_data.get("gpu_time_ns", (uint64_t)0);
		uint64_t cpu_time = p_event_data.get("cpu_time_ns", (uint64_t)0);
		uint32_t frame_index = p_event_data.get("frame_index", (uint32_t)0);
		on_gpu_timestamp(name, gpu_time, cpu_time, frame_index);
	}
}

Dictionary GPUChannel::serialize() {
	Dictionary dict;
	dict["zone_count"] = gpu_zones.size();
	return dict;
}

void GPUChannel::_bind_methods() {
	ClassDB::bind_method(D_METHOD("on_gpu_zone", "name", "queue_id", "submit_ns", "start_ns", "end_ns", "context_id"), &GPUChannel::on_gpu_zone);
	ClassDB::bind_method(D_METHOD("get_gpu_zones_in_range", "start_ns", "end_ns"), &GPUChannel::get_gpu_zones_in_range);
	ClassDB::bind_method(D_METHOD("get_gpu_zones_for_cpu_zone", "cpu_zone_id"), &GPUChannel::get_gpu_zones_for_cpu_zone);
	ClassDB::bind_method(D_METHOD("get_zone_count"), &GPUChannel::get_zone_count);
	ClassDB::bind_method(D_METHOD("collect_frame_timestamps", "rd"), &GPUChannel::collect_frame_timestamps);
}

GPUChannel::GPUChannel() {
	set_category(CHANNEL_CATEGORY_GPU);
	set_color(Color(0x7A / 255.0f, 0xE5 / 255.0f, 0x7A / 255.0f));
	set_name("gpu");
}

GPUChannel::~GPUChannel() {
}
