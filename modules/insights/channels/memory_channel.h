/**************************************************************************/
/*  memory_channel.h                                                      */
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
#include "core/templates/hash_map.h"
#include "core/templates/local_vector.h"

class MemoryChannel : public InsightsChannel {
	GDCLASS(MemoryChannel, InsightsChannel);

public:
	struct LeakInfo {
		uint64_t ptr = 0;
		uint64_t size = 0;
		uint64_t alloc_ns = 0;
		uint64_t thread_id = 0;
	};

private:
	// Active allocations: ptr -> {size, alloc_ns, thread_id}
	HashMap<uint64_t, LeakInfo> active_allocations;
	uint64_t total_allocated = 0;
	uint64_t allocation_count = 0;
	uint64_t total_freed = 0;

protected:
	static void _bind_methods();

public:
	void on_alloc(uint64_t p_ptr, uint64_t p_size, uint64_t p_thread_id, uint64_t p_timestamp_ns);
	void on_free(uint64_t p_ptr, uint64_t p_thread_id, uint64_t p_timestamp_ns);

	uint64_t get_total_allocated() const;
	uint64_t get_allocation_count() const;
	uint64_t get_total_freed() const;
	TypedArray<Dictionary> detect_leaks() const;

	virtual void on_event(const Dictionary &p_event_data) override;
	virtual Dictionary serialize() override;

	MemoryChannel();
	virtual ~MemoryChannel();
};
