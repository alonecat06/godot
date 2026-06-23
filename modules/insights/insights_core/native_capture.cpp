#include "native_capture.h"

#include "core/os/os.h"
#include "core/os/time.h"

void NativeCapture::_consumer_thread_func(void *p_userdata) {
	NativeCapture *self = static_cast<NativeCapture *>(p_userdata);
	self->_process_events();
}

void NativeCapture::_process_events() {
	while (true) {
		// Swap buffers under mutex.
		{
			MutexLock lock(event_mutex);
			if (stop_requested && event_queue.is_empty()) {
				break;
			}
			write_buffer = event_queue;
			event_queue.clear();
		}

		// Process write_buffer without holding the mutex.
		for (uint32_t i = 0; i < write_buffer.size(); i++) {
			const TraceEvent &ev = write_buffer[i];
			switch (ev.type) {
				case EventType::ZONE_BEGIN: {
					if (database.is_valid()) {
						database->insert_zone(
								String(ev.data.zone_begin.name),
								String(ev.data.zone_begin.file),
								ev.data.zone_begin.line,
								String(ev.data.zone_begin.function),
								String(ev.data.zone_begin.channel),
								ev.data.zone_begin.thread_id,
								ev.data.zone_begin.start_ns,
								0, // end_ns will be filled by ZONE_END
								0, // depth
								-1); // parent_zone_id
					}
				} break;
				case EventType::ZONE_END: {
					// Zone end updates are deferred to Phase 2.
					// For now, we just record the event.
				} break;
				case EventType::ALLOC: {
					if (database.is_valid()) {
						database->insert_allocation(
								ev.data.alloc.ptr,
								ev.data.alloc.size,
								-1, // site_zone_id
								ev.data.alloc.timestamp_ns,
								0, // free_ns
								ev.data.alloc.thread_id);
					}
				} break;
				case EventType::FREE: {
					// Free updates are deferred to Phase 2.
				} break;
				case EventType::FRAME: {
					if (database.is_valid()) {
						uint64_t frame_start = start_time_ns + (uint64_t)(ev.data.frame.frame_time * 1e6);
						uint64_t frame_end = frame_start + (uint64_t)(ev.data.frame.frame_time * 1e6);
						database->insert_frame_marker(frame_count, frame_start, frame_end);
						frame_count++;
					}
				} break;
				case EventType::RESOURCE_LOAD: {
					if (database.is_valid()) {
						database->insert_resource_load(
								String(ev.data.resource_load.path),
								String(ev.data.resource_load.loader),
								ev.data.resource_load.start_ns,
								ev.data.resource_load.end_ns,
								ev.data.resource_load.size_bytes,
								String(ev.data.resource_load.parent_path),
								ev.data.resource_load.thread_id);
					}
				} break;
				case EventType::LOG_MESSAGE: {
					if (database.is_valid()) {
						database->insert_message(
								ev.data.log_message.level,
								String(ev.data.log_message.text),
								ev.data.log_message.timestamp_ns,
								ev.data.log_message.zone_id);
					}
				} break;
				case EventType::GC_EVENT: {
					// GC events are stored as zones with a special name.
					if (database.is_valid()) {
						database->insert_zone(
								"godot:script/gdscript/gc",
								"",
								0,
								"",
								"script",
								0,
								ev.data.gc_event.timestamp_ns,
								ev.data.gc_event.timestamp_ns + 1,
								0,
								-1);
					}
				} break;
				case EventType::GPU_ZONE: {
					if (database.is_valid()) {
						database->insert_gpu_zone(
								String(ev.data.gpu_zone.name),
								ev.data.gpu_zone.queue_id,
								ev.data.gpu_zone.submit_ns,
								ev.data.gpu_zone.start_ns,
								ev.data.gpu_zone.end_ns,
								ev.data.gpu_zone.context_id);
					}
				} break;
			}
		}
		write_buffer.clear();

		// Small sleep to avoid busy-waiting.
		OS::get_singleton()->delay_usec(1000); // 1ms
	}
}

void NativeCapture::start() {
	if (capturing) {
		return;
	}
	capturing = true;
	stop_requested = false;
	frame_count = 0;
	start_time_ns = Time::get_singleton()->get_ticks_usec() * 1000;

	if (database.is_null()) {
		database.instantiate();
		database->open("memory");
		database->create_tables();
	}

	consumer_thread.start(_consumer_thread_func, this);
}

void NativeCapture::stop() {
	if (!capturing) {
		return;
	}

	// Signal consumer thread to stop.
	{
		MutexLock lock(event_mutex);
		stop_requested = true;
	}

	consumer_thread.wait_to_finish();
	capturing = false;
}

bool NativeCapture::is_capturing() const {
	return capturing;
}

void NativeCapture::on_zone_begin(const String &p_name, const String &p_file, int p_line, const String &p_function, const String &p_channel, uint64_t p_thread_id, uint64_t p_start_ns) {
	if (!capturing) {
		return;
	}

	TraceEvent ev;
	ev.type = EventType::ZONE_BEGIN;
	// Copy strings into fixed-size buffers.
	{
		CharString cs = p_name.utf8();
		memcpy(ev.data.zone_begin.name, cs.ptr(), MIN((size_t)cs.length(), sizeof(ev.data.zone_begin.name) - 1));
		ev.data.zone_begin.name[MIN((size_t)cs.length(), sizeof(ev.data.zone_begin.name) - 1)] = '\0';
	}
	{
		CharString cs = p_file.utf8();
		memcpy(ev.data.zone_begin.file, cs.ptr(), MIN((size_t)cs.length(), sizeof(ev.data.zone_begin.file) - 1));
		ev.data.zone_begin.file[MIN((size_t)cs.length(), sizeof(ev.data.zone_begin.file) - 1)] = '\0';
	}
	{
		CharString cs = p_function.utf8();
		memcpy(ev.data.zone_begin.function, cs.ptr(), MIN((size_t)cs.length(), sizeof(ev.data.zone_begin.function) - 1));
		ev.data.zone_begin.function[MIN((size_t)cs.length(), sizeof(ev.data.zone_begin.function) - 1)] = '\0';
	}
	{
		CharString cs = p_channel.utf8();
		memcpy(ev.data.zone_begin.channel, cs.ptr(), MIN((size_t)cs.length(), sizeof(ev.data.zone_begin.channel) - 1));
		ev.data.zone_begin.channel[MIN((size_t)cs.length(), sizeof(ev.data.zone_begin.channel) - 1)] = '\0';
	}
	ev.data.zone_begin.line = p_line;
	ev.data.zone_begin.thread_id = p_thread_id;
	ev.data.zone_begin.start_ns = p_start_ns;

	MutexLock lock(event_mutex);
	event_queue.push_back(ev);
}

void NativeCapture::on_zone_end(uint64_t p_end_ns) {
	if (!capturing) {
		return;
	}

	TraceEvent ev;
	ev.type = EventType::ZONE_END;
	ev.data.zone_end.end_ns = p_end_ns;

	MutexLock lock(event_mutex);
	event_queue.push_back(ev);
}

void NativeCapture::on_alloc(uint64_t p_ptr, uint64_t p_size, uint64_t p_thread_id, uint64_t p_timestamp_ns) {
	if (!capturing) {
		return;
	}

	TraceEvent ev;
	ev.type = EventType::ALLOC;
	ev.data.alloc.ptr = p_ptr;
	ev.data.alloc.size = p_size;
	ev.data.alloc.thread_id = p_thread_id;
	ev.data.alloc.timestamp_ns = p_timestamp_ns;

	MutexLock lock(event_mutex);
	event_queue.push_back(ev);
}

void NativeCapture::on_free(uint64_t p_ptr, uint64_t p_thread_id, uint64_t p_timestamp_ns) {
	if (!capturing) {
		return;
	}

	TraceEvent ev;
	ev.type = EventType::FREE;
	ev.data.free_event.ptr = p_ptr;
	ev.data.free_event.thread_id = p_thread_id;
	ev.data.free_event.timestamp_ns = p_timestamp_ns;

	MutexLock lock(event_mutex);
	event_queue.push_back(ev);
}

void NativeCapture::on_frame(double p_frame_time, double p_process_time, double p_physics_time, double p_physics_frame_time) {
	if (!capturing) {
		return;
	}

	TraceEvent ev;
	ev.type = EventType::FRAME;
	ev.data.frame.frame_time = p_frame_time;
	ev.data.frame.process_time = p_process_time;
	ev.data.frame.physics_time = p_physics_time;
	ev.data.frame.physics_frame_time = p_physics_frame_time;

	MutexLock lock(event_mutex);
	event_queue.push_back(ev);
}

void NativeCapture::on_resource_load(const String &p_path, const String &p_loader, uint64_t p_start_ns, uint64_t p_end_ns, uint64_t p_size_bytes, const String &p_parent_path, uint64_t p_thread_id) {
	if (!capturing) {
		return;
	}

	TraceEvent ev;
	ev.type = EventType::RESOURCE_LOAD;
	{
		CharString cs = p_path.utf8();
		memcpy(ev.data.resource_load.path, cs.ptr(), MIN((size_t)cs.length(), sizeof(ev.data.resource_load.path) - 1));
		ev.data.resource_load.path[MIN((size_t)cs.length(), sizeof(ev.data.resource_load.path) - 1)] = '\0';
	}
	{
		CharString cs = p_loader.utf8();
		memcpy(ev.data.resource_load.loader, cs.ptr(), MIN((size_t)cs.length(), sizeof(ev.data.resource_load.loader) - 1));
		ev.data.resource_load.loader[MIN((size_t)cs.length(), sizeof(ev.data.resource_load.loader) - 1)] = '\0';
	}
	{
		CharString cs = p_parent_path.utf8();
		memcpy(ev.data.resource_load.parent_path, cs.ptr(), MIN((size_t)cs.length(), sizeof(ev.data.resource_load.parent_path) - 1));
		ev.data.resource_load.parent_path[MIN((size_t)cs.length(), sizeof(ev.data.resource_load.parent_path) - 1)] = '\0';
	}
	ev.data.resource_load.start_ns = p_start_ns;
	ev.data.resource_load.end_ns = p_end_ns;
	ev.data.resource_load.size_bytes = p_size_bytes;
	ev.data.resource_load.thread_id = p_thread_id;

	MutexLock lock(event_mutex);
	event_queue.push_back(ev);
}

void NativeCapture::on_log_message(int p_level, const String &p_text, const String &p_file, int p_line, uint64_t p_timestamp_ns, int p_zone_id) {
	if (!capturing) {
		return;
	}

	TraceEvent ev;
	ev.type = EventType::LOG_MESSAGE;
	ev.data.log_message.level = p_level;
	{
		CharString cs = p_text.utf8();
		memcpy(ev.data.log_message.text, cs.ptr(), MIN((size_t)cs.length(), sizeof(ev.data.log_message.text) - 1));
		ev.data.log_message.text[MIN((size_t)cs.length(), sizeof(ev.data.log_message.text) - 1)] = '\0';
	}
	{
		CharString cs = p_file.utf8();
		memcpy(ev.data.log_message.file, cs.ptr(), MIN((size_t)cs.length(), sizeof(ev.data.log_message.file) - 1));
		ev.data.log_message.file[MIN((size_t)cs.length(), sizeof(ev.data.log_message.file) - 1)] = '\0';
	}
	ev.data.log_message.line = p_line;
	ev.data.log_message.timestamp_ns = p_timestamp_ns;
	ev.data.log_message.zone_id = p_zone_id;

	MutexLock lock(event_mutex);
	event_queue.push_back(ev);
}

void NativeCapture::on_gc_event(int p_generation, int p_objects_collected, uint64_t p_timestamp_ns) {
	if (!capturing) {
		return;
	}

	TraceEvent ev;
	ev.type = EventType::GC_EVENT;
	ev.data.gc_event.generation = p_generation;
	ev.data.gc_event.objects_collected = p_objects_collected;
	ev.data.gc_event.timestamp_ns = p_timestamp_ns;

	MutexLock lock(event_mutex);
	event_queue.push_back(ev);
}

void NativeCapture::on_gpu_zone(const String &p_name, uint32_t p_queue_id, uint64_t p_submit_ns, uint64_t p_start_ns, uint64_t p_end_ns, uint32_t p_context_id) {
	if (!capturing) {
		return;
	}

	TraceEvent ev;
	ev.type = EventType::GPU_ZONE;
	{
		CharString cs = p_name.utf8();
		memcpy(ev.data.gpu_zone.name, cs.ptr(), MIN((size_t)cs.length(), sizeof(ev.data.gpu_zone.name) - 1));
		ev.data.gpu_zone.name[MIN((size_t)cs.length(), sizeof(ev.data.gpu_zone.name) - 1)] = '\0';
	}
	ev.data.gpu_zone.queue_id = p_queue_id;
	ev.data.gpu_zone.submit_ns = p_submit_ns;
	ev.data.gpu_zone.start_ns = p_start_ns;
	ev.data.gpu_zone.end_ns = p_end_ns;
	ev.data.gpu_zone.context_id = p_context_id;

	MutexLock lock(event_mutex);
	event_queue.push_back(ev);
}

void NativeCapture::flush() {
	// Wait for the consumer thread to process all pending events.
	// This is a simple busy-wait approach; a more sophisticated
	// implementation would use a condition variable.
	while (true) {
		MutexLock lock(event_mutex);
		if (event_queue.is_empty()) {
			break;
		}
	}
	OS::get_singleton()->delay_usec(100);
}

void NativeCapture::set_database(const Ref<InsightsDatabase> &p_database) {
	database = p_database;
}

Ref<InsightsDatabase> NativeCapture::get_database() const {
	return database;
}

void NativeCapture::_bind_methods() {
	ClassDB::bind_method(D_METHOD("start"), &NativeCapture::start);
	ClassDB::bind_method(D_METHOD("stop"), &NativeCapture::stop);
	ClassDB::bind_method(D_METHOD("is_capturing"), &NativeCapture::is_capturing);
	ClassDB::bind_method(D_METHOD("flush"), &NativeCapture::flush);
	ClassDB::bind_method(D_METHOD("set_database", "database"), &NativeCapture::set_database);
	ClassDB::bind_method(D_METHOD("get_database"), &NativeCapture::get_database);
}

NativeCapture::NativeCapture() {
}

NativeCapture::~NativeCapture() {
	if (capturing) {
		stop();
	}
}
