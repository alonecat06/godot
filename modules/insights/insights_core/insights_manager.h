/**************************************************************************/
/*  insights_manager.h                                                    */
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

#include "core/object/object.h"
#include "core/string/ustring.h"
#include "core/templates/hash_map.h"
#include "core/templates/local_vector.h"

#include "modules/insights/insights_core/insights_database.h"
#include "modules/insights/insights_core/insights_capture.h"
#include "modules/insights/insights_core/native_capture.h"
#include "modules/insights/channels/insights_channel.h"
#include "modules/insights/channels/memory_channel.h"
#include "modules/insights/channels/log_channel.h"
#include "modules/insights/channels/script_channel.h"
#include "modules/insights/channels/loading_channel.h"
#include "modules/insights/channels/gpu_channel.h"

class InsightsManager : public Object {
	GDCLASS(InsightsManager, Object);

private:
	static InsightsManager *singleton;

	bool recording = false;
	String capture_path;

	Ref<InsightsDatabase> database;
	Ref<NativeCapture> capture;
	HashMap<StringName, InsightsChannel *> channels;

	// Built-in channels for quick access.
	MemoryChannel *memory_channel = nullptr;
	LogChannel *log_channel = nullptr;
	ScriptChannel *script_channel = nullptr;
	LoadingChannel *loading_channel = nullptr;
	GPUChannel *gpu_channel = nullptr;

	void _cleanup();
	void _register_builtin_channels();
	void _unregister_builtin_channels();

protected:
	static void _bind_methods();

public:
	static InsightsManager *get_singleton() { return singleton; }

	Error start_capture(const String &p_path = "res://capture.gitracy");
	String stop_capture();
	bool is_recording() const;

	void register_channel(InsightsChannel *p_channel);
	void unregister_channel(const StringName &p_name);
	InsightsChannel *get_channel(const StringName &p_name) const;
	int get_channel_count() const;

	MemoryChannel *get_memory_channel() const;
	LogChannel *get_log_channel() const;
	ScriptChannel *get_script_channel() const;
	LoadingChannel *get_loading_channel() const;
	GPUChannel *get_gpu_channel() const;

	void tick(double p_delta);

	Ref<InsightsDatabase> get_database() const;

	InsightsManager();
	~InsightsManager();
};
