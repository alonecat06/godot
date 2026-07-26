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

#include "nanite/gpu/nanite_gpu_pipeline.h"
#include "tests/test_macros.h"

#include "nanite/core/nanite_builder.h"
#include "nanite/core/nanite_resource.h"
#include "nanite/gpu/nanite_mesh_data.h"
#include "nanite/tests/test_helpers.h"

#include "servers/rendering/rendering_device.h"

#include <cstring>

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

// Task 1.16.13.1 — full cull + rasterize end-to-end test using the real
// algorithms (per-cluster frustum/backface/HZB cull + soft rasterization).
// Builds a sphere NaniteMeshResource, uploads it, runs cull + rasterize, and
// verifies the vis_buffer has at least one non-zero pixel (i.e. the soft
// rasterizer covered at least one pixel).
TEST_CASE("[Nanite][GPUPipeline] cull_produces_visible_list") {
	RenderingDevice *rd = RenderingDevice::get_singleton();
	if (!rd) {
		// SKIP — no Vulkan backend available.
		return;
	}

	// Build a sphere resource via the Stage 0 builder.
	Ref<ArrayMesh> sphere = NaniteTestHelpers::create_sphere_mesh(32);
	REQUIRE(sphere.is_valid());
	Ref<NaniteBuilder> builder;
	builder.instantiate();
	Ref<NaniteMeshResource> res = builder->build(sphere);
	REQUIRE(res.is_valid());
	REQUIRE(res->get_cluster_count() > 0);

	// Upload to GPU.
	NaniteMeshData md;
	md.upload_to_gpu(rd, res.ptr());
	REQUIRE(md.is_gpu_uploaded());

	NaniteGPUPipeline pipeline;
	pipeline.init(rd);
	REQUIRE(pipeline.is_initialized());

	// Stage 1: identity camera + small screen. The real cull shader may
	// cull some clusters via frustum/backface/HZB tests, but with the
	// camera at origin (identity view) and the sphere centered at origin,
	// at least one cluster must remain visible.
	const int screen_w = 64;
	const int screen_h = 64;
	pipeline.ensure_screen_buffers(rd, screen_w, screen_h);

	NaniteGPUPipeline::CullParams params;
	static const float identity[16] = {
		1, 0, 0, 0,
		0, 1, 0, 0,
		0, 0, 1, 0,
		0, 0, 0, 1
	};
	memcpy(params.view_matrix, identity, sizeof(identity));
	memcpy(params.projection, identity, sizeof(identity));
	memcpy(params.model_matrix, identity, sizeof(identity));
	params.screen_size[0] = screen_w;
	params.screen_size[1] = screen_h;
	params.error_threshold = 0.01f;
	params.bvh_node_count = 1;
	params.cluster_count = res->get_cluster_count();

	RID visible_buffer = pipeline.dispatch_cull(rd, params, &md);
	CHECK(visible_buffer.is_valid());

	// Stage 1 simplification: dispatch rasterize with cluster_count as the
	// visible_count upper bound (extra threads return early via the shader's
	// bounds check).
	pipeline.dispatch_rasterize(rd, visible_buffer, res->get_cluster_count(), &md, identity);

	rd->barrier();

	// vis_buffer is R32_UINT; at least one pixel must be non-zero (i.e.
	// the soft rasterizer covered at least one pixel of the sphere).
	PackedByteArray vis_data = rd->texture_get_data(pipeline.get_vis_buffer(), 0);
	REQUIRE(vis_data.size() >= screen_w * screen_h * 4);
	const uint32_t *vis_u32 = reinterpret_cast<const uint32_t *>(vis_data.ptr());
	uint32_t non_zero_pixels = 0;
	for (int i = 0; i < screen_w * screen_h; ++i) {
		if (vis_u32[i] != 0) {
			++non_zero_pixels;
		}
	}
	CHECK(non_zero_pixels > 0);

	md.free_gpu_resources(rd);
	pipeline.cleanup(rd);
}

// Task 1.16.13.1 — rasterize produces non-zero VisBuffer coverage with real
// soft-rasterization. Complements `cull_produces_visible_list` by focusing on
// the rasterize step's per-triangle barycentric coverage test.
TEST_CASE("[Nanite][GPUPipeline] rasterize_produces_nonzero_visbuffer") {
	RenderingDevice *rd = RenderingDevice::get_singleton();
	if (!rd) {
		// SKIP — no Vulkan backend available.
		return;
	}

	Ref<ArrayMesh> sphere = NaniteTestHelpers::create_sphere_mesh(32);
	REQUIRE(sphere.is_valid());
	Ref<NaniteBuilder> builder;
	builder.instantiate();
	Ref<NaniteMeshResource> res = builder->build(sphere);
	REQUIRE(res.is_valid());

	NaniteMeshData md;
	md.upload_to_gpu(rd, res.ptr());
	REQUIRE(md.is_gpu_uploaded());

	NaniteGPUPipeline pipeline;
	pipeline.init(rd);
	REQUIRE(pipeline.is_initialized());

	const int screen_w = 64;
	const int screen_h = 64;
	pipeline.ensure_screen_buffers(rd, screen_w, screen_h);

	// Force every cluster visible by using identity camera with the sphere
	// centered at origin (frustum/backface/HZB tests may still cull some,
	// but enough will pass to exercise the rasterizer).
	NaniteGPUPipeline::CullParams params;
	static const float identity[16] = {
		1, 0, 0, 0,
		0, 1, 0, 0,
		0, 0, 1, 0,
		0, 0, 0, 1
	};
	memcpy(params.view_matrix, identity, sizeof(identity));
	memcpy(params.projection, identity, sizeof(identity));
	memcpy(params.model_matrix, identity, sizeof(identity));
	params.screen_size[0] = screen_w;
	params.screen_size[1] = screen_h;
	params.error_threshold = 0.01f;
	params.bvh_node_count = 1;
	params.cluster_count = res->get_cluster_count();

	RID visible_buffer = pipeline.dispatch_cull(rd, params, &md);
	REQUIRE(visible_buffer.is_valid());

	pipeline.dispatch_rasterize(rd, visible_buffer, res->get_cluster_count(), &md, identity);
	rd->barrier();

	// Soft rasterizer must cover at least one pixel.
	PackedByteArray vis_data = rd->texture_get_data(pipeline.get_vis_buffer(), 0);
	REQUIRE(vis_data.size() >= screen_w * screen_h * 4);
	const uint32_t *vis_u32 = reinterpret_cast<const uint32_t *>(vis_data.ptr());
	uint32_t non_zero = 0;
	for (int i = 0; i < screen_w * screen_h; ++i) {
		if (vis_u32[i] != 0) {
			++non_zero;
		}
	}
	CHECK(non_zero > 0);

	// Also check the depth buffer was written: at least one pixel should
	// have a depth != 1.0 (the cleared "far" value).
	PackedByteArray depth_data = rd->texture_get_data(pipeline.get_depth_buffer(), 0);
	REQUIRE(depth_data.size() >= screen_w * screen_h * 4);
	const float *depth_f32 = reinterpret_cast<const float *>(depth_data.ptr());
	uint32_t depth_written = 0;
	for (int i = 0; i < screen_w * screen_h; ++i) {
		if (depth_f32[i] < 1.0f) {
			++depth_written;
		}
	}
	CHECK(depth_written > 0);

	md.free_gpu_resources(rd);
	pipeline.cleanup(rd);
}

} // namespace TestNaniteGPUPipeline
