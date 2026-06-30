/**************************************************************************/
/*  mono_profiler_bridge.cpp                                              */
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

#include "modules/insights/scripting/mono_profiler_bridge.h"

#include "core/object/class_db.h"
#include "modules/insights/insights_core/insights_manager.h"

ScriptChannel *MonoProfilerBridge::_get_script_channel() {
	InsightsManager *manager = InsightsManager::get_singleton();
	if (manager) {
		return manager->get_script_channel();
	}
	return nullptr;
}

void MonoProfilerBridge::enter_function(const String &p_name, const String &p_file, int p_line) {
	ScriptChannel *ch = _get_script_channel();
	if (ch) {
		ch->enter_function(p_name, p_file, p_line, ScriptChannel::LANGUAGE_C_SHARP);
	}
}

void MonoProfilerBridge::leave_function() {
	ScriptChannel *ch = _get_script_channel();
	if (ch) {
		ch->leave_function();
	}
}

void MonoProfilerBridge::suspend_function(const String &p_name) {
	ScriptChannel *ch = _get_script_channel();
	if (ch) {
		ch->suspend_function(p_name);
	}
}

void MonoProfilerBridge::resume_function(const String &p_name) {
	ScriptChannel *ch = _get_script_channel();
	if (ch) {
		ch->resume_function(p_name);
	}
}

void MonoProfilerBridge::bridge_enter_function(const String &p_name, const String &p_file, int p_line) {
	enter_function(p_name, p_file, p_line);
}

void MonoProfilerBridge::bridge_leave_function() {
	leave_function();
}

void MonoProfilerBridge::bridge_suspend_function(const String &p_name) {
	suspend_function(p_name);
}

void MonoProfilerBridge::bridge_resume_function(const String &p_name) {
	resume_function(p_name);
}

void MonoProfilerBridge::_bind_methods() {
	ClassDB::bind_method(D_METHOD("bridge_enter_function", "name", "file", "line"), &MonoProfilerBridge::bridge_enter_function);
	ClassDB::bind_method(D_METHOD("bridge_leave_function"), &MonoProfilerBridge::bridge_leave_function);
	ClassDB::bind_method(D_METHOD("bridge_suspend_function", "name"), &MonoProfilerBridge::bridge_suspend_function);
	ClassDB::bind_method(D_METHOD("bridge_resume_function", "name"), &MonoProfilerBridge::bridge_resume_function);
}

#ifdef MODULE_MONO_ENABLED
// TODO: Register Mono runtime profiler callbacks here.
// Example:
//   mono_profiler_install_enter(enter_function_callback, nullptr);
//   mono_profiler_install_leave(leave_function_callback, nullptr);
#endif
