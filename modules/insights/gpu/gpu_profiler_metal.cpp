/**************************************************************************/
/*  gpu_profiler_metal.cpp                                                */
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

#include "modules/insights/gpu/gpu_profiler_metal.h"
#include "core/object/class_db.h"

uint32_t GPUProfilerMetal::write_timestamp(void *p_cmd_buffer, Stage p_stage) {
	if (!initialized) {
		WARN_PRINT("GPUProfilerMetal: write_timestamp called before initialization.");
		return 0;
	}

	// Metal uses command buffer start/end time rather than query pools,
	// so we just track the query index.
	uint32_t index = current_query;
	current_query++;
	return index;
}

bool GPUProfilerMetal::fetch_results(TimestampResult *p_results, uint32_t p_count) {
	// Stub - actual Metal timestamp operations require RD internals.
	return false;
}

void GPUProfilerMetal::begin_frame(void *p_cmd_buffer) {
	current_query = 0;
}

bool GPUProfilerMetal::is_supported() const {
	return initialized;
}

void GPUProfilerMetal::_bind_methods() {
	// is_supported is already bound in GPUTimestampQuery.
}

GPUProfilerMetal::GPUProfilerMetal() {
}

GPUProfilerMetal::~GPUProfilerMetal() {
}
