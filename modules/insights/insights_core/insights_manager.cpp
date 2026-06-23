/**************************************************************************/
/*  insights_manager.cpp                                                  */
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

#include "insights_manager.h"

InsightsManager *InsightsManager::singleton = nullptr;

Error InsightsManager::start_capture(const String &p_path) {
	if (recording) {
		return ERR_ALREADY_IN_USE;
	}

	capture_path = p_path;

	// Create database.
	database.instantiate();
	Error err = database->open(p_path);
	if (err != OK) {
		database.unref();
		return err;
	}
	database->create_tables();

	// Create and start capture.
	capture.instantiate();
	capture->set_database(database);
	capture->start();

	// Register built-in channels.
	_register_builtin_channels();

	recording = true;
	return OK;
}

String InsightsManager::stop_capture() {
	if (!recording) {
		return "";
	}

	// Stop capture.
	if (capture.is_valid()) {
		capture->flush();
		capture->stop();
	}

	// Unregister built-in channels.
	_unregister_builtin_channels();

	// Save database to file.
	if (database.is_valid()) {
		String save_path = capture_path;
		if (save_path.is_empty()) {
			save_path = "res://capture.gitracy";
		}
		(void)database->save_to_file(save_path);
		database->close();
	}

	recording = false;
	String result_path = capture_path;
	capture.unref();
	database.unref();
	return result_path;
}

bool InsightsManager::is_recording() const {
	return recording;
}

void InsightsManager::register_channel(InsightsChannel *p_channel) {
	if (!p_channel) {
		return;
	}
	channels[p_channel->get_name()] = p_channel;
}

void InsightsManager::unregister_channel(const StringName &p_name) {
	channels.erase(p_name);
}

InsightsChannel *InsightsManager::get_channel(const StringName &p_name) const {
	HashMap<StringName, InsightsChannel *>::ConstIterator it = channels.find(p_name);
	if (it != channels.end()) {
		return it->value;
	}
	return nullptr;
}

int InsightsManager::get_channel_count() const {
	return channels.size();
}

void InsightsManager::tick(double p_delta) {
	if (!recording || capture.is_null()) {
		return;
	}

	// Forward frame tick to capture.
	// The frame time data will come from the engine's main loop.
	// For now, we just pass delta time.
	double frame_time = p_delta;
	double process_time = p_delta;
	double physics_time = 0.0;
	double physics_frame_time = 0.0;

	capture->on_frame(frame_time, process_time, physics_time, physics_frame_time);
}

Ref<InsightsDatabase> InsightsManager::get_database() const {
	return database;
}

void InsightsManager::_register_builtin_channels() {
	memory_channel = memnew(MemoryChannel);
	register_channel(memory_channel);

	log_channel = memnew(LogChannel);
	register_channel(log_channel);

	script_channel = memnew(ScriptChannel);
	register_channel(script_channel);

	loading_channel = memnew(LoadingChannel);
	register_channel(loading_channel);

	gpu_channel = memnew(GPUChannel);
	register_channel(gpu_channel);
}

void InsightsManager::_unregister_builtin_channels() {
	if (memory_channel) {
		unregister_channel(memory_channel->get_name());
		memdelete(memory_channel);
		memory_channel = nullptr;
	}
	if (log_channel) {
		unregister_channel(log_channel->get_name());
		memdelete(log_channel);
		log_channel = nullptr;
	}
	if (script_channel) {
		unregister_channel(script_channel->get_name());
		memdelete(script_channel);
		script_channel = nullptr;
	}
	if (loading_channel) {
		unregister_channel(loading_channel->get_name());
		memdelete(loading_channel);
		loading_channel = nullptr;
	}
	if (gpu_channel) {
		unregister_channel(gpu_channel->get_name());
		memdelete(gpu_channel);
		gpu_channel = nullptr;
	}
}

MemoryChannel *InsightsManager::get_memory_channel() const {
	return memory_channel;
}

LogChannel *InsightsManager::get_log_channel() const {
	return log_channel;
}

ScriptChannel *InsightsManager::get_script_channel() const {
	return script_channel;
}

LoadingChannel *InsightsManager::get_loading_channel() const {
	return loading_channel;
}

GPUChannel *InsightsManager::get_gpu_channel() const {
	return gpu_channel;
}

void InsightsManager::_cleanup() {
	if (recording) {
		(void)stop_capture();
	}
}

void InsightsManager::_bind_methods() {
	ClassDB::bind_method(D_METHOD("start_capture", "path"), &InsightsManager::start_capture, DEFVAL("res://capture.gitracy"));
	ClassDB::bind_method(D_METHOD("stop_capture"), &InsightsManager::stop_capture);
	ClassDB::bind_method(D_METHOD("is_recording"), &InsightsManager::is_recording);

	ClassDB::bind_method(D_METHOD("register_channel", "channel"), &InsightsManager::register_channel);
	ClassDB::bind_method(D_METHOD("unregister_channel", "name"), &InsightsManager::unregister_channel);
	ClassDB::bind_method(D_METHOD("get_channel", "name"), &InsightsManager::get_channel);
	ClassDB::bind_method(D_METHOD("get_channel_count"), &InsightsManager::get_channel_count);

	ClassDB::bind_method(D_METHOD("get_memory_channel"), &InsightsManager::get_memory_channel);
	ClassDB::bind_method(D_METHOD("get_log_channel"), &InsightsManager::get_log_channel);
	ClassDB::bind_method(D_METHOD("get_script_channel"), &InsightsManager::get_script_channel);
	ClassDB::bind_method(D_METHOD("get_loading_channel"), &InsightsManager::get_loading_channel);
	ClassDB::bind_method(D_METHOD("get_gpu_channel"), &InsightsManager::get_gpu_channel);

	ClassDB::bind_method(D_METHOD("tick", "delta"), &InsightsManager::tick);
	ClassDB::bind_method(D_METHOD("get_database"), &InsightsManager::get_database);
}

InsightsManager::InsightsManager() {
	singleton = this;
}

InsightsManager::~InsightsManager() {
	_cleanup();
	if (singleton == this) {
		singleton = nullptr;
	}
}
