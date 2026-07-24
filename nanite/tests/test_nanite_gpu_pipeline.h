/**************************************************************************/
/*  test_nanite_gpu_pipeline.h                                            */
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

#include "gpu/nanite_gpu_pipeline.h"
#include "tests/test_macros.h"

#include "servers/rendering/rendering_device.h"

namespace TestNaniteGPUPipeline {

// NOTE: The GPU tests require a Vulkan backend (RenderingDevice::get_singleton()
// != nullptr). On OpenGL/headless they SKIP rather than FAIL.

TEST_CASE("[Nanite][GPUPipeline] class_exists_and_not_initialized_by_default") {
	// Static check: the class is constructible and starts uninitialized.
	NaniteGPUPipeline pipeline;
	CHECK_FALSE(pipeline.is_initialized());
	CHECK_FALSE(pipeline.get_vis_buffer().is_valid());
	CHECK_FALSE(pipeline.get_depth_buffer().is_valid());
}

TEST_CASE("[Nanite][GPUPipeline] init_creates_pipelines_and_hzb") {
	RenderingDevice *rd = RenderingDevice::get_singleton();
	if (!rd) {
		// SKIP — no Vulkan backend available.
		return;
	}

	NaniteGPUPipeline pipeline;
	pipeline.init(rd);
	REQUIRE(pipeline.is_initialized());
	CHECK(pipeline.get_hzb() != nullptr);
	CHECK(pipeline.get_hzb()->is_initialized());

	pipeline.cleanup(rd);
	CHECK_FALSE(pipeline.is_initialized());
}

TEST_CASE("[Nanite][GPUPipeline] ensure_screen_buffers_creates_textures") {
	RenderingDevice *rd = RenderingDevice::get_singleton();
	if (!rd) {
		// SKIP — no Vulkan backend available.
		return;
	}

	NaniteGPUPipeline pipeline;
	pipeline.init(rd);
	REQUIRE(pipeline.is_initialized());

	pipeline.ensure_screen_buffers(rd, 64, 64);
	CHECK(pipeline.get_vis_buffer().is_valid());
	CHECK(pipeline.get_depth_buffer().is_valid());

	// Re-calling with the same size must be a no-op (same RIDs kept).
	RID vis_before = pipeline.get_vis_buffer();
	RID depth_before = pipeline.get_depth_buffer();
	pipeline.ensure_screen_buffers(rd, 64, 64);
	CHECK(pipeline.get_vis_buffer() == vis_before);
	CHECK(pipeline.get_depth_buffer() == depth_before);

	// Resizing must replace the RIDs.
	pipeline.ensure_screen_buffers(rd, 128, 128);
	CHECK(pipeline.get_vis_buffer() != vis_before);
	CHECK(pipeline.get_depth_buffer() != depth_before);

	pipeline.cleanup(rd);
}

// Full cull + rasterize end-to-end test is deferred until a NaniteMeshResource
// with real cluster/vertex/bvh blobs is wired up in the harness. The
// dispatch_cull path requires a NaniteMeshData with valid SSBO RIDs; building
// that from scratch in a unit test is tracked separately.
// TODO: add "[Nanite][GPUPipeline] cull_produces_visible_list" once a mesh
// fixture helper exists.

} // namespace TestNaniteGPUPipeline
