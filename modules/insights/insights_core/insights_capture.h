/**************************************************************************/
/*  insights_capture.h                                                    */
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

#include "core/object/ref_counted.h"
#include "core/string/ustring.h"

class InsightsCapture : public RefCounted {
	GDCLASS(InsightsCapture, RefCounted);

protected:
	static void _bind_methods();

public:
	virtual void start() = 0;
	virtual void stop() = 0;
	virtual bool is_capturing() const = 0;

	virtual void on_zone_begin(const String &p_name, const String &p_file, int p_line, const String &p_function, const String &p_channel, uint64_t p_thread_id, uint64_t p_start_ns) = 0;
	virtual void on_zone_end(uint64_t p_end_ns) = 0;
	virtual void on_alloc(uint64_t p_ptr, uint64_t p_size, uint64_t p_thread_id, uint64_t p_timestamp_ns) = 0;
	virtual void on_free(uint64_t p_ptr, uint64_t p_thread_id, uint64_t p_timestamp_ns) = 0;
	virtual void on_frame(double p_frame_time, double p_process_time, double p_physics_time, double p_physics_frame_time) = 0;

	virtual void on_resource_load(const String &p_path, const String &p_loader, uint64_t p_start_ns, uint64_t p_end_ns, uint64_t p_size_bytes, const String &p_parent_path, uint64_t p_thread_id) = 0;
	virtual void on_log_message(int p_level, const String &p_text, const String &p_file, int p_line, uint64_t p_timestamp_ns, int p_zone_id) = 0;
	virtual void on_gc_event(int p_generation, int p_objects_collected, uint64_t p_timestamp_ns) = 0;

	virtual void flush() = 0;

	InsightsCapture();
	virtual ~InsightsCapture();
};
