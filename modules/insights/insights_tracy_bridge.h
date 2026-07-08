#pragma once

#ifdef TRACY_SERVER_ENABLED

#include "core/object/ref_counted.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"

#include "modules/insights/insights_core/insights_database.h"

#include <memory>
#include <cstdint>

namespace tracy { class Worker; }

class InsightsTracyBridge : public RefCounted {
    GDCLASS(InsightsTracyBridge, RefCounted);

public:
    InsightsTracyBridge();
    ~InsightsTracyBridge();

    // Connection management
    Error connect_to_client(const String &p_addr = "127.0.0.1", uint16_t p_port = 8086);
    Error load_tracy_file(const String &p_path);
    void disconnect();
    bool is_connected() const;
    bool has_data() const;

    // Save
    Error save_tracy_file(const String &p_path) const;

    // Data query API - returns Godot types
    Array get_thread_list() const;
    Array get_thread_zones(uint64_t p_tid, int64_t p_start_ns = -1, int64_t p_end_ns = -1) const;
    Array get_gpu_context_list() const;
    Array get_gpu_zones(uint32_t p_ctx_id, int64_t p_start_ns = -1, int64_t p_end_ns = -1) const;
    Array get_frame_sets() const;
    Dictionary get_memory_stats() const;
    Array get_messages(int64_t p_start_ns = -1, int64_t p_end_ns = -1) const;
    Array get_plots() const;
    int64_t get_last_time() const;
    int get_frame_count() const;
    int get_zone_count() const;

    // Bridge to InsightsDatabase
    Error populate_database(const Ref<InsightsDatabase> &p_db);

protected:
    static void _bind_methods();

private:
    std::unique_ptr<tracy::Worker> m_worker;
    bool m_is_live = false;
};

#else
// Stub: InsightsTracyBridge is not available without Tracy Server
#endif // TRACY_SERVER_ENABLED
