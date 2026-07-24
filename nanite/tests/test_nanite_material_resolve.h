/**************************************************************************/
/*  test_nanite_material_resolve.h                                        */
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

#include "core/nanite_builder.h"
#include "core/nanite_resource.h"
#include "gpu/nanite_mesh_data.h"
#include "tests/test_helpers.h"

#include "servers/rendering/rendering_device.h"

#include <cstring>
#include <unordered_set>

namespace TestNaniteMaterialResolve {

// NOTE: These tests require a Vulkan backend (RenderingDevice::get_singleton()
// != nullptr). On OpenGL/headless they SKIP rather than FAIL.
//
// The Stage 1 rasterize shader writes exactly one pixel per visible cluster
// (a placeholder coverage scheme — see nanite_rasterize.glsl). For the
// ">= 10% non-black" assertion to pass, the screen size must be small
// relative to the cluster count. We use an 8x8 target (64 pixels) with a
// 32-segment sphere (~16 clusters by default config), giving ~25% coverage.

// Builds a sphere NaniteMeshResource, uploads it to GPU SSBOs via
// NaniteMeshData, and runs cull -> rasterize -> material_resolve.
// Returns the NaniteMeshData (caller frees) and seeds the pipeline with
// screen buffers. Returns nullptr if any non-fatal precondition fails
// (build produced no clusters, upload failed, cull returned no buffer).
// Fatal failures (REQUIRE) are left to the caller so the doctest report
// points at the TEST_CASE, not the helper.
inline NaniteMeshData *build_upload_and_run_pipeline(NaniteGPUPipeline &p_pipeline, RenderingDevice *p_rd, int p_screen_w, int p_screen_h, int p_debug_mode) {
	Ref<ArrayMesh> sphere = NaniteTestHelpers::create_sphere_mesh(32);
	if (sphere.is_null()) {
		return nullptr;
	}

	Ref<NaniteBuilder> builder;
	builder.instantiate();
	Ref<NaniteMeshResource> res = builder->build(sphere);
	if (res.is_null() || res->get_cluster_count() == 0) {
		return nullptr;
	}

	NaniteMeshData *md = memnew(NaniteMeshData);
	md->upload_to_gpu(p_rd, res.ptr());
	if (!md->is_gpu_uploaded()) {
		memdelete(md);
		return nullptr;
	}

	p_pipeline.ensure_screen_buffers(p_rd, p_screen_w, p_screen_h);

	// Stage 1: identity camera; the cull shader is pass-through so matrices
	// don't affect the visible list.
	NaniteGPUPipeline::CullParams params;
	static const float identity[16] = {
		1, 0, 0, 0,
		0, 1, 0, 0,
		0, 0, 1, 0,
		0, 0, 0, 1
	};
	memcpy(params.view_matrix, identity, sizeof(identity));
	memcpy(params.projection, identity, sizeof(identity));
	params.screen_size[0] = p_screen_w;
	params.screen_size[1] = p_screen_h;
	params.error_threshold = 0.01f;
	params.bvh_node_count = 1;
	params.cluster_count = res->get_cluster_count();

	RID visible_buffer = p_pipeline.dispatch_cull(p_rd, params, md);
	if (!visible_buffer.is_valid()) {
		md->free_gpu_resources(p_rd);
		memdelete(md);
		return nullptr;
	}

	// Stage 1 cull emits cluster_count as visible_count (pass-through).
	p_pipeline.dispatch_rasterize(p_rd, visible_buffer, params.cluster_count, md);

	p_pipeline.dispatch_material_resolve(p_rd, p_pipeline.get_vis_buffer(), md, p_debug_mode);

	// Make the compute writes visible to the CPU read-back below.
	p_rd->barrier();

	return md;
}

// Reads the color_buffer back as RGBA8 (4 bytes/pixel) and counts the
// non-black pixels. A pixel is "black" if R == G == B == 0.
inline uint32_t count_non_black_pixels(RenderingDevice *p_rd, const RID &p_color_buffer, int p_width, int p_height) {
	PackedByteArray data = p_rd->texture_get_data(p_color_buffer, 0);
	if (data.size() < p_width * p_height * 4) {
		return 0;
	}
	const uint8_t *ptr = data.ptr();
	uint32_t non_black = 0;
	for (int i = 0; i < p_width * p_height; ++i) {
		uint8_t r = ptr[i * 4 + 0];
		uint8_t g = ptr[i * 4 + 1];
		uint8_t b = ptr[i * 4 + 2];
		if (r != 0 || g != 0 || b != 0) {
			++non_black;
		}
	}
	return non_black;
}

TEST_CASE("[Nanite][MaterialResolve] output_is_non_black") {
	RenderingDevice *rd = RenderingDevice::get_singleton();
	if (!rd) {
		// SKIP — no Vulkan backend available.
		return;
	}

	NaniteGPUPipeline pipeline;
	pipeline.init(rd);
	REQUIRE(pipeline.is_initialized());

	const int screen_w = 8;
	const int screen_h = 8;
	const int total_pixels = screen_w * screen_h;

	NaniteMeshData *md = build_upload_and_run_pipeline(pipeline, rd, screen_w, screen_h, 0 /* NONE */);
	REQUIRE(md != nullptr);

	uint32_t non_black = count_non_black_pixels(rd, pipeline.get_color_buffer(), screen_w, screen_h);
	// Stage 1 writes one pixel per visible cluster. The 32-segment sphere
	// yields ~16 clusters, all of which land inside the 8x8 target, so we
	// expect well above the 10% threshold (16/64 = 25%).
	CHECK(non_black * 10 >= (uint32_t)total_pixels); // >= 10%

	md->free_gpu_resources(rd);
	memdelete(md);
	pipeline.cleanup(rd);
}

TEST_CASE("[Nanite][MaterialResolve] cluster_solid_color_mode") {
	RenderingDevice *rd = RenderingDevice::get_singleton();
	if (!rd) {
		// SKIP — no Vulkan backend available.
		return;
	}

	NaniteGPUPipeline pipeline;
	pipeline.init(rd);
	REQUIRE(pipeline.is_initialized());

	const int screen_w = 8;
	const int screen_h = 8;

	// debug_mode = 1 (CLUSTER_SOLID_COLOR): hash_color(cluster_id).
	NaniteMeshData *md = build_upload_and_run_pipeline(pipeline, rd, screen_w, screen_h, 1 /* CLUSTER_SOLID_COLOR */);
	REQUIRE(md != nullptr);

	PackedByteArray data = rd->texture_get_data(pipeline.get_color_buffer(), 0);
	REQUIRE(data.size() >= screen_w * screen_h * 4);

	const uint8_t *ptr = data.ptr();
	std::unordered_set<uint32_t> unique_colors;
	for (int i = 0; i < screen_w * screen_h; ++i) {
		uint8_t r = ptr[i * 4 + 0];
		uint8_t g = ptr[i * 4 + 1];
		uint8_t b = ptr[i * 4 + 2];
		// Skip black pixels (no Nanite geometry).
		if (r == 0 && g == 0 && b == 0) {
			continue;
		}
		uint32_t packed = (uint32_t)r | ((uint32_t)g << 8) | ((uint32_t)b << 16);
		unique_colors.insert(packed);
	}

	// Multiple clusters must produce multiple distinct hash colors.
	// (Two cluster ids could theoretically collide under hash_color's
	// 8-bit mask, but with ~16 clusters and the chosen constants the
	// probability of all collisions is negligible.)
	CHECK(unique_colors.size() > 1);

	md->free_gpu_resources(rd);
	memdelete(md);
	pipeline.cleanup(rd);
}

} // namespace TestNaniteMaterialResolve
