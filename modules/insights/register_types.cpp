/**************************************************************************/
/*  register_types.cpp                                                    */
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

#include "register_types.h"

#include "core/object/class_db.h"
#include "core/config/engine.h"

#include "modules/insights/insights_core/insights_manager.h"
#include "modules/insights/insights_core/insights_database.h"
#include "modules/insights/insights_core/insights_capture.h"
#include "modules/insights/insights_core/native_capture.h"
#include "modules/insights/insights_core/resource_load_tracker.h"
#include "modules/insights/channels/insights_channel.h"
#include "modules/insights/channels/memory_channel.h"
#include "modules/insights/channels/log_channel.h"
#include "modules/insights/channels/script_channel.h"
#include "modules/insights/channels/loading_channel.h"

void initialize_insights_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}

	GDREGISTER_CLASS(InsightsChannel);
	GDREGISTER_CLASS(InsightsDatabase);
	GDREGISTER_ABSTRACT_CLASS(InsightsCapture);
	GDREGISTER_CLASS(NativeCapture);
	GDREGISTER_CLASS(ResourceLoadTracker);
	GDREGISTER_CLASS(MemoryChannel);
	GDREGISTER_CLASS(LogChannel);
	GDREGISTER_CLASS(ScriptChannel);
	GDREGISTER_CLASS(LoadingChannel);
	GDREGISTER_CLASS(InsightsManager);

	InsightsManager *insights_manager = memnew(InsightsManager);
	Engine::get_singleton()->add_singleton(Engine::Singleton("InsightsManager", insights_manager));
}

void uninitialize_insights_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}

	InsightsManager *insights_manager = InsightsManager::get_singleton();
	if (insights_manager) {
		memdelete(insights_manager);
	}
}
