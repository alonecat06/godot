/**************************************************************************/
/*  gpu_profiler_d3d12.cpp                                                */
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

#include "modules/insights/gpu/gpu_profiler_d3d12.h"
#include "core/object/class_db.h"

uint32_t GPUProfilerD3D12::write_timestamp(void *p_cmd_buffer, Stage p_stage) {
	if (!initialized) {
		WARN_PRINT("GPUProfilerD3D12: write_timestamp called before initialization.");
		return 0;
	}

	uint32_t index = current_query;
	current_query++;
	return index;
}

bool GPUProfilerD3D12::fetch_results(TimestampResult *p_results, uint32_t p_count) {
	// Stub - actual D3D12 timestamp query operations require RD internals.
	return false;
}

void GPUProfilerD3D12::begin_frame(void *p_cmd_buffer) {
	current_query = 0;
}

bool GPUProfilerD3D12::is_supported() const {
	return initialized;
}

void GPUProfilerD3D12::_bind_methods() {
	// is_supported is already bound in GPUTimestampQuery.
}

GPUProfilerD3D12::GPUProfilerD3D12() {
}

GPUProfilerD3D12::~GPUProfilerD3D12() {
}
