/**************************************************************************/
/*  native_capture.h                                                      */
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

#include "modules/insights/insights_core/insights_capture.h"
#include "modules/insights/insights_core/insights_database.h"

#include "core/os/thread.h"
#include "core/os/mutex.h"
#include "core/templates/local_vector.h"

class NativeCapture : public InsightsCapture {
	GDCLASS(NativeCapture, InsightsCapture);

public:
	enum class EventType : uint8_t {
		ZONE_BEGIN,
		ZONE_END,
		ALLOC,
		FREE,
		FRAME,
		RESOURCE_LOAD,
		LOG_MESSAGE,
		GC_EVENT,
		GPU_ZONE,
	};

	struct TraceEvent {
		EventType type;
		union {
			struct {
				char name[128];
				char file[128];
				char function[128];
				char channel[32];
				int line;
				uint64_t thread_id;
				uint64_t start_ns;
			} zone_begin;
			struct {
				uint64_t end_ns;
			} zone_end;
			struct {
				uint64_t ptr;
				uint64_t size;
				uint64_t thread_id;
				uint64_t timestamp_ns;
			} alloc;
			struct {
				uint64_t ptr;
				uint64_t thread_id;
				uint64_t timestamp_ns;
			} free_event;
			struct {
				double frame_time;
				double process_time;
				double physics_time;
				double physics_frame_time;
			} frame;
			struct {
				char path[256];
				char loader[64];
				char parent_path[256];
				uint64_t start_ns;
				uint64_t end_ns;
				uint64_t size_bytes;
				uint64_t thread_id;
			} resource_load;
			struct {
				int level;
				char text[512];
				char file[128];
				int line;
				uint64_t timestamp_ns;
				int zone_id;
			} log_message;
			struct {
				int generation;
				int objects_collected;
				uint64_t timestamp_ns;
			} gc_event;
			struct {
				char name[128];
				uint32_t queue_id;
				uint64_t submit_ns;
				uint64_t start_ns;
				uint64_t end_ns;
				uint32_t context_id;
			} gpu_zone;
		} data;
	};

private:
	bool capturing = false;
	Ref<InsightsDatabase> database;

	// Thread-safe event queue using mutex.
	Mutex event_mutex;
	LocalVector<TraceEvent> event_queue;
	LocalVector<TraceEvent> write_buffer; // Double buffer for consumer.

	Thread consumer_thread;
	bool stop_requested = false;

	uint64_t frame_count = 0;
	uint64_t start_time_ns = 0;

	static void _consumer_thread_func(void *p_userdata);
	void _process_events();

protected:
	static void _bind_methods();

public:
	virtual void start() override;
	virtual void stop() override;
	virtual bool is_capturing() const override;

	virtual void on_zone_begin(const String &p_name, const String &p_file, int p_line, const String &p_function, const String &p_channel, uint64_t p_thread_id, uint64_t p_start_ns) override;
	virtual void on_zone_end(uint64_t p_end_ns) override;
	virtual void on_alloc(uint64_t p_ptr, uint64_t p_size, uint64_t p_thread_id, uint64_t p_timestamp_ns) override;
	virtual void on_free(uint64_t p_ptr, uint64_t p_thread_id, uint64_t p_timestamp_ns) override;
	virtual void on_frame(double p_frame_time, double p_process_time, double p_physics_time, double p_physics_frame_time) override;
	virtual void on_resource_load(const String &p_path, const String &p_loader, uint64_t p_start_ns, uint64_t p_end_ns, uint64_t p_size_bytes, const String &p_parent_path, uint64_t p_thread_id) override;
	virtual void on_log_message(int p_level, const String &p_text, const String &p_file, int p_line, uint64_t p_timestamp_ns, int p_zone_id) override;
	virtual void on_gc_event(int p_generation, int p_objects_collected, uint64_t p_timestamp_ns) override;
	virtual void on_gpu_zone(const String &p_name, uint32_t p_queue_id, uint64_t p_submit_ns, uint64_t p_start_ns, uint64_t p_end_ns, uint32_t p_context_id) override;
	virtual void flush() override;

	void set_database(const Ref<InsightsDatabase> &p_database);
	Ref<InsightsDatabase> get_database() const;

	NativeCapture();
	virtual ~NativeCapture();
};
