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
#ifdef TOOLS_ENABLED
#include "editor/plugins/editor_plugin.h"
#endif

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
#include "modules/insights/gpu/gpu_timestamp_query.h"
#include "modules/insights/gpu/gpu_profiler_vulkan.h"
#include "modules/insights/gpu/gpu_profiler_d3d12.h"
#include "modules/insights/gpu/gpu_profiler_metal.h"
#include "modules/insights/channels/gpu_channel.h"
#include "modules/insights/insights_core/insights_comparator.h"
#include "modules/insights/insights_core/insights_replay.h"
#include "modules/insights/scripting/mono_profiler_bridge.h"
#include "modules/insights/scripting/gdscript_profiler_decorator.h"
#include "modules/insights/tools/insights_launcher.h"
#include "modules/insights/tools/tracy_converter.h"
#include "modules/insights/tools/insights_cli.h"
#include "modules/insights/channels/cpu_channel.h"
#include "modules/insights/channels/custom_channel.h"
#include "modules/insights/tools/web_exporter.h"
#include "modules/insights/insights_core/ai_analyzer.h"
#ifdef TRACY_SERVER_ENABLED
#include "modules/insights/insights_tracy_bridge.h"
#endif
#ifdef TOOLS_ENABLED
#include "modules/insights/editor/insights_contention_panel.h"
#include "modules/insights/editor/insights_editor_plugin.h"
#include "modules/insights/editor/insights_dock.h"
#include "modules/insights/editor/insights_timeline.h"
#include "modules/insights/editor/insights_flamegraph.h"
#include "modules/insights/editor/insights_memory_panel.h"
#include "modules/insights/editor/insights_loading_panel.h"
#include "modules/insights/editor/insights_network_panel.h"
#include "modules/insights/editor/insights_compare_panel.h"
#endif

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
	GDREGISTER_ABSTRACT_CLASS(GPUTimestampQuery);
	GDREGISTER_CLASS(GPUProfilerVulkan);
	GDREGISTER_CLASS(GPUProfilerD3D12);
	GDREGISTER_CLASS(GPUProfilerMetal);
	GDREGISTER_CLASS(GPUChannel);
	GDREGISTER_CLASS(InsightsComparator);
	GDREGISTER_CLASS(InsightsReplay);
	GDREGISTER_CLASS(MonoProfilerBridge);
	GDREGISTER_CLASS(GDScriptProfilerDecorator);
	GDREGISTER_CLASS(InsightsLauncher);
	GDREGISTER_CLASS(TracyConverter);
	GDREGISTER_CLASS(InsightsCLI);
#ifdef TOOLS_ENABLED
	GDREGISTER_CLASS(InsightsTimeline);
	GDREGISTER_CLASS(InsightsFlamegraph);
	GDREGISTER_CLASS(InsightsMemoryPanel);
	GDREGISTER_CLASS(InsightsLoadingPanel);
	GDREGISTER_CLASS(InsightsNetworkPanel);
	GDREGISTER_CLASS(InsightsComparePanel);
	GDREGISTER_CLASS(InsightsDock);
	GDREGISTER_CLASS(InsightsEditorPlugin);
	EditorPlugins::add_by_type<InsightsEditorPlugin>();
#endif
	#ifdef TRACY_SERVER_ENABLED
	GDREGISTER_CLASS(InsightsTracyBridge);
#endif
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
