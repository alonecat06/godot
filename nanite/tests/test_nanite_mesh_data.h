/**************************************************************************/
/*  test_nanite_mesh_data.h                                               */
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

#include "gpu/nanite_mesh_data.h"
#include "core/nanite_resource.h"
#include "tests/test_macros.h"

#include "servers/rendering/rendering_device.h"

#include <cstring>

namespace TestNaniteMeshData {

// NOTE: These tests require a Vulkan backend (RenderingDevice::get_singleton() != nullptr).
// On OpenGL/headless they SKIP rather than FAIL.

TEST_CASE("[Nanite][MeshData] upload_creates_valid_rids") {
	RenderingDevice *rd = RenderingDevice::get_singleton();
	if (!rd) {
		// SKIP — no Vulkan backend available.
		return;
	}

	// Build a minimal NaniteMeshResource with small populated blobs.
	Ref<NaniteMeshResource> res;
	res.instantiate();
	REQUIRE(res.is_valid());

	PackedByteArray cluster_data;
	cluster_data.resize(16);
	memset(cluster_data.ptrw(), 0xAB, 16);
	res->set_clusters_data(cluster_data);

	PackedByteArray vertex_data;
	vertex_data.resize(16);
	memset(vertex_data.ptrw(), 0xCD, 16);
	res->set_vertex_data(vertex_data);

	PackedByteArray nodes_data;
	nodes_data.resize(16);
	memset(nodes_data.ptrw(), 0x12, 16);
	res->set_nodes_data(nodes_data);

	PackedByteArray page_data;
	page_data.resize(16);
	memset(page_data.ptrw(), 0x34, 16);
	res->set_page_table_data(page_data);

	// Task 1.16.5.7 — materials_data + meshlet_vertices_data +
	// meshlet_triangles_data are uploaded as separate SSBOs.
	PackedByteArray materials_data;
	materials_data.resize(32); // 1 material × 32 bytes
	memset(materials_data.ptrw(), 0xEF, 32);
	res->set_materials_data(materials_data);

	PackedByteArray meshlet_vertices_data;
	meshlet_vertices_data.resize(16);
	memset(meshlet_vertices_data.ptrw(), 0x55, 16);
	res->set_meshlet_vertices_data(meshlet_vertices_data);

	PackedByteArray meshlet_triangles_data;
	meshlet_triangles_data.resize(16);
	memset(meshlet_triangles_data.ptrw(), 0x77, 16);
	res->set_meshlet_triangles_data(meshlet_triangles_data);

	NaniteMeshData md;
	md.upload_to_gpu(rd, res.ptr());

	CHECK(md.cluster_ssbo.is_valid());
	CHECK(md.vertex_ssbo.is_valid());
	CHECK(md.bvh_ssbo.is_valid());
	CHECK(md.page_ssbo.is_valid());
	// Task 1.16.5.7 — materials_ssbo + meshlet_*_ssbo must be created.
	CHECK(md.materials_ssbo.is_valid());
	CHECK(md.meshlet_vertices_ssbo.is_valid());
	CHECK(md.meshlet_triangles_ssbo.is_valid());
	CHECK(md.is_gpu_uploaded());

	md.free_gpu_resources(rd);
	CHECK_FALSE(md.is_gpu_uploaded());
	CHECK_FALSE(md.cluster_ssbo.is_valid());
	CHECK_FALSE(md.vertex_ssbo.is_valid());
	CHECK_FALSE(md.bvh_ssbo.is_valid());
	CHECK_FALSE(md.page_ssbo.is_valid());
	CHECK_FALSE(md.materials_ssbo.is_valid());
	CHECK_FALSE(md.meshlet_vertices_ssbo.is_valid());
	CHECK_FALSE(md.meshlet_triangles_ssbo.is_valid());
}

TEST_CASE("[Nanite][MeshData] upload_handles_empty_blobs") {
	RenderingDevice *rd = RenderingDevice::get_singleton();
	if (!rd) {
		// SKIP — no Vulkan backend available.
		return;
	}

	// A resource with all-empty blobs must still produce valid RIDs via
	// the 4-byte placeholder path (zero-sized buffers cause driver issues).
	Ref<NaniteMeshResource> res;
	res.instantiate();
	REQUIRE(res.is_valid());

	NaniteMeshData md;
	md.upload_to_gpu(rd, res.ptr());

	CHECK(md.cluster_ssbo.is_valid());
	CHECK(md.vertex_ssbo.is_valid());
	CHECK(md.bvh_ssbo.is_valid());
	CHECK(md.page_ssbo.is_valid());
	CHECK(md.is_gpu_uploaded());

	md.free_gpu_resources(rd);
	CHECK_FALSE(md.is_gpu_uploaded());
}

TEST_CASE("[Nanite][MeshData] upload_is_idempotent") {
	RenderingDevice *rd = RenderingDevice::get_singleton();
	if (!rd) {
		// SKIP — no Vulkan backend available.
		return;
	}

	Ref<NaniteMeshResource> res;
	res.instantiate();
	REQUIRE(res.is_valid());

	PackedByteArray data;
	data.resize(8);
	memset(data.ptrw(), 0x01, 8);
	res->set_clusters_data(data);

	NaniteMeshData md;
	md.upload_to_gpu(rd, res.ptr());
	REQUIRE(md.is_gpu_uploaded());

	RID first_cluster = md.cluster_ssbo;
	CHECK(first_cluster.is_valid());

	// Second upload must be a no-op (does not overwrite the RIDs).
	md.upload_to_gpu(rd, res.ptr());
	CHECK(md.cluster_ssbo == first_cluster);

	md.free_gpu_resources(rd);
}

// Task 1.16.5.7 — materials_ssbo must be created when the resource has
// materials_data, and must be byte-equal to the source blob.
TEST_CASE("[Nanite][MeshData] materials_ssbo_created") {
	RenderingDevice *rd = RenderingDevice::get_singleton();
	if (!rd) {
		// SKIP — no Vulkan backend available.
		return;
	}

	Ref<NaniteMeshResource> res;
	res.instantiate();
	REQUIRE(res.is_valid());

	// Encode one material: vec4 base_color(0.8, 0.8, 0.8, 1.0) + vec4
	// (metallic=0.0, roughness=0.5, 0, 0). 32 bytes total per material.
	PackedByteArray materials_data;
	materials_data.resize(32);
	{
		float *w = reinterpret_cast<float *>(materials_data.ptrw());
		w[0] = 0.8f; w[1] = 0.8f; w[2] = 0.8f; w[3] = 1.0f; // base_color
		w[4] = 0.0f; w[5] = 0.5f; w[6] = 0.0f; w[7] = 0.0f; // metallic, roughness, pad
	}
	res->set_materials_data(materials_data);

	NaniteMeshData md;
	md.upload_to_gpu(rd, res.ptr());
	REQUIRE(md.is_gpu_uploaded());
	CHECK(md.materials_ssbo.is_valid());

	// Verify byte content matches the source blob.
	PackedByteArray readback = rd->buffer_get_data(md.materials_ssbo);
	CHECK(readback.size() == materials_data.size());
	if (readback.size() == materials_data.size()) {
		CHECK(memcmp(readback.ptr(), materials_data.ptr(), materials_data.size()) == 0);
	}

	md.free_gpu_resources(rd);
	CHECK_FALSE(md.materials_ssbo.is_valid());
}

} // namespace TestNaniteMeshData
