/**************************************************************************/
/*  gpu_timestamp_query.cpp                                               */
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

#include "gpu_timestamp_query.h"

#include "core/object/class_db.h"
#include "core/variant/binder_common.h"

void GPUTimestampQuery::begin_frame(void *p_cmd_buffer) {
	current_query = 0;
}

void GPUTimestampQuery::end_frame() {
}

bool GPUTimestampQuery::is_supported() const {
	return supported;
}

float GPUTimestampQuery::get_timestamp_period() const {
	return timestamp_period;
}

uint32_t GPUTimestampQuery::get_current_query_index() const {
	return current_query;
}

uint32_t GPUTimestampQuery::get_max_queries() const {
	return max_queries;
}

void GPUTimestampQuery::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_supported"), &GPUTimestampQuery::is_supported);
	ClassDB::bind_method(D_METHOD("get_timestamp_period"), &GPUTimestampQuery::get_timestamp_period);
	ClassDB::bind_method(D_METHOD("get_max_queries"), &GPUTimestampQuery::get_max_queries);

	BIND_ENUM_CONSTANT(STAGE_BEGIN_RENDER_PASS);
	BIND_ENUM_CONSTANT(STAGE_END_RENDER_PASS);
	BIND_ENUM_CONSTANT(STAGE_DRAW);
	BIND_ENUM_CONSTANT(STAGE_DISPATCH);
	BIND_ENUM_CONSTANT(STAGE_PIPELINE_BARRIER);
	BIND_ENUM_CONSTANT(STAGE_COPY);
	BIND_ENUM_CONSTANT(STAGE_PRESENT);
}

GPUTimestampQuery::GPUTimestampQuery() {
}

GPUTimestampQuery::~GPUTimestampQuery() {
}

VARIANT_ENUM_CAST(GPUTimestampQuery::Stage);
