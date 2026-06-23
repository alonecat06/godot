/**************************************************************************/
/*  gpu_timestamp_query.h                                                 */
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
#include "core/variant/variant.h"

class GPUTimestampQuery : public Object {
	GDCLASS(GPUTimestampQuery, Object);

public:
	enum Stage {
		STAGE_BEGIN_RENDER_PASS = 0,
		STAGE_END_RENDER_PASS = 1,
		STAGE_DRAW = 2,
		STAGE_DISPATCH = 3,
		STAGE_PIPELINE_BARRIER = 4,
		STAGE_COPY = 5,
		STAGE_PRESENT = 6,
	};

	struct TimestampResult {
		uint64_t gpu_timestamp_ns = 0;
		uint64_t cpu_timestamp_ns = 0;
		String name;
		uint32_t query_index = 0;
	};

protected:
	static void _bind_methods();

	bool supported = false;
	float timestamp_period = 1.0f;
	uint32_t max_queries = 256;
	uint32_t current_query = 0;

public:
	virtual uint32_t write_timestamp(void *p_cmd_buffer, Stage p_stage) = 0;
	virtual bool fetch_results(TimestampResult *p_results, uint32_t p_count) = 0;

	virtual void begin_frame(void *p_cmd_buffer);
	virtual void end_frame();

	virtual bool is_supported() const;
	virtual float get_timestamp_period() const;

	uint32_t get_current_query_index() const;
	uint32_t get_max_queries() const;

	GPUTimestampQuery();
	virtual ~GPUTimestampQuery();
};
