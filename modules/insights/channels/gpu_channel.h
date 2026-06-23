/**************************************************************************/
/*  gpu_channel.h                                                         */
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

#include "modules/insights/channels/insights_channel.h"
#include "core/templates/local_vector.h"
#include "core/templates/hash_map.h"

class GPUChannel : public InsightsChannel {
	GDCLASS(GPUChannel, InsightsChannel);

public:
	struct GPUZoneRecord {
		String name;
		uint32_t queue_id = 0;
		uint64_t submit_ns = 0;
		uint64_t start_ns = 0;
		uint64_t end_ns = 0;
		uint32_t context_id = 0;
	};

private:
	LocalVector<GPUZoneRecord> gpu_zones;
	uint32_t max_zones = 10000;

protected:
	static void _bind_methods();

public:
	void on_gpu_timestamp(const String &p_name, uint64_t p_gpu_time_ns, uint64_t p_cpu_time_ns, uint32_t p_frame_index);
	void on_gpu_zone(const String &p_name, uint32_t p_queue_id, uint64_t p_submit_ns, uint64_t p_start_ns, uint64_t p_end_ns, uint32_t p_context_id);

	void collect_frame_timestamps(class RenderingDevice *p_rd);

	TypedArray<Dictionary> get_gpu_zones_in_range(uint64_t p_start_ns, uint64_t p_end_ns) const;
	TypedArray<Dictionary> get_gpu_zones_for_cpu_zone(uint32_t p_cpu_zone_id) const;

	uint32_t get_zone_count() const;

	virtual void on_event(const Dictionary &p_event_data) override;
	virtual Dictionary serialize() override;

	GPUChannel();
	virtual ~GPUChannel();
};
