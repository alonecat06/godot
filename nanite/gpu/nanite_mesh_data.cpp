/**************************************************************************/
/*  nanite_mesh_data.cpp                                                  */
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

#include "nanite/gpu/nanite_mesh_data.h"

#include "nanite/core/nanite_resource.h"
#include "core/error/error_macros.h"
#include "servers/rendering/rendering_device.h"

void NaniteMeshData::upload_to_gpu(RenderingDevice *p_rd, const NaniteMeshResource *p_resource) {
	ERR_FAIL_NULL(p_rd);
	ERR_FAIL_NULL(p_resource);
	if (gpu_uploaded) {
		return;
	}

	// Create storage buffers (SSBOs) from each blob.
	// storage_buffer_create(size, data) — data is Span<uint8_t>; PackedByteArray
	// (a typedef of Vector<uint8_t>) implicitly converts to Span<uint8_t>.
	PackedByteArray clusters = p_resource->get_clusters_data();
	PackedByteArray vertices = p_resource->get_vertex_data();
	PackedByteArray nodes = p_resource->get_nodes_data();
	PackedByteArray pages = p_resource->get_page_table_data();
	PackedByteArray materials = p_resource->get_materials_data(); // Task 1.16.5
	PackedByteArray meshlet_vertices = p_resource->get_meshlet_vertices_data(); // Task 1.16.4
	PackedByteArray meshlet_triangles = p_resource->get_meshlet_triangles_data(); // Task 1.16.4

	// Zero-sized buffers cause driver issues on some backends, so allocate a
	// small 4-byte placeholder (with no initial data) when a blob is empty.
	auto create_ssbo = [p_rd](const PackedByteArray &p_data) -> RID {
		if (p_data.size() == 0) {
			return p_rd->storage_buffer_create(4);
		}
		return p_rd->storage_buffer_create(static_cast<uint32_t>(p_data.size()), p_data);
	};

	cluster_ssbo = create_ssbo(clusters);
	vertex_ssbo = create_ssbo(vertices);
	bvh_ssbo = create_ssbo(nodes);
	page_ssbo = create_ssbo(pages);
	materials_ssbo = create_ssbo(materials); // Task 1.16.5
	meshlet_vertices_ssbo = create_ssbo(meshlet_vertices); // Task 1.16.4
	meshlet_triangles_ssbo = create_ssbo(meshlet_triangles); // Task 1.16.4

	// Task 1.16.5 — materials_data is a flat byte blob; no separate material_rids
	// need to be collected (Stage 0 already had no `get_materials()` and Stage 1
	// resolves materials via `material_index` into materials_ssbo directly).
	// material_rids is left empty — kept in the struct for Stage 2+ when
	// full Material resources may need to be bound.

	gpu_uploaded = true;
}

void NaniteMeshData::free_gpu_resources(RenderingDevice *p_rd) {
	if (!gpu_uploaded || p_rd == nullptr) {
		return;
	}

	if (cluster_ssbo.is_valid()) {
		p_rd->free_rid(cluster_ssbo);
		cluster_ssbo = RID();
	}
	if (vertex_ssbo.is_valid()) {
		p_rd->free_rid(vertex_ssbo);
		vertex_ssbo = RID();
	}
	if (bvh_ssbo.is_valid()) {
		p_rd->free_rid(bvh_ssbo);
		bvh_ssbo = RID();
	}
	if (page_ssbo.is_valid()) {
		p_rd->free_rid(page_ssbo);
		page_ssbo = RID();
	}
	if (materials_ssbo.is_valid()) { // Task 1.16.5
		p_rd->free_rid(materials_ssbo);
		materials_ssbo = RID();
	}
	if (meshlet_vertices_ssbo.is_valid()) { // Task 1.16.4
		p_rd->free_rid(meshlet_vertices_ssbo);
		meshlet_vertices_ssbo = RID();
	}
	if (meshlet_triangles_ssbo.is_valid()) { // Task 1.16.4
		p_rd->free_rid(meshlet_triangles_ssbo);
		meshlet_triangles_ssbo = RID();
	}

	material_rids.clear();
	gpu_uploaded = false;
}
