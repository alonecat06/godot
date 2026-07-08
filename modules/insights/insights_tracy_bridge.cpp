#include "insights_tracy_bridge.h"

#ifdef TRACY_SERVER_ENABLED

#include "tracy_server/TracyWorker.hpp"
#include "tracy_server/TracyFileWrite.hpp"
#include "tracy_server/TracyFileRead.hpp"

#include "core/error/error_list.h"
#include "core/os/os.h"

InsightsTracyBridge::InsightsTracyBridge() {}

InsightsTracyBridge::~InsightsTracyBridge() {
	disconnect();
}

Error InsightsTracyBridge::connect_to_client(const String &p_addr, uint16_t p_port) {
	if (m_worker) {
		disconnect();
	}
	try {
		m_worker = std::make_unique<tracy::Worker>(p_addr.utf8().get_data(), p_port, -1);
		m_is_live = true;
		return OK;
	} catch (...) {
		m_worker.reset();
		m_is_live = false;
		return ERR_CANT_CONNECT;
	}
}

Error InsightsTracyBridge::load_tracy_file(const String &p_path) {
	if (m_worker) {
		disconnect();
	}
	try {
		auto fileRead = tracy::FileRead::Open(p_path.utf8().get_data());
		if (!fileRead) {
			return ERR_FILE_CANT_OPEN;
		}
		m_worker = std::make_unique<tracy::Worker>(*fileRead, tracy::EventType::All, false, false);
		m_is_live = false;
		return OK;
	} catch (...) {
		m_worker.reset();
		m_is_live = false;
		return ERR_FILE_CANT_READ;
	}
}

void InsightsTracyBridge::disconnect() {
	m_worker.reset();
	m_is_live = false;
}

bool InsightsTracyBridge::is_connected() const {
	return m_worker && m_worker->IsConnected();
}

bool InsightsTracyBridge::has_data() const {
	return m_worker && m_worker->HasData();
}

Error InsightsTracyBridge::save_tracy_file(const String &p_path) const {
	if (!m_worker || !m_worker->HasData()) {
		return ERR_UNAVAILABLE;
	}
	try {
		auto fileWrite = tracy::FileWrite::Open(p_path.utf8().get_data(), tracy::FileCompression::Fast);
		if (!fileWrite) {
			return ERR_FILE_CANT_WRITE;
		}
		m_worker->Write(*fileWrite, false);
		return OK;
	} catch (...) {
		return ERR_FILE_CANT_WRITE;
	}
}

Array InsightsTracyBridge::get_thread_list() const {
	Array result;
	if (!m_worker) return result;

	const auto &threads = m_worker->GetThreadData();
	for (const auto &td : threads) {
		Dictionary thread_info;
		thread_info["id"] = (int64_t)td->id;
		thread_info["name"] = String(m_worker->GetThreadName(td->id));
		thread_info["zone_count"] = (int)td->timeline.size();
		result.append(thread_info);
	}
	return result;
}

Array InsightsTracyBridge::get_thread_zones(uint64_t p_tid, int64_t p_start_ns, int64_t p_end_ns) const {
	Array result;
	if (!m_worker) return result;

	const auto *td = m_worker->GetThreadData(p_tid);
	if (!td) return result;

	const auto &timeline = td->timeline;
	for (size_t i = 0; i < timeline.size(); i++) {
		const auto &ze = timeline[i];
		if (!ze) continue;

		int64_t start_ns = (int64_t)ze->Start();
		int64_t end_ns = ze->IsEndValid() ? (int64_t)ze->End() : -1;

		// Time range filter
		if (p_start_ns >= 0 && end_ns >= 0 && end_ns < p_start_ns) continue;
		if (p_end_ns >= 0 && start_ns > p_end_ns) continue;

		Dictionary zone_info;
		zone_info["start_ns"] = start_ns;
		zone_info["end_ns"] = end_ns;

		int16_t srcloc = ze->Srcloc();
		const auto &sl = m_worker->GetSourceLocation(srcloc);
		zone_info["name"] = String(m_worker->GetZoneName(sl));
		zone_info["function"] = String(sl.function.active ? m_worker->GetString(sl.function.str) : "");
		zone_info["file"] = String(sl.file.active ? m_worker->GetString(sl.file.str) : "");
		zone_info["line"] = (int)sl.line;
		zone_info["srcloc"] = (int)srcloc;

		result.append(zone_info);
	}
	return result;
}

Array InsightsTracyBridge::get_gpu_context_list() const {
	Array result;
	if (!m_worker) return result;

	const auto &gpuCtx = m_worker->GetGpuData();
	for (const auto &ctx : gpuCtx) {
		Dictionary ctx_info;
		ctx_info["id"] = (int)ctx->context;
		ctx_info["name"] = String(m_worker->GetGpuContextName(ctx->context));
		ctx_info["type"] = (int)ctx->type;
		result.append(ctx_info);
	}
	return result;
}

Array InsightsTracyBridge::get_gpu_zones(uint32_t p_ctx_id, int64_t p_start_ns, int64_t p_end_ns) const {
	Array result;
	if (!m_worker) return result;

	const auto &gpuCtx = m_worker->GetGpuData();
	for (const auto &ctx : gpuCtx) {
		if (ctx->context != p_ctx_id) continue;

		const auto &timeline = ctx->timeline;
		for (size_t i = 0; i < timeline.size(); i++) {
			const auto &ge = timeline[i];
			if (!ge) continue;

			int64_t gpu_start = (int64_t)ge->GpuStart();
			int64_t gpu_end = (int64_t)ge->GpuEnd();

			if (p_start_ns >= 0 && gpu_end >= 0 && gpu_end < p_start_ns) continue;
			if (p_end_ns >= 0 && gpu_start > p_end_ns) continue;

			Dictionary zone_info;
			zone_info["gpu_start_ns"] = gpu_start;
			zone_info["gpu_end_ns"] = gpu_end;
			zone_info["cpu_start_ns"] = (int64_t)ge->CpuStart();
			zone_info["cpu_end_ns"] = (int64_t)ge->CpuEnd();

			int16_t srcloc = ge->Srcloc();
			const auto &sl = m_worker->GetSourceLocation(srcloc);
			zone_info["name"] = String(m_worker->GetZoneName(sl));

			result.append(zone_info);
		}
	}
	return result;
}

Array InsightsTracyBridge::get_frame_sets() const {
	Array result;
	if (!m_worker) return result;

	const auto &frames = m_worker->GetFrames();
	for (const auto &fd : frames) {
		Dictionary frame_set;
		frame_set["name"] = String(fd->name);
		frame_set["continuous"] = fd->continuous;

		Array frames_arr;
		const auto &frameData = fd->data;
		for (size_t i = 0; i < frameData.size(); i++) {
			Dictionary frame_info;
			frame_info["start_ns"] = (int64_t)frameData[i].start;
			if (!fd->continuous) {
				frame_info["end_ns"] = (int64_t)frameData[i].end;
			}
			frames_arr.append(frame_info);
		}
		frame_set["frames"] = frames_arr;
		result.append(frame_set);
	}
	return result;
}

Dictionary InsightsTracyBridge::get_memory_stats() const {
	Dictionary result;
	if (!m_worker) return result;

	const auto &memMap = m_worker->GetMemNameMap();
	uint64_t total_alloc = 0;
	uint64_t total_free = 0;
	uint64_t active_allocs = 0;

	for (const auto &pair : memMap) {
		const auto *md = pair.second;
		if (!md) continue;
		active_allocs += md->active.size();
		total_alloc += md->active.size();
	}

	result["active_allocations"] = (int64_t)active_allocs;
	return result;
}

Array InsightsTracyBridge::get_messages(int64_t p_start_ns, int64_t p_end_ns) const {
	Array result;
	if (!m_worker) return result;

	const auto &messages = m_worker->GetMessages();
	for (size_t i = 0; i < messages.size(); i++) {
		const auto &msg = messages[i];
		if (!msg) continue;

		int64_t time_ns = (int64_t)msg->time;
		if (p_start_ns >= 0 && time_ns < p_start_ns) continue;
		if (p_end_ns >= 0 && time_ns > p_end_ns) continue;

		Dictionary msg_info;
		msg_info["time_ns"] = time_ns;
		msg_info["text"] = String(m_worker->GetString(msg->ref));
		result.append(msg_info);
	}
	return result;
}

Array InsightsTracyBridge::get_plots() const {
	Array result;
	if (!m_worker) return result;

	const auto &plots = m_worker->GetPlots();
	for (const auto &plot : plots) {
		Dictionary plot_info;
		plot_info["name"] = String(m_worker->GetString(plot->name));

		Array data_points;
		for (const auto &dp : plot->data) {
			Dictionary point;
			point["time_ns"] = (int64_t)dp.time;
			point["value"] = dp.val;
			data_points.append(point);
		}
		plot_info["data"] = data_points;
		result.append(plot_info);
	}
	return result;
}

int64_t InsightsTracyBridge::get_last_time() const {
	if (!m_worker) return 0;
	return m_worker->GetLastTime();
}

int InsightsTracyBridge::get_frame_count() const {
	if (!m_worker) return 0;
	return m_worker->GetFrameCount();
}

int InsightsTracyBridge::get_zone_count() const {
	if (!m_worker) return 0;
	int count = 0;
	const auto &threads = m_worker->GetThreadData();
	for (const auto &td : threads) {
		count += (int)td->timeline.size();
	}
	return count;
}

Error InsightsTracyBridge::populate_database(const Ref<InsightsDatabase> &p_db) {
	if (!m_worker || !m_worker->HasData()) {
		return ERR_UNAVAILABLE;
	}
	if (!p_db.is_valid()) {
		return ERR_INVALID_PARAMETER;
	}

	// Open the database for writing
	p_db->open("tracy_bridge_data");

	// 1. CPU Zones: iterate all threads
	const auto &threads = m_worker->GetThreadData();
	for (const auto &td : threads) {
		uint64_t tid = td->id;
		const auto &timeline = td->timeline;
		for (size_t i = 0; i < timeline.size(); i++) {
			const auto &ze = timeline[i];
			if (!ze) continue;

			int64_t start_ns = (int64_t)ze->Start();
			int64_t end_ns = ze->IsEndValid() ? (int64_t)ze->End() : -1;
			if (end_ns < 0) continue; // Skip incomplete zones

			int16_t srcloc = ze->Srcloc();
			const auto &sl = m_worker->GetSourceLocation(srcloc);
			String name = String(m_worker->GetZoneName(sl));
			String function = String(sl.function.active ? m_worker->GetString(sl.function.str) : "");
			String file = String(sl.file.active ? m_worker->GetString(sl.file.str) : "");
			int line = (int)sl.line;

			// Determine channel from zone name prefix
			String channel = "cpu";
			if (name.begins_with("godot:gpu/") || name.begins_with("gpu/")) {
				channel = "gpu";
			} else if (name.begins_with("godot:memory/") || name.begins_with("memory/")) {
				channel = "memory";
			} else if (name.begins_with("godot:loading/") || name.begins_with("loading/")) {
				channel = "loading";
			} else if (name.begins_with("godot:network/") || name.begins_with("network/")) {
				channel = "network";
			} else if (name.begins_with("godot:script/") || name.begins_with("script/")) {
				channel = "script";
			}

			p_db->insert_zone(name, file, line, function, channel, tid, (uint64_t)start_ns, (uint64_t)end_ns, 0, -1);
		}
	}

	// 2. GPU Zones
	const auto &gpuCtx = m_worker->GetGpuData();
	for (const auto &ctx : gpuCtx) {
		uint32_t ctx_id = ctx->context;
		const auto &timeline = ctx->timeline;
		for (size_t i = 0; i < timeline.size(); i++) {
			const auto &ge = timeline[i];
			if (!ge) continue;

			int64_t gpu_start = (int64_t)ge->GpuStart();
			int64_t gpu_end = (int64_t)ge->GpuEnd();
			if (gpu_start < 0 || gpu_end < 0) continue;

			int16_t srcloc = ge->Srcloc();
			const auto &sl = m_worker->GetSourceLocation(srcloc);
			String name = String(m_worker->GetZoneName(sl));

			p_db->insert_gpu_zone(name, (uint64_t)ctx_id, (uint64_t)ge->CpuStart(), (uint64_t)gpu_start, (uint64_t)gpu_end, (int)ctx_id);
		}
	}

	// 3. Frame markers
	const auto &frames = m_worker->GetFrames();
	for (size_t fi = 0; fi < frames.size(); fi++) {
		const auto &fd = frames[fi];
		const auto &frameData = fd->data;
		for (size_t i = 0; i < frameData.size(); i++) {
			uint64_t start_ns = (uint64_t)frameData[i].start;
			uint64_t end_ns = fd->continuous ? start_ns : (uint64_t)frameData[i].end;
			p_db->insert_frame_marker((int)fi, start_ns, end_ns);
		}
	}

	p_db->close();
	return OK;
}

void InsightsTracyBridge::_bind_methods() {
	ClassDB::bind_method(D_METHOD("connect_to_client", "addr", "port"), &InsightsTracyBridge::connect_to_client, DEFVAL("127.0.0.1"), DEFVAL(8086));
	ClassDB::bind_method(D_METHOD("load_tracy_file", "path"), &InsightsTracyBridge::load_tracy_file);
	ClassDB::bind_method(D_METHOD("disconnect"), &InsightsTracyBridge::disconnect);
	ClassDB::bind_method(D_METHOD("is_connected"), &InsightsTracyBridge::is_connected);
	ClassDB::bind_method(D_METHOD("has_data"), &InsightsTracyBridge::has_data);
	ClassDB::bind_method(D_METHOD("save_tracy_file", "path"), &InsightsTracyBridge::save_tracy_file);
	ClassDB::bind_method(D_METHOD("get_thread_list"), &InsightsTracyBridge::get_thread_list);
	ClassDB::bind_method(D_METHOD("get_thread_zones", "tid", "start_ns", "end_ns"), &InsightsTracyBridge::get_thread_zones, DEFVAL(-1), DEFVAL(-1));
	ClassDB::bind_method(D_METHOD("get_gpu_context_list"), &InsightsTracyBridge::get_gpu_context_list);
	ClassDB::bind_method(D_METHOD("get_gpu_zones", "ctx_id", "start_ns", "end_ns"), &InsightsTracyBridge::get_gpu_zones, DEFVAL(-1), DEFVAL(-1));
	ClassDB::bind_method(D_METHOD("get_frame_sets"), &InsightsTracyBridge::get_frame_sets);
	ClassDB::bind_method(D_METHOD("get_memory_stats"), &InsightsTracyBridge::get_memory_stats);
	ClassDB::bind_method(D_METHOD("get_messages", "start_ns", "end_ns"), &InsightsTracyBridge::get_messages, DEFVAL(-1), DEFVAL(-1));
	ClassDB::bind_method(D_METHOD("get_plots"), &InsightsTracyBridge::get_plots);
	ClassDB::bind_method(D_METHOD("get_last_time"), &InsightsTracyBridge::get_last_time);
	ClassDB::bind_method(D_METHOD("get_frame_count"), &InsightsTracyBridge::get_frame_count);
	ClassDB::bind_method(D_METHOD("get_zone_count"), &InsightsTracyBridge::get_zone_count);
	ClassDB::bind_method(D_METHOD("populate_database", "db"), &InsightsTracyBridge::populate_database);
}

#endif // TRACY_SERVER_ENABLED
