/**************************************************************************/
/*  insights.h                                                            */
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

// This header provides extended profiling macros for the Godot Insights system.
// These macros add channel-awareness and structured naming on top of the base
// profiling macros in profiling.h.
//
// Like profiling.h, it is recommended to include this header only in .cpp files
// to minimize recompile cost when the profiler configuration changes.
//
// When GODOT_USE_TRACY is not defined, all macros are no-op stubs with zero
// runtime overhead.

#if defined(GODOT_USE_TRACY)

// Category color constants for Tracy zones.
// These colors match the InsightsChannel::get_category_color() mapping.
#define GODOT_INSIGHTS_COLOR_CPU 0x7AC0E5
#define GODOT_INSIGHTS_COLOR_GPU 0xC0E57A
#define GODOT_INSIGHTS_COLOR_MEMORY 0xE57AC0
#define GODOT_INSIGHTS_COLOR_SCRIPT 0x478CBF
#define GODOT_INSIGHTS_COLOR_LOADING 0xE5C77A
#define GODOT_INSIGHTS_COLOR_NETWORK 0xE5E57A
#define GODOT_INSIGHTS_COLOR_LOG 0xC0C0C0
#define GODOT_INSIGHTS_COLOR_CUSTOM 0xFF69B4

// Channel-aware zone macro.
// Creates a Tracy zone with the given name and applies the category color.
// category: A GODOT_INSIGHTS_COLOR_* color constant (e.g. GODOT_INSIGHTS_COLOR_CPU).
// name: The full zone name (e.g. "godot:physics/3d/step").
#define GodotProfileZoneC(category, name) \
	ZoneNamedN(GD_UNIQUE_NAME(__godot_insights_zone_), name, true); \
	ZoneColor(category)

// Hierarchical zone macro.
// Automatically constructs the zone name as "godot:subsystem/op1/op2".
// subsystem: The subsystem name (e.g. "physics", "rendering").
// op1: First operation level (e.g. "3d").
// op2: Second operation level (e.g. "step").
// Note: For color-coding, use GodotProfileZoneC with an explicit color constant.
#define GodotProfileZoneH(subsystem, op1, op2) \
	ZoneNamedN(GD_UNIQUE_NAME(__godot_insights_hzone_), "godot:" subsystem "/" op1 "/" op2, true)

// Async/fiber tracking macro.
// Marks the current thread as a fiber with the given name for Tracy's fiber view.
#define GodotProfileFiber(name) TracyFiberEnter(name)

// Counter/plot macro.
// Records a named value that Tracy can display as a plot/chart.
#define GodotProfilePlot(name, value) TracyPlot(name, value)

// Custom message macro.
// Sends a text message to Tracy's message log.
#define GodotProfileMessage(text) TracyMessage(text, strlen(text))

// Resource load tracking macro.
// Creates a zone specifically for resource loading operations.
#define GodotProfileResourceLoad(path) \
	ZoneNamedN(GD_UNIQUE_NAME(__godot_insights_load_), "godot:loading/resource/load", true)

// GPU stage marker macro.
// Creates a GPU zone in Tracy for tracking GPU command execution time.
#define GodotProfileGpuStage(name) TracyGpuZone(name)

#else // !GODOT_USE_TRACY

// No profiling; all macros are stubs.

// Category color constants (defined as 0 when profiling is disabled).
#define GODOT_INSIGHTS_COLOR_CPU 0
#define GODOT_INSIGHTS_COLOR_GPU 0
#define GODOT_INSIGHTS_COLOR_MEMORY 0
#define GODOT_INSIGHTS_COLOR_SCRIPT 0
#define GODOT_INSIGHTS_COLOR_LOADING 0
#define GODOT_INSIGHTS_COLOR_NETWORK 0
#define GODOT_INSIGHTS_COLOR_LOG 0
#define GODOT_INSIGHTS_COLOR_CUSTOM 0

// Channel-aware zone macro.
#define GodotProfileZoneC(category, name)
// Hierarchical zone macro.
#define GodotProfileZoneH(subsystem, op1, op2)
// Async/fiber tracking macro.
#define GodotProfileFiber(name)
// Counter/plot macro.
#define GodotProfilePlot(name, value)
// Custom message macro.
#define GodotProfileMessage(text)
// Resource load tracking macro.
#define GodotProfileResourceLoad(path)
// GPU stage marker macro.
#define GodotProfileGpuStage(name)

#endif // GODOT_USE_TRACY
