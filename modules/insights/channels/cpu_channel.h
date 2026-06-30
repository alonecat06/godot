/**************************************************************************/
/*  cpu_channel.h                                                         */
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

class CPUChannel : public InsightsChannel {
	GDCLASS(CPUChannel, InsightsChannel);

public:
	struct ContentionEvent {
		String lock_name;
		uint64_t owner_thread = 0;
		uint64_t waiter_thread = 0;
		uint64_t wait_time_ns = 0;
		uint64_t acquire_ns = 0;
		uint64_t release_ns = 0;
	};

private:
	bool contention_tracking_enabled = false;
	LocalVector<ContentionEvent> contention_events;
	// Track active locks: lock_name -> {owner_thread, acquire_ns}
	struct ActiveLock {
		String lock_name;
		uint64_t owner_thread = 0;
		uint64_t acquire_ns = 0;
	};
	LocalVector<ActiveLock> active_locks;
	// Track pending attempts: lock_name -> {waiter_thread, attempt_ns}
	struct PendingAttempt {
		String lock_name;
		uint64_t waiter_thread = 0;
		uint64_t attempt_ns = 0;
	};
	LocalVector<PendingAttempt> pending_attempts;
	uint32_t max_contention_events = 10000;

protected:
	static void _bind_methods();

public:
	void enable_contention_tracking();
	void disable_contention_tracking();
	bool is_contention_tracking_enabled() const;

	void on_lock_acquire(const String &p_lock_name, uint64_t p_thread_id, uint64_t p_timestamp_ns = 0);
	void on_lock_attempt(const String &p_lock_name, uint64_t p_thread_id, uint64_t p_timestamp_ns = 0);
	void on_lock_release(const String &p_lock_name, uint64_t p_thread_id, uint64_t p_timestamp_ns = 0);

	TypedArray<Dictionary> get_contention_events() const;

	virtual void on_event(const Dictionary &p_event_data) override;
	virtual Dictionary serialize() override;

	CPUChannel();
	virtual ~CPUChannel();
};
