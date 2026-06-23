/**************************************************************************/
/*  loading_channel.h                                                     */
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

class LoadingChannel : public InsightsChannel {
	GDCLASS(LoadingChannel, InsightsChannel);

public:
	struct LoadEvent {
		String path;
		String loader;
		uint64_t start_ns = 0;
		uint64_t end_ns = 0;
		uint64_t size_bytes = 0;
		String parent_path;
		uint64_t thread_id = 0;
		bool failed = false;
		String error;
	};

private:
	LocalVector<LoadEvent> load_events;
	HashMap<String, uint32_t> active_loads; // path -> index in load_events
	uint32_t max_events = 10000;

protected:
	static void _bind_methods();

public:
	void on_load_begin(const String &p_path, const String &p_loader, uint64_t p_timestamp_ns = 0);
	void on_load_end(const String &p_path, uint64_t p_size_bytes, uint64_t p_timestamp_ns = 0);
	void on_load_fail(const String &p_path, const String &p_error, uint64_t p_timestamp_ns = 0);

	TypedArray<Dictionary> get_load_events_in_range(uint64_t p_start_ns, uint64_t p_end_ns) const;
	TypedArray<Dictionary> get_active_loads() const;
	uint32_t get_event_count() const;

	void set_max_events(uint32_t p_max);
	uint32_t get_max_events() const;

	virtual void on_event(const Dictionary &p_event_data) override;
	virtual Dictionary serialize() override;

	LoadingChannel();
	virtual ~LoadingChannel();
};
