/**************************************************************************/
/*  nanite_resource.cpp                                                   */
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

#include "nanite_resource.h"

#include "core/error/error_macros.h"
#include "core/io/file_access.h"
#include "core/object/class_db.h"
#include "core/object/object.h"
#include "scene/resources/mesh.h"

#include "builder_config.h"

// ---------------------------------------------------------------------------
// Setters / getters
// ---------------------------------------------------------------------------

void NaniteMeshResource::set_vertex_data(const PackedByteArray &p_data) {
	vertex_data = p_data;
}

PackedByteArray NaniteMeshResource::get_vertex_data() const {
	return vertex_data;
}

void NaniteMeshResource::set_clusters_data(const PackedByteArray &p_data) {
	clusters_data = p_data;
}

PackedByteArray NaniteMeshResource::get_clusters_data() const {
	return clusters_data;
}

void NaniteMeshResource::set_nodes_data(const PackedByteArray &p_data) {
	nodes_data = p_data;
}

PackedByteArray NaniteMeshResource::get_nodes_data() const {
	return nodes_data;
}

void NaniteMeshResource::set_page_table_data(const PackedByteArray &p_data) {
	page_table_data = p_data;
}

PackedByteArray NaniteMeshResource::get_page_table_data() const {
	return page_table_data;
}

// Task 1.16.4 — meshlet vertex index + triangle micro-index pools.
void NaniteMeshResource::set_meshlet_vertices_data(const PackedByteArray &p_data) {
	meshlet_vertices_data = p_data;
}

PackedByteArray NaniteMeshResource::get_meshlet_vertices_data() const {
	return meshlet_vertices_data;
}

void NaniteMeshResource::set_meshlet_triangles_data(const PackedByteArray &p_data) {
	meshlet_triangles_data = p_data;
}

PackedByteArray NaniteMeshResource::get_meshlet_triangles_data() const {
	return meshlet_triangles_data;
}

// Task 1.16.2 — materials_data blob (encoded material parameters consumed by
// nanite_material_resolve.glsl). See layout comment in nanite_resource.h.
void NaniteMeshResource::set_materials_data(const PackedByteArray &p_data) {
	materials_data = p_data;
}

PackedByteArray NaniteMeshResource::get_materials_data() const {
	return materials_data;
}

void NaniteMeshResource::set_shadow_mesh(const Ref<ArrayMesh> &p_mesh) {
	// Validate that the assigned resource is actually an ArrayMesh (or a
	// subclass thereof). A null Ref is allowed — clears the field.
	if (p_mesh.is_null()) {
		shadow_mesh = p_mesh;
		return;
	}
	ArrayMesh *casted = Object::cast_to<ArrayMesh>(p_mesh.ptr());
	ERR_FAIL_COND_MSG(casted == nullptr, "NaniteMeshResource: shadow_mesh must be an ArrayMesh.");
	shadow_mesh = p_mesh;
}

Ref<ArrayMesh> NaniteMeshResource::get_shadow_mesh() const {
	return shadow_mesh;
}

void NaniteMeshResource::set_build_config(const Ref<BuilderConfig> &p_cfg) {
	if (p_cfg.is_null()) {
		build_config = p_cfg;
		return;
	}
	BuilderConfig *casted = Object::cast_to<BuilderConfig>(p_cfg.ptr());
	ERR_FAIL_COND_MSG(casted == nullptr, "NaniteMeshResource: build_config must be a BuilderConfig.");
	build_config = p_cfg;
}

Ref<BuilderConfig> NaniteMeshResource::get_build_config() const {
	return build_config;
}

void NaniteMeshResource::set_cluster_count(int p_count) {
	cluster_count = p_count;
}

int NaniteMeshResource::get_cluster_count() const {
	return cluster_count;
}

void NaniteMeshResource::set_node_count(int p_count) {
	node_count = p_count;
}

int NaniteMeshResource::get_node_count() const {
	return node_count;
}

void NaniteMeshResource::set_page_count(int p_count) {
	page_count = p_count;
}

int NaniteMeshResource::get_page_count() const {
	return page_count;
}

// ---------------------------------------------------------------------------
// .nanite binary format
// ---------------------------------------------------------------------------
//
// All integers are uint32 little-endian. Each data blob is preceded by its
// uint32 size and followed by 0-3 zero padding bytes to bring the file
// cursor back to a 4-byte boundary. The trailer carries the three integer
// counts as a sanity check (they must match the sizes implied by the blobs
// when the resource was produced by a correctly-built NaniteBuilder).

// File format version. Bumped only on backwards-incompatible layout changes.
// v1 → v2 (Task 1.16.1): NaniteCluster serialized size 64 → 68 bytes (added
//   uint32 material_index after group_id). Old .nanite v1 files fail to load
//   with "unsupported version" — rebuild from source mesh via NaniteBuilder.
// v2 → v3 (Task 1.16.4): vertex_data changed from meshopt-compressed (stride
//   12 B) to raw (stride 32 B: position.xyz + normal.xyz + uv.xy); clusters_data
//   changed from variable-length [meta+meshlet_bytes] per cluster to fixed
//   68-byte metadata only; meshlet geometry moved into two new blobs
//   (meshlet_vertices_data, meshlet_triangles_data). Old v2 files fail to
//   load — rebuild from source mesh via NaniteBuilder.
static const uint32_t NANITE_FORMAT_VERSION = 3;

// Magic bytes spell "NANM" when written in little-endian byte order.
static const uint8_t NANITE_MAGIC[4] = { 'N', 'A', 'N', 'M' };

// Writes a uint32 little-endian to p_f.
static inline void nanite_store_u32(FileAccess *p_f, uint32_t p_value) {
	p_f->store_8(static_cast<uint8_t>((p_value >> 0) & 0xFF));
	p_f->store_8(static_cast<uint8_t>((p_value >> 8) & 0xFF));
	p_f->store_8(static_cast<uint8_t>((p_value >> 16) & 0xFF));
	p_f->store_8(static_cast<uint8_t>((p_value >> 24) & 0xFF));
}

// Reads a uint32 little-endian from p_f.
static inline uint32_t nanite_load_u32(FileAccess *p_f) {
	uint32_t v = 0;
	v |= uint32_t(p_f->get_8()) << 0;
	v |= uint32_t(p_f->get_8()) << 8;
	v |= uint32_t(p_f->get_8()) << 16;
	v |= uint32_t(p_f->get_8()) << 24;
	return v;
}

// Writes a blob section: uint32 size, blob bytes, zero padding to 4-align.
static void nanite_store_blob(FileAccess *p_f, const PackedByteArray &p_data) {
	const uint32_t size = static_cast<uint32_t>(p_data.size());
	nanite_store_u32(p_f, size);
	if (size > 0) {
		p_f->store_buffer(p_data.ptr(), size);
	}
	// Pad to 4-byte alignment.
	const uint32_t rem = size % 4;
	if (rem != 0) {
		const uint32_t pad = 4 - rem;
		for (uint32_t i = 0; i < pad; ++i) {
			p_f->store_8(0);
		}
	}
}

// Reads a blob section previously written by nanite_store_blob().
// Returns ERR_OK on success and stores the bytes in r_out.
static Error nanite_load_blob(FileAccess *p_f, PackedByteArray &r_out) {
	const uint32_t size = nanite_load_u32(p_f);
	r_out.resize(size);
	if (size > 0) {
		const uint64_t got = p_f->get_buffer(r_out.ptrw(), size);
		if (got != size) {
			return ERR_FILE_CORRUPT;
		}
	}
	// Skip padding to 4-byte alignment.
	const uint32_t rem = size % 4;
	if (rem != 0) {
		const uint32_t pad = 4 - rem;
		for (uint32_t i = 0; i < pad; ++i) {
			(void)p_f->get_8();
		}
	}
	return OK;
}

Error NaniteMeshResource::save(const String &p_path) const {
	Error open_err = OK;
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::WRITE, &open_err);
	ERR_FAIL_COND_V_MSG(f.is_null(), open_err, vformat("NaniteMeshResource::save: cannot open '%s' for writing (err=%d).", p_path, open_err));

	// Magic + version.
	for (int i = 0; i < 4; ++i) {
		f->store_8(NANITE_MAGIC[i]);
	}
	nanite_store_u32(f.ptr(), NANITE_FORMAT_VERSION);

	// Seven blob sections (v3 adds meshlet_vertices_data + meshlet_triangles_data — Task 1.16.4).
	nanite_store_blob(f.ptr(), vertex_data);
	nanite_store_blob(f.ptr(), clusters_data);
	nanite_store_blob(f.ptr(), nodes_data);
	nanite_store_blob(f.ptr(), page_table_data);
	nanite_store_blob(f.ptr(), materials_data);
	nanite_store_blob(f.ptr(), meshlet_vertices_data); // Task 1.16.4
	nanite_store_blob(f.ptr(), meshlet_triangles_data); // Task 1.16.4

	// Sanity trailer.
	nanite_store_u32(f.ptr(), static_cast<uint32_t>(cluster_count));
	nanite_store_u32(f.ptr(), static_cast<uint32_t>(node_count));
	nanite_store_u32(f.ptr(), static_cast<uint32_t>(page_count));

	return OK;
}

Error NaniteMeshResource::load(const String &p_path) {
	Error open_err = OK;
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ, &open_err);
	ERR_FAIL_COND_V_MSG(f.is_null(), open_err, vformat("NaniteMeshResource::load: cannot open '%s' for reading (err=%d).", p_path, open_err));

	// Magic.
	for (int i = 0; i < 4; ++i) {
		const uint8_t b = f->get_8();
		if (b != NANITE_MAGIC[i]) {
			ERR_FAIL_V_MSG(ERR_FILE_UNRECOGNIZED, vformat("NaniteMeshResource::load: '%s' is not a .nanite file (bad magic).", p_path));
		}
	}

	// Version.
	const uint32_t version = nanite_load_u32(f.ptr());
	if (version != NANITE_FORMAT_VERSION) {
		ERR_FAIL_V_MSG(ERR_FILE_UNRECOGNIZED, vformat("NaniteMeshResource::load: unsupported .nanite version %u (expected %u).", version, NANITE_FORMAT_VERSION));
	}

	// Seven blob sections (v3 adds meshlet_vertices_data + meshlet_triangles_data — Task 1.16.4).
	Error blob_err = OK;
	blob_err = nanite_load_blob(f.ptr(), vertex_data);
	ERR_FAIL_COND_V_MSG(blob_err != OK, blob_err, "NaniteMeshResource::load: vertex_data truncated.");
	blob_err = nanite_load_blob(f.ptr(), clusters_data);
	ERR_FAIL_COND_V_MSG(blob_err != OK, blob_err, "NaniteMeshResource::load: clusters_data truncated.");
	blob_err = nanite_load_blob(f.ptr(), nodes_data);
	ERR_FAIL_COND_V_MSG(blob_err != OK, blob_err, "NaniteMeshResource::load: nodes_data truncated.");
	blob_err = nanite_load_blob(f.ptr(), page_table_data);
	ERR_FAIL_COND_V_MSG(blob_err != OK, blob_err, "NaniteMeshResource::load: page_table_data truncated.");
	blob_err = nanite_load_blob(f.ptr(), materials_data);
	ERR_FAIL_COND_V_MSG(blob_err != OK, blob_err, "NaniteMeshResource::load: materials_data truncated.");
	blob_err = nanite_load_blob(f.ptr(), meshlet_vertices_data); // Task 1.16.4
	ERR_FAIL_COND_V_MSG(blob_err != OK, blob_err, "NaniteMeshResource::load: meshlet_vertices_data truncated.");
	blob_err = nanite_load_blob(f.ptr(), meshlet_triangles_data); // Task 1.16.4
	ERR_FAIL_COND_V_MSG(blob_err != OK, blob_err, "NaniteMeshResource::load: meshlet_triangles_data truncated.");

	// Sanity trailer.
	cluster_count = static_cast<int>(nanite_load_u32(f.ptr()));
	node_count = static_cast<int>(nanite_load_u32(f.ptr()));
	page_count = static_cast<int>(nanite_load_u32(f.ptr()));

	return OK;
}

// ---------------------------------------------------------------------------
// ClassDB binding
// ---------------------------------------------------------------------------

void NaniteMeshResource::_bind_methods() {
	// Encoded data blobs.
	ClassDB::bind_method(D_METHOD("set_vertex_data", "data"), &NaniteMeshResource::set_vertex_data);
	ClassDB::bind_method(D_METHOD("get_vertex_data"), &NaniteMeshResource::get_vertex_data);
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_BYTE_ARRAY, "vertex_data"), "set_vertex_data", "get_vertex_data");

	ClassDB::bind_method(D_METHOD("set_clusters_data", "data"), &NaniteMeshResource::set_clusters_data);
	ClassDB::bind_method(D_METHOD("get_clusters_data"), &NaniteMeshResource::get_clusters_data);
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_BYTE_ARRAY, "clusters_data"), "set_clusters_data", "get_clusters_data");

	ClassDB::bind_method(D_METHOD("set_nodes_data", "data"), &NaniteMeshResource::set_nodes_data);
	ClassDB::bind_method(D_METHOD("get_nodes_data"), &NaniteMeshResource::get_nodes_data);
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_BYTE_ARRAY, "nodes_data"), "set_nodes_data", "get_nodes_data");

	ClassDB::bind_method(D_METHOD("set_page_table_data", "data"), &NaniteMeshResource::set_page_table_data);
	ClassDB::bind_method(D_METHOD("get_page_table_data"), &NaniteMeshResource::get_page_table_data);
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_BYTE_ARRAY, "page_table_data"), "set_page_table_data", "get_page_table_data");

	// Task 1.16.2 — materials_data blob.
	ClassDB::bind_method(D_METHOD("set_materials_data", "data"), &NaniteMeshResource::set_materials_data);
	ClassDB::bind_method(D_METHOD("get_materials_data"), &NaniteMeshResource::get_materials_data);
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_BYTE_ARRAY, "materials_data"), "set_materials_data", "get_materials_data");

	// Task 1.16.4 — meshlet vertex index + triangle micro-index pools.
	ClassDB::bind_method(D_METHOD("set_meshlet_vertices_data", "data"), &NaniteMeshResource::set_meshlet_vertices_data);
	ClassDB::bind_method(D_METHOD("get_meshlet_vertices_data"), &NaniteMeshResource::get_meshlet_vertices_data);
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_BYTE_ARRAY, "meshlet_vertices_data"), "set_meshlet_vertices_data", "get_meshlet_vertices_data");

	ClassDB::bind_method(D_METHOD("set_meshlet_triangles_data", "data"), &NaniteMeshResource::set_meshlet_triangles_data);
	ClassDB::bind_method(D_METHOD("get_meshlet_triangles_data"), &NaniteMeshResource::get_meshlet_triangles_data);
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_BYTE_ARRAY, "meshlet_triangles_data"), "set_meshlet_triangles_data", "get_meshlet_triangles_data");

	// Metadata resources.
	ClassDB::bind_method(D_METHOD("set_shadow_mesh", "mesh"), &NaniteMeshResource::set_shadow_mesh);
	ClassDB::bind_method(D_METHOD("get_shadow_mesh"), &NaniteMeshResource::get_shadow_mesh);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "shadow_mesh", PROPERTY_HINT_RESOURCE_TYPE, "ArrayMesh"), "set_shadow_mesh", "get_shadow_mesh");

	ClassDB::bind_method(D_METHOD("set_build_config", "config"), &NaniteMeshResource::set_build_config);
	ClassDB::bind_method(D_METHOD("get_build_config"), &NaniteMeshResource::get_build_config);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "build_config", PROPERTY_HINT_RESOURCE_TYPE, "BuilderConfig"), "set_build_config", "get_build_config");

	// Integer counts.
	ClassDB::bind_method(D_METHOD("set_cluster_count", "count"), &NaniteMeshResource::set_cluster_count);
	ClassDB::bind_method(D_METHOD("get_cluster_count"), &NaniteMeshResource::get_cluster_count);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "cluster_count"), "set_cluster_count", "get_cluster_count");

	ClassDB::bind_method(D_METHOD("set_node_count", "count"), &NaniteMeshResource::set_node_count);
	ClassDB::bind_method(D_METHOD("get_node_count"), &NaniteMeshResource::get_node_count);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "node_count"), "set_node_count", "get_node_count");

	ClassDB::bind_method(D_METHOD("set_page_count", "count"), &NaniteMeshResource::set_page_count);
	ClassDB::bind_method(D_METHOD("get_page_count"), &NaniteMeshResource::get_page_count);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "page_count"), "set_page_count", "get_page_count");

	// .nanite binary format.
	ClassDB::bind_method(D_METHOD("save", "path"), &NaniteMeshResource::save);
	ClassDB::bind_method(D_METHOD("load", "path"), &NaniteMeshResource::load);
}
