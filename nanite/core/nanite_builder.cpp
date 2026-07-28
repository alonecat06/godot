/**************************************************************************/
/*  nanite_builder.cpp                                                     */
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

#include "nanite_builder.h"

#include "core/error/error_macros.h"
#include "core/object/class_db.h"
#include "core/templates/list.h" // List<Node *> for PackedScene traversal.
#include "page_packer.h" // PagePacker::pack + PageTable for finalize_resource().
#include "scene/resources/material.h" // BaseMaterial3D for collect_materials().
#include "scene/resources/mesh.h"

#include <cstring> // memcpy

// ---------------------------------------------------------------------------
// Construction / config
// ---------------------------------------------------------------------------

NaniteBuilder::NaniteBuilder() {
	m_cfg.instantiate(); // Default-construct a BuilderConfig.
}

NaniteBuilder::NaniteBuilder(Ref<BuilderConfig> p_cfg) {
	m_cfg = p_cfg;
	if (m_cfg.is_null()) {
		// Defensive: a null config was passed — fall back to a default.
		m_cfg.instantiate();
	}
}

void NaniteBuilder::set_config(const Ref<BuilderConfig> &p_cfg) {
	m_cfg = p_cfg;
	if (m_cfg.is_null()) {
		m_cfg.instantiate();
	}
}

Ref<BuilderConfig> NaniteBuilder::get_config() const {
	return m_cfg;
}

// ---------------------------------------------------------------------------
// build() — pipeline entry point
// ---------------------------------------------------------------------------

Ref<NaniteMeshResource> NaniteBuilder::build(Ref<ArrayMesh> p_mesh) {
	// Default-construct a null Ref used as the common failure return value.
	Ref<NaniteMeshResource> null_result;

	ERR_FAIL_COND_V(p_mesh.is_null(), null_result);
	ERR_FAIL_COND_V(m_cfg.is_null(), null_result);
	ERR_FAIL_COND_V_MSG(!m_cfg->is_valid(), null_result, "NaniteBuilder: BuilderConfig is invalid");
	ERR_FAIL_COND_V_MSG(p_mesh->get_surface_count() == 0, null_result, "NaniteBuilder: Mesh has no surfaces");

	Array surface_arrays = p_mesh->surface_get_arrays(0);
	PackedVector3Array vertices = surface_arrays[Mesh::ARRAY_VERTEX];
	PackedInt32Array indices = surface_arrays[Mesh::ARRAY_INDEX];
	// Task 1.16.4 — extract normals + UVs for the raw stride-32 vertex_data
	// blob. Empty arrays are OK; preprocess_mesh fills defaults.
	PackedVector3Array normals;
	PackedVector2Array uvs;
	if (surface_arrays.size() > Mesh::ARRAY_NORMAL) {
		normals = surface_arrays[Mesh::ARRAY_NORMAL];
	}
	if (surface_arrays.size() > Mesh::ARRAY_TEX_UV) {
		uvs = surface_arrays[Mesh::ARRAY_TEX_UV];
	}

	ERR_FAIL_COND_V_MSG(vertices.size() == 0, null_result, "NaniteBuilder: Mesh surface has no vertices");
	ERR_FAIL_COND_V_MSG(indices.size() == 0, null_result, "NaniteBuilder: Mesh surface has no indices");

	if (!preprocess_mesh(vertices, indices, normals, uvs)) {
		return null_result;
	}
	if (!collect_materials(p_mesh)) {
		return null_result;
	}
	if (!build_leaf_clusters()) {
		return null_result;
	}
	if (!build_hierarchy()) {
		return null_result;
	}
	if (!build_bvh()) {
		return null_result;
	}
	if (!build_shadow_mesh()) {
		return null_result;
	}
	return finalize_resource();
}

// ---------------------------------------------------------------------------
// build_from_resource — Task 0.12.1
// Accepts ArrayMesh / PackedScene / MeshInstance3D, merges all surfaces into
// a single ArrayMesh (vertex-offset-adjusted indices), then delegates to
// build(). Exposed to GDScript as NaniteBuilder.build_from_resource(res).
// ---------------------------------------------------------------------------

Ref<NaniteMeshResource> NaniteBuilder::build_from_resource(Ref<Resource> p_resource) {
	Ref<NaniteMeshResource> null_result;

	ERR_FAIL_COND_V_MSG(p_resource.is_null(), null_result,
			"NaniteBuilder::build_from_resource: input resource is null");

	// Collect merged vertex/index/normal/uv into these buffers.
	PackedVector3Array merged_verts;
	PackedInt32Array merged_indices;
	PackedVector3Array merged_normals;
	PackedVector2Array merged_uvs;

	// --- Path A: ArrayMesh ------------------------------------------------
	// Accept both direct ArrayMesh resources and ImportMesh (subclass).
	// Note: cast_to<T>(*p_resource) would deduce O=Resource and fail
	// static_cast<T*>(Resource*) because MeshInstance3D is a Node, not a
	// Resource sibling. Explicit Object* conversion routes to the
	// single-argument cast_to overload that uses runtime derives_from.
	Object *obj_ptr = p_resource.ptr();
	Ref<ArrayMesh> arr_mesh = Object::cast_to<ArrayMesh>(obj_ptr);
	Ref<PackedScene> packed_scene = Object::cast_to<PackedScene>(obj_ptr);
	MeshInstance3D *mesh_instance = Object::cast_to<MeshInstance3D>(obj_ptr);

	if (arr_mesh.is_valid()) {
		const int surface_count = arr_mesh->get_surface_count();
		ERR_FAIL_COND_V_MSG(surface_count == 0, null_result,
				"NaniteBuilder::build_from_resource: ArrayMesh has no surfaces");
		for (int s = 0; s < surface_count; ++s) {
			Array arrays = arr_mesh->surface_get_arrays(s);
			if (arrays.size() <= Mesh::ARRAY_VERTEX) {
				continue;
			}
			PackedVector3Array s_verts = arrays[Mesh::ARRAY_VERTEX];
			PackedInt32Array s_indices = arrays[Mesh::ARRAY_INDEX];
			const int64_t base = merged_verts.size();
			merged_verts.append_array(s_verts);
			// Merge normals if present, otherwise fill with defaults.
			if (arrays.size() > Mesh::ARRAY_NORMAL) {
				PackedVector3Array s_normals = arrays[Mesh::ARRAY_NORMAL];
				if (s_normals.size() == s_verts.size()) {
					merged_normals.append_array(s_normals);
				} else {
					for (int i = 0; i < s_verts.size(); ++i) {
						merged_normals.push_back(Vector3(0, 1, 0));
					}
				}
			} else {
				for (int i = 0; i < s_verts.size(); ++i) {
					merged_normals.push_back(Vector3(0, 1, 0));
				}
			}
			// Merge UVs if present, otherwise fill with defaults.
			if (arrays.size() > Mesh::ARRAY_TEX_UV) {
				PackedVector2Array s_uvs = arrays[Mesh::ARRAY_TEX_UV];
				if (s_uvs.size() == s_verts.size()) {
					merged_uvs.append_array(s_uvs);
				} else {
					for (int i = 0; i < s_verts.size(); ++i) {
						merged_uvs.push_back(Vector2(0, 0));
					}
				}
			} else {
				for (int i = 0; i < s_verts.size(); ++i) {
					merged_uvs.push_back(Vector2(0, 0));
				}
			}
			if (s_indices.size() == 0) {
				// Non-indexed surface: synthesize 0..N-1 indices.
				for (int i = 0; i < s_verts.size(); ++i) {
					merged_indices.append(static_cast<int32_t>(base + i));
				}
			} else {
				for (int i = 0; i < s_indices.size(); ++i) {
					merged_indices.append(static_cast<int32_t>(base + s_indices[i]));
				}
			}
		}
	} else if (packed_scene.is_valid()) {
		// --- Path B: PackedScene (gltf/glb/fbx imported scene) ---------------
		Node *root = packed_scene->instantiate();
		ERR_FAIL_COND_V_MSG(root == nullptr, null_result,
				"NaniteBuilder::build_from_resource: failed to instantiate PackedScene");
		// Depth-first walk of all MeshInstance3D descendants.
		List<Node *> stack;
		stack.push_back(root);
		while (stack.size() > 0) {
			Node *node = stack.front()->get();
			stack.pop_front();
			if (MeshInstance3D *mi = Object::cast_to<MeshInstance3D>(node)) {
				Ref<ArrayMesh> mesh = mi->get_mesh();
				if (mesh.is_null()) {
					continue;
				}
				for (int s = 0; s < mesh->get_surface_count(); ++s) {
					Array arrays = mesh->surface_get_arrays(s);
					if (arrays.size() <= Mesh::ARRAY_VERTEX) {
						continue;
					}
					PackedVector3Array s_verts = arrays[Mesh::ARRAY_VERTEX];
					PackedInt32Array s_indices = arrays[Mesh::ARRAY_INDEX];
					const int64_t base = merged_verts.size();
					merged_verts.append_array(s_verts);
					// Merge normals if present, otherwise fill with defaults.
					if (arrays.size() > Mesh::ARRAY_NORMAL) {
						PackedVector3Array s_normals = arrays[Mesh::ARRAY_NORMAL];
						if (s_normals.size() == s_verts.size()) {
							merged_normals.append_array(s_normals);
						} else {
							for (int i = 0; i < s_verts.size(); ++i) {
								merged_normals.push_back(Vector3(0, 1, 0));
							}
						}
					} else {
						for (int i = 0; i < s_verts.size(); ++i) {
							merged_normals.push_back(Vector3(0, 1, 0));
						}
					}
					// Merge UVs if present, otherwise fill with defaults.
					if (arrays.size() > Mesh::ARRAY_TEX_UV) {
						PackedVector2Array s_uvs = arrays[Mesh::ARRAY_TEX_UV];
						if (s_uvs.size() == s_verts.size()) {
							merged_uvs.append_array(s_uvs);
						} else {
							for (int i = 0; i < s_verts.size(); ++i) {
								merged_uvs.push_back(Vector2(0, 0));
							}
						}
					} else {
						for (int i = 0; i < s_verts.size(); ++i) {
							merged_uvs.push_back(Vector2(0, 0));
						}
					}
					if (s_indices.size() == 0) {
						for (int i = 0; i < s_verts.size(); ++i) {
							merged_indices.append(static_cast<int32_t>(base + i));
						}
					} else {
						for (int i = 0; i < s_indices.size(); ++i) {
							merged_indices.append(static_cast<int32_t>(base + s_indices[i]));
						}
					}
				}
			}
			for (int i = 0; i < node->get_child_count(); ++i) {
				stack.push_back(node->get_child(i));
			}
		}
		memdelete(root);
	} else if (mesh_instance != nullptr) {
		// --- Path C: MeshInstance3D node (in-scene selection) -----------------
		Ref<ArrayMesh> mesh = mesh_instance->get_mesh();
		if (mesh.is_null() || mesh->get_surface_count() == 0) {
			ERR_FAIL_COND_V_MSG(true, null_result,
					"NaniteBuilder::build_from_resource: MeshInstance3D has no mesh or no surfaces");
		}
		for (int s = 0; s < mesh->get_surface_count(); ++s) {
			Array arrays = mesh->surface_get_arrays(s);
			if (arrays.size() <= Mesh::ARRAY_VERTEX) {
				continue;
			}
			PackedVector3Array s_verts = arrays[Mesh::ARRAY_VERTEX];
			PackedInt32Array s_indices = arrays[Mesh::ARRAY_INDEX];
			const int64_t base = merged_verts.size();
			merged_verts.append_array(s_verts);
			// Merge normals if present, otherwise fill with defaults.
			if (arrays.size() > Mesh::ARRAY_NORMAL) {
				PackedVector3Array s_normals = arrays[Mesh::ARRAY_NORMAL];
				if (s_normals.size() == s_verts.size()) {
					merged_normals.append_array(s_normals);
				} else {
					for (int i = 0; i < s_verts.size(); ++i) {
						merged_normals.push_back(Vector3(0, 1, 0));
					}
				}
			} else {
				for (int i = 0; i < s_verts.size(); ++i) {
					merged_normals.push_back(Vector3(0, 1, 0));
				}
			}
			// Merge UVs if present, otherwise fill with defaults.
			if (arrays.size() > Mesh::ARRAY_TEX_UV) {
				PackedVector2Array s_uvs = arrays[Mesh::ARRAY_TEX_UV];
				if (s_uvs.size() == s_verts.size()) {
					merged_uvs.append_array(s_uvs);
				} else {
					for (int i = 0; i < s_verts.size(); ++i) {
						merged_uvs.push_back(Vector2(0, 0));
					}
				}
			} else {
				for (int i = 0; i < s_verts.size(); ++i) {
					merged_uvs.push_back(Vector2(0, 0));
				}
			}
			if (s_indices.size() == 0) {
				for (int i = 0; i < s_verts.size(); ++i) {
					merged_indices.append(static_cast<int32_t>(base + i));
				}
			} else {
				for (int i = 0; i < s_indices.size(); ++i) {
					merged_indices.append(static_cast<int32_t>(base + s_indices[i]));
				}
			}
		}
	} else {
		ERR_FAIL_COND_V_MSG(true, null_result,
				"NaniteBuilder::build_from_resource: unsupported resource type (expected ArrayMesh / PackedScene / MeshInstance3D)");
	}

	ERR_FAIL_COND_V_MSG(merged_verts.size() == 0, null_result,
			"NaniteBuilder::build_from_resource: merged mesh has 0 vertices");
	ERR_FAIL_COND_V_MSG(merged_indices.size() == 0, null_result,
			"NaniteBuilder::build_from_resource: merged mesh has 0 indices");

	// Build a single-surface ArrayMesh from merged buffers and delegate to build().
	Ref<ArrayMesh> merged_mesh = memnew(ArrayMesh);
	Array merged_arrays;
	merged_arrays.resize(Mesh::ARRAY_MAX);
	merged_arrays[Mesh::ARRAY_VERTEX] = merged_verts;
	merged_arrays[Mesh::ARRAY_NORMAL] = merged_normals;
	merged_arrays[Mesh::ARRAY_TEX_UV] = merged_uvs;
	merged_arrays[Mesh::ARRAY_INDEX] = merged_indices;
	merged_mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, merged_arrays);

	// Use a default BuilderConfig when none is provided — mirrors the
	// default config used by E2E tests in test_nanite_e2e_build.gd.
	Ref<BuilderConfig> cfg;
	cfg.instantiate();
	NaniteBuilder builder(cfg);
	return builder.build(merged_mesh);
}

// ---------------------------------------------------------------------------
// preprocess_mesh — dedup + optimize (spec 0.4.2)
// ---------------------------------------------------------------------------

bool NaniteBuilder::preprocess_mesh(const PackedVector3Array &p_vertices, const PackedInt32Array &p_indices, const PackedVector3Array &p_normals, const PackedVector2Array &p_uvs) {
	m_verts_pos.clear();
	m_verts_nrm.clear();
	m_verts_uv.clear();
	m_indices.clear();

	const size_t index_count = static_cast<size_t>(p_indices.size());
	const size_t vertex_count = static_cast<size_t>(p_vertices.size());

	if (index_count == 0 || vertex_count == 0) {
		ERR_PRINT("NaniteBuilder::preprocess_mesh: empty input.");
		return false;
	}

	// Reject non-triangle-list input. The meshopt pipeline assumes every
	// index participates in a triangle.
	if (index_count % 3 != 0) {
		ERR_PRINT("NaniteBuilder::preprocess_mesh: index_count is not a multiple of 3.");
		return false;
	}

	// Meshopt's C API wants `const unsigned int *` indices. PackedInt32Array
	// is `int32_t *`, so we copy + widen + filter any -1 sentinels (which
	// shouldn't appear for PRIMITIVE_TRIANGLES surfaces, but we guard anyway).
	LocalVector<unsigned int> in_indices;
	in_indices.resize(index_count);
	for (size_t i = 0; i < index_count; ++i) {
		int32_t v = p_indices[i];
		if (v < 0) {
			ERR_PRINT("NaniteBuilder::preprocess_mesh: encountered a negative index (-1 sentinel); aborting.");
			return false;
		}
		in_indices[i] = static_cast<unsigned int>(v);
	}

	// PackedVector3Array is `Vector3`-typed but the storage is exactly 3
	// floats per element, so we can reinterpret it as `const float *` with
	// stride 12 bytes for meshopt.
	const float *verts_in = reinterpret_cast<const float *>(p_vertices.ptr());
	const size_t vertex_stride = sizeof(float) * 3;

	// Task 1.16.4 — normals + UVs. PackedVector3Array/PackedVector2Array
	// storage is tightly packed floats (3 / 2 per element). When the source
	// mesh lacks the array, defaults are filled (normal=(0,1,0), uv=(0,0))
	// so every vertex still has a full 32-byte record in vertex_data.
	const bool has_normals = (p_normals.size() == static_cast<int>(vertex_count));
	const bool has_uvs = (p_uvs.size() == static_cast<int>(vertex_count));
	const float *nrms_in = has_normals ? reinterpret_cast<const float *>(p_normals.ptr()) : nullptr;
	const float *uvs_in = has_uvs ? reinterpret_cast<const float *>(p_uvs.ptr()) : nullptr;
	const size_t nrm_stride = sizeof(float) * 3;
	const size_t uv_stride = sizeof(float) * 2;

	// 1) Generate vertex remap (dedup) based on positions only. Normals/UVs
	// follow the same remap — they're attributes of the same vertices.
	LocalVector<unsigned int> remap;
	remap.resize(vertex_count);
	size_t unique_vertex_count = meshopt_generateVertexRemap(
			remap.ptr(),
			in_indices.ptr(),
			index_count,
			verts_in,
			vertex_count,
			vertex_stride);

	// 2) Remap vertex buffers (positions + normals + UVs share the remap).
	LocalVector<float> remapped_verts;
	remapped_verts.resize(unique_vertex_count * 3);
	meshopt_remapVertexBuffer(
			remapped_verts.ptr(),
			verts_in,
			vertex_count,
			vertex_stride,
			remap.ptr());

	LocalVector<float> remapped_nrms;
	remapped_nrms.resize(unique_vertex_count * 3);
	if (has_normals) {
		meshopt_remapVertexBuffer(
				remapped_nrms.ptr(),
				nrms_in,
				vertex_count,
				nrm_stride,
				remap.ptr());
	} else {
		// Default normal = (0, 1, 0).
		for (size_t i = 0; i < unique_vertex_count; ++i) {
			remapped_nrms[i * 3 + 0] = 0.0f;
			remapped_nrms[i * 3 + 1] = 1.0f;
			remapped_nrms[i * 3 + 2] = 0.0f;
		}
	}

	LocalVector<float> remapped_uvs;
	remapped_uvs.resize(unique_vertex_count * 2);
	if (has_uvs) {
		meshopt_remapVertexBuffer(
				remapped_uvs.ptr(),
				uvs_in,
				vertex_count,
				uv_stride,
				remap.ptr());
	} else {
		// Default UV = (0, 0).
		memset(remapped_uvs.ptr(), 0, unique_vertex_count * 2 * sizeof(float));
	}

	// 3) Remap index buffer.
	LocalVector<unsigned int> remapped_indices;
	remapped_indices.resize(index_count);
	meshopt_remapIndexBuffer(
			remapped_indices.ptr(),
			in_indices.ptr(),
			index_count,
			remap.ptr());

	// 4) Optimize vertex cache (in-place: same buffer for src/dst is OK per
	// meshopt docs — output is computed before being written).
	meshopt_optimizeVertexCache(
			remapped_indices.ptr(),
			remapped_indices.ptr(),
			index_count,
			unique_vertex_count);

	// 5) Optimize vertex fetch via a remap table (Task 1.16.4). Using
	// meshopt_optimizeVertexFetchRemap instead of meshopt_optimizeVertexFetch
	// lets us apply the same reorder to all three attribute streams. The
	// function may shrink the vertex count (drops unreferenced vertices).
	LocalVector<unsigned int> fetch_remap;
	fetch_remap.resize(unique_vertex_count);
	size_t final_vertex_count = meshopt_optimizeVertexFetchRemap(
			fetch_remap.ptr(),
			remapped_indices.ptr(),
			index_count,
			unique_vertex_count);

	// Reorder indices in place using fetch_remap.
	meshopt_remapIndexBuffer(
			remapped_indices.ptr(),
			remapped_indices.ptr(),
			index_count,
			fetch_remap.ptr());

	// Reorder each attribute stream using fetch_remap.
	LocalVector<float> fetched_verts;
	fetched_verts.resize(final_vertex_count * 3);
	meshopt_remapVertexBuffer(
			fetched_verts.ptr(),
			remapped_verts.ptr(),
			unique_vertex_count,
			vertex_stride,
			fetch_remap.ptr());

	LocalVector<float> fetched_nrms;
	fetched_nrms.resize(final_vertex_count * 3);
	meshopt_remapVertexBuffer(
			fetched_nrms.ptr(),
			remapped_nrms.ptr(),
			unique_vertex_count,
			nrm_stride,
			fetch_remap.ptr());

	LocalVector<float> fetched_uvs;
	fetched_uvs.resize(final_vertex_count * 2);
	meshopt_remapVertexBuffer(
			fetched_uvs.ptr(),
			remapped_uvs.ptr(),
			unique_vertex_count,
			uv_stride,
			fetch_remap.ptr());

	// 6) Commit to member state.
	m_verts_pos.resize(final_vertex_count * 3);
	memcpy(m_verts_pos.ptr(), fetched_verts.ptr(), final_vertex_count * 3 * sizeof(float));

	m_verts_nrm.resize(final_vertex_count * 3);
	memcpy(m_verts_nrm.ptr(), fetched_nrms.ptr(), final_vertex_count * 3 * sizeof(float));

	m_verts_uv.resize(final_vertex_count * 2);
	memcpy(m_verts_uv.ptr(), fetched_uvs.ptr(), final_vertex_count * 2 * sizeof(float));

	m_indices.resize(index_count);
	memcpy(m_indices.ptr(), remapped_indices.ptr(), index_count * sizeof(unsigned int));

	return true;
}

// ---------------------------------------------------------------------------
// collect_materials — Task 1.16.3
// Stage 1: single-surface ArrayMesh only. Reads surface 0's material, casts
// to BaseMaterial3D, extracts albedo_color. Falls back to white (1,1,1,1)
// when the material is null or not a BaseMaterial3D subclass. Always
// produces exactly one entry in m_material_base_colors so the material_index
// of every leaf/parent cluster (always 0) is valid.
// ---------------------------------------------------------------------------
bool NaniteBuilder::collect_materials(const Ref<ArrayMesh> &p_mesh) {
	m_material_base_colors.clear();

	if (p_mesh.is_null() || p_mesh->get_surface_count() == 0) {
		// Defensive — should have been caught by build() earlier, but the
		// helper is also independently testable. Emit white and succeed.
		m_material_base_colors.push_back(Color(1.0f, 1.0f, 1.0f, 1.0f));
		return true;
	}

	Color base_color(1.0f, 1.0f, 1.0f, 1.0f); // default albedo fallback
	Ref<Material> mat = p_mesh->surface_get_material(0);
	if (mat.is_valid()) {
		BaseMaterial3D *bm3d = Object::cast_to<BaseMaterial3D>(mat.ptr());
		if (bm3d != nullptr) {
			base_color = bm3d->get_albedo();
		}
	}
	m_material_base_colors.push_back(base_color);
	return true;
}

// ---------------------------------------------------------------------------
// build_leaf_clusters — meshlet building (spec 0.4.3)
// ---------------------------------------------------------------------------

bool NaniteBuilder::build_leaf_clusters() {
	m_clusters.clear();
	m_meshlets.clear();
	m_meshlet_vertices.clear();
	m_meshlet_triangles.clear();

	if (m_indices.is_empty() || m_verts_pos.is_empty()) {
		ERR_PRINT("NaniteBuilder::build_leaf_clusters: no preprocessed geometry; call preprocess_mesh first.");
		return false;
	}

	ERR_FAIL_COND_V(m_cfg.is_null(), false);

	const size_t index_count = m_indices.size();
	const size_t vertex_count = m_verts_pos.size() / 3;
	const size_t vertex_stride = sizeof(float) * 3;

	const size_t max_vertices = static_cast<size_t>(m_cfg->max_vertices);
	const size_t min_triangles = static_cast<size_t>(m_cfg->min_triangles);
	const size_t max_triangles = static_cast<size_t>(m_cfg->max_triangles);
	const float cone_weight = static_cast<float>(m_cfg->cone_weight);
	const float split_factor = static_cast<float>(m_cfg->split_factor);
	const int optimize_level = m_cfg->meshlet_optimize_level;

	// 1) Bound the meshlet output buffer (use min_triangles per meshopt docs).
	const size_t max_meshlets = meshopt_buildMeshletsBound(index_count, max_vertices, min_triangles);

	// 2) Allocate scratch buffers. Per meshopt docs, the worst case for both
	// meshlet_vertices and meshlet_triangles is `index_count` elements.
	LocalVector<struct meshopt_Meshlet> meshlets;
	meshlets.resize(max_meshlets);

	LocalVector<unsigned int> meshlet_vertices;
	meshlet_vertices.resize(index_count);

	LocalVector<unsigned char> meshlet_triangles;
	meshlet_triangles.resize(index_count);

	// 3) Build meshlets.
	size_t meshlet_count = meshopt_buildMeshletsFlex(
			meshlets.ptr(),
			meshlet_vertices.ptr(),
			meshlet_triangles.ptr(),
			m_indices.ptr(),
			index_count,
			m_verts_pos.ptr(),
			vertex_count,
			vertex_stride,
			max_vertices,
			min_triangles,
			max_triangles,
			cone_weight,
			split_factor);

	if (meshlet_count == 0) {
		ERR_PRINT("NaniteBuilder::build_leaf_clusters: meshopt_buildMeshletsFlex produced no meshlets.");
		return false;
	}

	// 4) Per-meshlet: optimize for compression, compute bounds, fill NaniteCluster.
	m_clusters.resize(meshlet_count);

	for (size_t i = 0; i < meshlet_count; ++i) {
		const struct meshopt_Meshlet &m = meshlets[i];

		unsigned int *ml_verts = meshlet_vertices.ptr() + m.vertex_offset;
		unsigned char *ml_tris = meshlet_triangles.ptr() + m.triangle_offset;

		meshopt_optimizeMeshletLevel(
				ml_verts,
				m.vertex_count,
				ml_tris,
				m.triangle_count,
				optimize_level);

		meshopt_Bounds b = meshopt_computeMeshletBounds(
				ml_verts,
				ml_tris,
				m.triangle_count,
				m_verts_pos.ptr(),
				vertex_count,
				vertex_stride);

		NaniteCluster &c = m_clusters[i];
		c.vertex_offset = m.vertex_offset;
		c.vertex_count = m.vertex_count;
		c.triangle_offset = m.triangle_offset;
		c.triangle_count = m.triangle_count;
		c.group_id = 0; // L0 leaves.
		c.material_index = 0; // Task 1.16.3 — Stage 1 single-material.
		c.error = 0.0f; // Leaves have zero simplification error.
		c.bounds = AABB(
				Vector3(b.center[0] - b.radius, b.center[1] - b.radius, b.center[2] - b.radius),
				Vector3(b.radius * 2.0f, b.radius * 2.0f, b.radius * 2.0f));
		Vector3 axis(b.cone_axis[0], b.cone_axis[1], b.cone_axis[2]);
		// Docs don't guarantee unit length; normalize defensively.
		if (axis.length_squared() > 0.0f) {
			axis.normalize();
			c.cone_cutoff = b.cone_cutoff;
		} else {
			// meshopt returned degenerate cone_axis (e.g., for meshlets with
			// cancelling normals after simplification). Use a unit fallback axis
			// and signal full-sphere cone so runtime backface culling is skipped
			// (safe conservative behavior).
			axis = Vector3(0, 1, 0);
			c.cone_cutoff = -1.0f;
		}
		c.cone_axis = axis;
		c.page_id = 0; // Assigned later by PagePacker.
	}

	// 5) Commit scratch buffers to member state (move semantics OK).
	m_meshlets = meshlets;
	m_meshlet_vertices = meshlet_vertices;
	m_meshlet_triangles = meshlet_triangles;

	return true;
}

// ---------------------------------------------------------------------------
// build_hierarchy — bottom-up hierarchical simplification (spec 0.5.1)
// ---------------------------------------------------------------------------
//
// Approach: per-level partition merge using meshopt_partitionClusters.
// At each level:
//   1. Decode every current-level cluster's meshlet micro-index into flat
//      vertex-index arrays (one per cluster) and feed them to
//      meshopt_partitionClusters(target=cfg.partition_size).
//   2. For each partition with >1 cluster:
//        - Build a partition-local vertex buffer + triangle index buffer
//          (remap global vertex indices to partition-local 0..N-1).
//        - Simplify to half the triangle count via meshopt_simplifyWithAttributes
//          (options = LockBorder | Regularize, vertex_lock = NULL).
//        - Re-cluster the simplified geometry with meshopt_buildMeshletsFlex.
//        - Append parent meshlets + clusters to the global pools.
//        - Create LEAF HierarchyNodes for each parent cluster, then a MERGE
//          HierarchyNode linking source nodes ↔ parent leaves.
//   3. For partitions with exactly 1 cluster: promote the source node
//      directly to the next level (no simplification).
//   4. Replace current level with the new MERGE HierarchyNodes and loop.
// Stop when current level has <=1 node OR current_lod >= max_lod_levels.
//
// HierarchyNode tree shape:
//   - LEAF nodes represent a single cluster (leaf or parent).
//   - MERGE nodes represent a partition merge event; they carry both the
//     source HierarchyNodes (input) and the parent-leaf HierarchyNodes
//     (output). build_bvh walks this tree to produce the flat BVH.

bool NaniteBuilder::build_hierarchy() {
	m_hierarchy_tree.clear();
	m_root_node_idx = UINT32_MAX;

	if (m_clusters.is_empty()) {
		ERR_PRINT("NaniteBuilder::build_hierarchy: no leaf clusters; call build_leaf_clusters first.");
		return false;
	}

	ERR_FAIL_COND_V(m_cfg.is_null(), false);

	const size_t vertex_count = m_verts_pos.size() / 3;
	const size_t vertex_stride = sizeof(float) * 3;
	const size_t max_vertices = static_cast<size_t>(m_cfg->max_vertices);
	const size_t min_triangles = static_cast<size_t>(m_cfg->min_triangles);
	const size_t max_triangles = static_cast<size_t>(m_cfg->max_triangles);
	const float cone_weight = static_cast<float>(m_cfg->cone_weight);
	const float split_factor = static_cast<float>(m_cfg->split_factor);
	const float target_error = static_cast<float>(m_cfg->target_error);
	const size_t partition_size = static_cast<size_t>(m_cfg->partition_size);
	const uint32_t max_lod = static_cast<uint32_t>(m_cfg->max_lod_levels);
	const int optimize_level = m_cfg->meshlet_optimize_level;

	// 1) Seed the tree with LEAF HierarchyNodes for each leaf cluster.
	LocalVector<uint32_t> current_level_nodes;
	for (uint32_t i = 0; i < m_clusters.size(); ++i) {
		HierarchyNode n;
		n.is_leaf = true;
		n.cluster_idx = i;
		m_hierarchy_tree.push_back(n);
		current_level_nodes.push_back(static_cast<uint32_t>(m_hierarchy_tree.size() - 1));
	}

	// Trivial single-cluster case: the leaf is the root.
	if (current_level_nodes.size() == 1) {
		m_root_node_idx = current_level_nodes[0];
		return true;
	}

	uint32_t current_lod = 0;
	// Note: meshopt_SimplifyLockBorder is intentionally NOT used. When a
	// partition is extracted from the full mesh, every vertex on the
	// partition's boundary becomes a topological "border" vertex (its link
	// is incomplete because neighbouring triangles were excluded). Locking
	// all those border vertices prevents meshopt from collapsing ANY edge,
	// so simplification returns the input unchanged (result_error == 0,
	// simplified_index_count == partition_index_count). That in turn makes
	// build_hierarchy loop until max_lod_levels, each iteration duplicating
	// the same triangles into m_clusters — causing parent_triangles to
	// explode to ~10x the leaf count. Dropping the flag lets meshopt
	// collapse border vertices (which is fine for Nanite: each LOD level
	// is an independent representation, and runtime picks one LOD per
	// region, so inter-partition seams within a single LOD are not a
	// concern). meshopt_SimplifyRegularize is kept to produce cleaner
	// topology.
	const unsigned int simplify_options = meshopt_SimplifyRegularize;

	// Track ALL MERGE HierarchyNodes produced across every level. The root
	// of the BVH must group over every MERGE (not just the last level's)
	// so that linearize_bvh_recursive walks the full hierarchy: a MERGE
	// node at LOD N carries the LOD-N source leaves in its left subtree,
	// and those source leaves are NOT re-exported to the next level (only
	// the simplified parent leaves are). If the root only grouped over the
	// last level's MERGEs, every earlier level's source leaves (e.g. the
	// L0 leaves from successful LOD-0 partitions) would be orphaned and
	// their bounds lost from the root AABB.
	LocalVector<uint32_t> all_merges;

	while (current_level_nodes.size() > 1 && current_lod < max_lod) {
		const uint32_t cluster_count = static_cast<uint32_t>(current_level_nodes.size());

		// MERGE HierarchyNodes created at THIS iteration. Appended to
		// all_merges so the root can reference every MERGE across all
		// levels (see the comment on all_merges for why).
		LocalVector<uint32_t> current_level_merges;

		// 2) Build flat_cluster_indices (vertex indices per cluster, concatenated)
		//    and cluster_index_counts (triangle_count*3 per cluster).
		LocalVector<unsigned int> flat_cluster_indices;
		LocalVector<unsigned int> cluster_index_counts;
		cluster_index_counts.resize(cluster_count);
		size_t total_index_count = 0;

		for (uint32_t c = 0; c < cluster_count; ++c) {
			const HierarchyNode &hn = m_hierarchy_tree[current_level_nodes[c]];
			const NaniteCluster &cluster = m_clusters[hn.cluster_idx];

			for (uint32_t t = 0; t < cluster.triangle_count; ++t) {
				uint32_t tri_offset = cluster.triangle_offset + t * 3;
				for (int k = 0; k < 3; ++k) {
					unsigned char local_idx = m_meshlet_triangles[tri_offset + k];
					unsigned int global_v = m_meshlet_vertices[cluster.vertex_offset + local_idx];
					flat_cluster_indices.push_back(global_v);
				}
			}
			cluster_index_counts[c] = cluster.triangle_count * 3;
			total_index_count += cluster.triangle_count * 3;
		}

		// 3) Partition.
		LocalVector<unsigned int> partition_ids;
		partition_ids.resize(cluster_count);
		size_t partition_count = meshopt_partitionClusters(
				partition_ids.ptr(),
				flat_cluster_indices.ptr(),
				total_index_count,
				cluster_index_counts.ptr(),
				cluster_count,
				m_verts_pos.ptr(),
				vertex_count,
				vertex_stride,
				partition_size);

		if (partition_count == 0) {
			break; // Defensive — should not happen.
		}

		// 4) Group clusters by partition_id (some IDs may be unused).
		uint32_t max_pid = 0;
		for (uint32_t c = 0; c < cluster_count; ++c) {
			if (partition_ids[c] > max_pid) {
				max_pid = partition_ids[c];
			}
		}
		LocalVector<LocalVector<uint32_t>> partition_to_local; // partition -> local indices into current_level_nodes
		partition_to_local.resize(max_pid + 1);
		for (uint32_t c = 0; c < cluster_count; ++c) {
			partition_to_local[partition_ids[c]].push_back(c);
		}

		// 5) For each non-empty partition, simplify + recluster.
		LocalVector<uint32_t> next_level_nodes;
		const uint32_t parent_lod = current_lod + 1;
		bool any_merge_happened = false;

		for (uint32_t p = 0; p < partition_to_local.size(); ++p) {
			const LocalVector<uint32_t> &partition = partition_to_local[p];
			if (partition.is_empty()) {
				continue;
			}

			if (partition.size() == 1) {
				// Single-cluster partition: promote directly, no simplification.
				next_level_nodes.push_back(current_level_nodes[partition[0]]);
				continue;
			}

			// 5a) Collect unique vertices referenced by this partition's clusters.
			// Build global -> partition-local remap.
			LocalVector<unsigned int> global_to_local;
			global_to_local.resize(vertex_count);
			for (size_t i = 0; i < vertex_count; ++i) {
				global_to_local[i] = UINT32_MAX;
			}
			LocalVector<unsigned int> local_to_global;

			for (uint32_t ci = 0; ci < partition.size(); ++ci) {
				uint32_t c = partition[ci];
				const HierarchyNode &hn = m_hierarchy_tree[current_level_nodes[c]];
				const NaniteCluster &cluster = m_clusters[hn.cluster_idx];
				for (uint32_t v = 0; v < cluster.vertex_count; ++v) {
					unsigned int global_v = m_meshlet_vertices[cluster.vertex_offset + v];
					if (global_v < vertex_count && global_to_local[global_v] == UINT32_MAX) {
						global_to_local[global_v] = static_cast<unsigned int>(local_to_global.size());
						local_to_global.push_back(global_v);
					}
				}
			}

			const size_t partition_vertex_count = local_to_global.size();
			if (partition_vertex_count == 0) {
				// Degenerate partition — promote originals.
				for (uint32_t ci = 0; ci < partition.size(); ++ci) {
					next_level_nodes.push_back(current_level_nodes[partition[ci]]);
				}
				continue;
			}

			// 5b) Build partition-local vertex buffer (3 floats per vertex).
			LocalVector<float> partition_verts;
			partition_verts.resize(partition_vertex_count * 3);
			for (size_t v = 0; v < partition_vertex_count; ++v) {
				unsigned int global_v = local_to_global[v];
				memcpy(partition_verts.ptr() + v * 3,
						m_verts_pos.ptr() + global_v * 3,
						sizeof(float) * 3);
			}

			// 5c) Build partition-local triangle index buffer by decoding each
			//     cluster's meshlet micro-index through the global->local remap.
			LocalVector<unsigned int> partition_indices;
			for (uint32_t ci = 0; ci < partition.size(); ++ci) {
				uint32_t c = partition[ci];
				const HierarchyNode &hn = m_hierarchy_tree[current_level_nodes[c]];
				const NaniteCluster &cluster = m_clusters[hn.cluster_idx];
				for (uint32_t t = 0; t < cluster.triangle_count; ++t) {
					uint32_t tri_offset = cluster.triangle_offset + t * 3;
					for (int k = 0; k < 3; ++k) {
						unsigned char local_idx = m_meshlet_triangles[tri_offset + k];
						unsigned int global_v = m_meshlet_vertices[cluster.vertex_offset + local_idx];
						partition_indices.push_back(global_to_local[global_v]);
					}
				}
			}
			const size_t partition_index_count = partition_indices.size();

			// 5d) Track max child error for monotonicity enforcement.
			float max_child_error = 0.0f;
			for (uint32_t ci = 0; ci < partition.size(); ++ci) {
				uint32_t c = partition[ci];
				const HierarchyNode &hn = m_hierarchy_tree[current_level_nodes[c]];
				const NaniteCluster &cluster = m_clusters[hn.cluster_idx];
				if (cluster.error > max_child_error) {
					max_child_error = cluster.error;
				}
			}

			// 5e) Simplify to half the triangle count.
			const size_t target_index_count = partition_index_count / 2;
			LocalVector<unsigned int> simplified_indices;
			simplified_indices.resize(partition_index_count); // upper bound for output.

			float result_error = 0.0f;
			size_t simplified_index_count = meshopt_simplifyWithAttributes(
					simplified_indices.ptr(),
					partition_indices.ptr(),
					partition_index_count,
					partition_verts.ptr(),
					partition_vertex_count,
					vertex_stride,
					nullptr, // vertex_attributes
					0, // vertex_attributes_stride
					nullptr, // attribute_weights
					0, // attribute_count
					nullptr, // vertex_lock — borders are not locked (see simplify_options note).
					target_index_count,
					target_error,
					simplify_options,
					&result_error);

			// Bail out if simplification collapsed the geometry below the
			// minimum cluster floor; promote the originals instead.
			if (simplified_index_count < min_triangles * 3 || simplified_index_count == 0) {
				for (uint32_t ci = 0; ci < partition.size(); ++ci) {
					next_level_nodes.push_back(current_level_nodes[partition[ci]]);
				}
				continue;
			}

			// Bail out if simplification barely reduced the geometry (less
			// than 10% reduction). Creating a parent level with essentially
			// the same triangles would duplicate geometry and — across many
			// levels — cause parent_triangles to explode. Promote the
			// originals instead so the next level can try again or the loop
			// terminates via the no-merge stop condition.
			if (simplified_index_count >= partition_index_count * 9 / 10) {
				for (uint32_t ci = 0; ci < partition.size(); ++ci) {
					next_level_nodes.push_back(current_level_nodes[partition[ci]]);
				}
				continue;
			}

			// 5f) Re-cluster the simplified geometry.
			const size_t max_meshlets = meshopt_buildMeshletsBound(simplified_index_count, max_vertices, min_triangles);
			LocalVector<struct meshopt_Meshlet> parent_meshlets;
			parent_meshlets.resize(max_meshlets);
			LocalVector<unsigned int> parent_meshlet_vertices;
			parent_meshlet_vertices.resize(simplified_index_count);
			LocalVector<unsigned char> parent_meshlet_triangles;
			parent_meshlet_triangles.resize(simplified_index_count);

			size_t parent_meshlet_count = meshopt_buildMeshletsFlex(
					parent_meshlets.ptr(),
					parent_meshlet_vertices.ptr(),
					parent_meshlet_triangles.ptr(),
					simplified_indices.ptr(),
					simplified_index_count,
					partition_verts.ptr(),
					partition_vertex_count,
					vertex_stride,
					max_vertices,
					min_triangles,
					max_triangles,
					cone_weight,
					split_factor);

			if (parent_meshlet_count == 0) {
				for (uint32_t ci = 0; ci < partition.size(); ++ci) {
					next_level_nodes.push_back(current_level_nodes[partition[ci]]);
				}
				continue;
			}

			// 5g) Append parent meshlet data to the global pools. We compute
			//     fresh vertex/triangle offsets rather than relying on tight
			//     packing inside parent_meshlet_vertices/_triangles.
			const uint32_t meshlet_vertices_offset = static_cast<uint32_t>(m_meshlet_vertices.size());
			const uint32_t meshlet_triangles_offset = static_cast<uint32_t>(m_meshlet_triangles.size());
			const uint32_t meshlet_offset = static_cast<uint32_t>(m_meshlets.size());

			LocalVector<uint32_t> new_vertex_offsets;
			LocalVector<uint32_t> new_triangle_offsets;
			new_vertex_offsets.resize(parent_meshlet_count);
			new_triangle_offsets.resize(parent_meshlet_count);
			{
				uint32_t cur_v = meshlet_vertices_offset;
				uint32_t cur_t = meshlet_triangles_offset;
				for (size_t i = 0; i < parent_meshlet_count; ++i) {
					const struct meshopt_Meshlet &src = parent_meshlets[i];
					new_vertex_offsets[i] = cur_v;
					new_triangle_offsets[i] = cur_t;
					cur_v += src.vertex_count;
					cur_t += src.triangle_count * 3;
				}
			}

			// Append vertices (remap partition-local → global).
			for (size_t i = 0; i < parent_meshlet_count; ++i) {
				const struct meshopt_Meshlet &src = parent_meshlets[i];
				for (uint32_t v = 0; v < src.vertex_count; ++v) {
					unsigned int local_v = parent_meshlet_vertices[src.vertex_offset + v];
					unsigned int global_v = local_to_global[local_v];
					m_meshlet_vertices.push_back(global_v);
				}
			}
			// Append triangle bytes (micro-indices are meshlet-local, copy as-is).
			for (size_t i = 0; i < parent_meshlet_count; ++i) {
				const struct meshopt_Meshlet &src = parent_meshlets[i];
				for (uint32_t t = 0; t < src.triangle_count * 3; ++t) {
					m_meshlet_triangles.push_back(parent_meshlet_triangles[src.triangle_offset + t]);
				}
			}
			// Append meshlet descriptors with rebased offsets.
			for (size_t i = 0; i < parent_meshlet_count; ++i) {
				const struct meshopt_Meshlet &src = parent_meshlets[i];
				struct meshopt_Meshlet dst;
				dst.vertex_offset = new_vertex_offsets[i];
				dst.triangle_offset = new_triangle_offsets[i];
				dst.vertex_count = src.vertex_count;
				dst.triangle_count = src.triangle_count;
				m_meshlets.push_back(dst);
			}

			// 5h) Per-parent-meshlet: optimize + compute bounds + fill NaniteCluster.
			// Enforce error monotonicity: parent error >= max child error.
			const float parent_error = MAX(result_error, max_child_error);

			LocalVector<uint32_t> parent_leaf_node_indices;
			for (size_t i = 0; i < parent_meshlet_count; ++i) {
				const struct meshopt_Meshlet &m = m_meshlets[meshlet_offset + i];
				unsigned int *ml_verts = m_meshlet_vertices.ptr() + m.vertex_offset;
				unsigned char *ml_tris = m_meshlet_triangles.ptr() + m.triangle_offset;

				meshopt_optimizeMeshletLevel(
						ml_verts,
						m.vertex_count,
						ml_tris,
						m.triangle_count,
						optimize_level);

				meshopt_Bounds b = meshopt_computeMeshletBounds(
						ml_verts,
						ml_tris,
						m.triangle_count,
						m_verts_pos.ptr(),
						vertex_count,
						vertex_stride);

				NaniteCluster c;
			c.vertex_offset = m.vertex_offset;
			c.vertex_count = m.vertex_count;
			c.triangle_offset = m.triangle_offset;
			c.triangle_count = m.triangle_count;
			c.group_id = parent_lod;
			c.material_index = 0; // Task 1.16.3 — Stage 1 single-material.
			c.error = parent_error;
				c.bounds = AABB(
						Vector3(b.center[0] - b.radius, b.center[1] - b.radius, b.center[2] - b.radius),
						Vector3(b.radius * 2.0f, b.radius * 2.0f, b.radius * 2.0f));
				Vector3 axis(b.cone_axis[0], b.cone_axis[1], b.cone_axis[2]);
				if (axis.length_squared() > 0.0f) {
					axis.normalize();
					c.cone_cutoff = b.cone_cutoff;
				} else {
					// meshopt returned degenerate cone_axis (e.g., for meshlets with
					// cancelling normals after simplification). Use a unit fallback axis
					// and signal full-sphere cone so runtime backface culling is skipped
					// (safe conservative behavior).
					axis = Vector3(0, 1, 0);
					c.cone_cutoff = -1.0f;
				}
				c.cone_axis = axis;
				c.page_id = 0;
				m_clusters.push_back(c);

				// LEAF HierarchyNode for this parent cluster.
				HierarchyNode pn;
				pn.is_leaf = true;
				pn.cluster_idx = static_cast<uint32_t>(m_clusters.size() - 1);
				m_hierarchy_tree.push_back(pn);
				parent_leaf_node_indices.push_back(static_cast<uint32_t>(m_hierarchy_tree.size() - 1));
			}

			// 5i) MERGE HierarchyNode linking sources ↔ parent leaves.
			// The MERGE node is recorded in m_hierarchy_tree for the BVH
			// linearizer, but it is NOT pushed into next_level_nodes:
			// next_level_nodes must contain only LEAF HierarchyNodes so the
			// next iteration can safely read hn.cluster_idx. The parent
			// clusters (parent_leaf_node_indices) are what the next level
			// actually iterates over.
			HierarchyNode merge_node;
			merge_node.is_leaf = false;
			merge_node.cluster_idx = UINT32_MAX;
			for (uint32_t ci = 0; ci < partition.size(); ++ci) {
				merge_node.source_node_indices.push_back(current_level_nodes[partition[ci]]);
			}
			merge_node.parent_leaf_node_indices = parent_leaf_node_indices;
			m_hierarchy_tree.push_back(merge_node);
			current_level_merges.push_back(static_cast<uint32_t>(m_hierarchy_tree.size() - 1));

			// The next level iterates over the parent LEAF HierarchyNodes
			// (each has a valid cluster_idx pointing into m_clusters).
			for (uint32_t i = 0; i < parent_leaf_node_indices.size(); ++i) {
				next_level_nodes.push_back(parent_leaf_node_indices[i]);
			}

			any_merge_happened = true;
		}

		if (!any_merge_happened) {
			// No merges this level — partitioning couldn't reduce further. Stop.
			// all_merges retains every MERGE from earlier levels so the root
			// can still reference them.
			break;
		}

		// Append this level's MERGEs to the global list so the root groups
		// over every level's MERGEs, not just the last.
		for (uint32_t i = 0; i < current_level_merges.size(); ++i) {
			all_merges.push_back(current_level_merges[i]);
		}
		current_level_nodes = next_level_nodes;
		current_lod = parent_lod;
	}

	// 6) Set the root. The root must group over EVERY MERGE HierarchyNode
	//    from every level (so the BVH walks each MERGE's source leaves AND
	//    parent leaves) PLUS any remaining `current_level_nodes` that were
	//    promoted all the way through without being merged (those leaves are
	//    not sources of any MERGE, so they'd otherwise be orphaned). A MERGE
	//    node's parent leaves may also appear in current_level_nodes as
	//    sources of a higher-level MERGE — that duplication is harmless for
	//    bounds (union is idempotent) and for error (max is idempotent).
	LocalVector<uint32_t> root_candidates;
	root_candidates = all_merges;
	for (uint32_t i = 0; i < current_level_nodes.size(); ++i) {
		root_candidates.push_back(current_level_nodes[i]);
	}

	if (root_candidates.size() == 1) {
		m_root_node_idx = root_candidates[0];
	} else if (root_candidates.size() > 1) {
		HierarchyNode root_merge;
		root_merge.is_leaf = false;
		root_merge.cluster_idx = UINT32_MAX;
		root_merge.source_node_indices = root_candidates;
		// parent_leaf_node_indices left empty — grouping-only.
		m_hierarchy_tree.push_back(root_merge);
		m_root_node_idx = static_cast<uint32_t>(m_hierarchy_tree.size() - 1);
	} else {
		return false;
	}

	return m_root_node_idx != UINT32_MAX;
}

// ---------------------------------------------------------------------------
// build_bvh — linearize hierarchy into a flat NaniteClusterNode array
//             (spec 0.5.2)
// ---------------------------------------------------------------------------
//
// Walks m_hierarchy_tree starting from m_root_node_idx, producing a binary
// NaniteClusterNode tree in m_nodes (root at index 0). Each LEAF HierarchyNode
// becomes a leaf NaniteClusterNode (first_cluster/cluster_count = 1); each
// MERGE HierarchyNode becomes an internal NaniteClusterNode whose left subtree
// is a balanced binary chain over its source children and whose right subtree
// is a balanced binary chain over its parent-leaf children. For grouping-only
// MERGEs (no parent leaves), only the left subtree is produced.
//
// Bounds and error on internal nodes are computed bottom-up:
//   bounds = left.bounds.merge(right.bounds)
//   error  = max(left.error, right.error)
// This guarantees the spec's bounds-containment and error-monotonicity
// invariants regardless of how the hierarchy was built.

bool NaniteBuilder::build_bvh() {
	m_nodes.clear();

	if (m_root_node_idx == UINT32_MAX || m_root_node_idx >= m_hierarchy_tree.size()) {
		ERR_PRINT("NaniteBuilder::build_bvh: no hierarchy; call build_hierarchy first.");
		return false;
	}

	const uint32_t root_idx = linearize_bvh_recursive(m_root_node_idx, 0);
	// Root must be the first node we emit (index 0).
	if (root_idx != 0 || m_nodes.is_empty()) {
		ERR_PRINT("NaniteBuilder::build_bvh: failed to linearize hierarchy.");
		return false;
	}

	return true;
}

// Recursively linearize a HierarchyNode into m_nodes at the given depth.
// Emits a placeholder NaniteClusterNode FIRST (reserving the current index),
// then recursively builds children, then fills in the placeholder. This
// pre-order emission guarantees the root (called first from build_bvh) ends
// up at index 0. Returns the index of the emitted NaniteClusterNode.
uint32_t NaniteBuilder::linearize_bvh_recursive(uint32_t p_hierarchy_node_idx, uint32_t p_depth) {
	ERR_FAIL_COND_V(p_hierarchy_node_idx >= m_hierarchy_tree.size(), UINT32_MAX);
	const HierarchyNode &hn = m_hierarchy_tree[p_hierarchy_node_idx];

	const uint32_t my_index = static_cast<uint32_t>(m_nodes.size());
	NaniteClusterNode node; // placeholder; left/right default to UINT32_MAX.
	node.depth = p_depth;
	m_nodes.push_back(node);

	if (hn.is_leaf) {
		ERR_FAIL_COND_V(hn.cluster_idx >= m_clusters.size(), UINT32_MAX);
		const NaniteCluster &cluster = m_clusters[hn.cluster_idx];
		m_nodes[my_index].first_cluster = hn.cluster_idx;
		m_nodes[my_index].cluster_count = 1;
		m_nodes[my_index].bounds = cluster.bounds;
		m_nodes[my_index].error = cluster.error;
		// left_child / right_child remain UINT32_MAX (leaf sentinel).
		return my_index;
	}

	// MERGE: left = binary chain over source nodes, right = binary chain over
	// parent leaves (omitted for grouping-only merges).
	const uint32_t left_idx = build_binary_chain(hn.source_node_indices, p_depth + 1);
	uint32_t right_idx = UINT32_MAX;
	if (!hn.parent_leaf_node_indices.is_empty()) {
		right_idx = build_binary_chain(hn.parent_leaf_node_indices, p_depth + 1);
	}

	m_nodes[my_index].left_child = left_idx;
	m_nodes[my_index].right_child = right_idx;
	if (left_idx != UINT32_MAX && right_idx != UINT32_MAX) {
		m_nodes[my_index].bounds = m_nodes[left_idx].bounds.merge(m_nodes[right_idx].bounds);
		m_nodes[my_index].error = MAX(m_nodes[left_idx].error, m_nodes[right_idx].error);
	} else if (left_idx != UINT32_MAX) {
		m_nodes[my_index].bounds = m_nodes[left_idx].bounds;
		m_nodes[my_index].error = m_nodes[left_idx].error;
	} else if (right_idx != UINT32_MAX) {
		m_nodes[my_index].bounds = m_nodes[right_idx].bounds;
		m_nodes[my_index].error = m_nodes[right_idx].error;
	}
	return my_index;
}

// Build a balanced binary tree of NaniteClusterNodes from a list of
// HierarchyNode indices. Splits the list in half, recursively builds each
// half, then emits an internal NaniteClusterNode joining the two halves.
// Pre-order emission: the placeholder for the chain root is reserved before
// recursing into children. Returns the index of the chain root (or
// UINT32_MAX for empty input).
uint32_t NaniteBuilder::build_binary_chain(const LocalVector<uint32_t> &p_hierarchy_indices, uint32_t p_depth) {
	if (p_hierarchy_indices.is_empty()) {
		return UINT32_MAX;
	}
	if (p_hierarchy_indices.size() == 1) {
		return linearize_bvh_recursive(p_hierarchy_indices[0], p_depth);
	}

	// Reserve the chain-root slot first so its index is lower than its children.
	const uint32_t my_index = static_cast<uint32_t>(m_nodes.size());
	NaniteClusterNode node;
	node.depth = p_depth;
	m_nodes.push_back(node);

	const size_t mid = p_hierarchy_indices.size() / 2;
	LocalVector<uint32_t> left_indices;
	LocalVector<uint32_t> right_indices;
	left_indices.resize(mid);
	for (size_t i = 0; i < mid; ++i) {
		left_indices[i] = p_hierarchy_indices[i];
	}
	right_indices.resize(p_hierarchy_indices.size() - mid);
	for (size_t i = mid; i < p_hierarchy_indices.size(); ++i) {
		right_indices[i - mid] = p_hierarchy_indices[i];
	}

	const uint32_t left_idx = build_binary_chain(left_indices, p_depth + 1);
	const uint32_t right_idx = build_binary_chain(right_indices, p_depth + 1);

	m_nodes[my_index].left_child = left_idx;
	m_nodes[my_index].right_child = right_idx;
	if (left_idx != UINT32_MAX && right_idx != UINT32_MAX) {
		m_nodes[my_index].bounds = m_nodes[left_idx].bounds.merge(m_nodes[right_idx].bounds);
		m_nodes[my_index].error = MAX(m_nodes[left_idx].error, m_nodes[right_idx].error);
	} else if (left_idx != UINT32_MAX) {
		m_nodes[my_index].bounds = m_nodes[left_idx].bounds;
		m_nodes[my_index].error = m_nodes[left_idx].error;
	} else if (right_idx != UINT32_MAX) {
		m_nodes[my_index].bounds = m_nodes[right_idx].bounds;
		m_nodes[my_index].error = m_nodes[right_idx].error;
	}
	return my_index;
}

// ---------------------------------------------------------------------------
// Stub pipeline stages — implemented in later tasks.
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// build_shadow_mesh — coarse LOD shadow mesh extraction (spec 0.7)
// ---------------------------------------------------------------------------
//
// Walks m_nodes and collects clusters attached to BVH leaf nodes whose depth
// <= cfg.shadow_lod_depth. By selecting only shallow-depth clusters we
// naturally skip the deeper (more detailed) ones, yielding a coarsened
// representation suitable for `mesh_set_shadow_mesh` fallback rendering.
//
// For each selected cluster, the meshlet micro-index (3 bytes per triangle
// into the cluster's local vertex range 0..vertex_count-1) is decoded into a
// flat triangle list, and the cluster's local vertices are remapped through
// m_meshlet_vertices to global indices into m_verts_pos. Each cluster is
// emitted as a contiguous block of vertices + offset triangles so the
// resulting ArrayMesh has a single PRIMITIVE_TRIANGLES surface.
bool NaniteBuilder::build_shadow_mesh() {
	m_shadow_mesh = Ref<ArrayMesh>(); // Clear any prior result.

	if (m_cfg.is_null()) {
		ERR_PRINT("NaniteBuilder::build_shadow_mesh: null config.");
		return false;
	}
	if (m_nodes.is_empty() || m_clusters.is_empty()) {
		ERR_PRINT("NaniteBuilder::build_shadow_mesh: BVH/clusters missing; call build_bvh first.");
		return false;
	}

	const int shadow_depth = m_cfg->shadow_lod_depth;
	const size_t vertex_count = m_verts_pos.size() / 3;

	// Select clusters for the shadow mesh by LOD level (cluster.group_id).
	// The BVH binary-chain linearization produces a tree whose leaf depth
	// scales with log2(siblings_per_level), so node.depth is NOT a reliable
	// proxy for LOD level. Instead we read each cluster's group_id (which
	// build_hierarchy sets to the LOD level: L0=0, L1=1, ...) and select the
	// deepest available LOD level that is <= shadow_lod_depth. Picking a
	// single LOD level avoids emitting duplicate geometry from multiple
	// LODs of the same region (which would inflate shadow_tri_count past
	// the original mesh's triangle count and fail the spec's
	// "shadow_tri_count < original_tri_count" acceptance check).
	uint32_t target_lod = 0;
	for (uint32_t i = 0; i < m_clusters.size(); ++i) {
		const uint32_t gid = m_clusters[i].group_id;
		if (static_cast<int>(gid) <= shadow_depth && gid > target_lod) {
			target_lod = gid;
		}
	}

	LocalVector<uint32_t> selected_clusters;
	for (uint32_t i = 0; i < m_clusters.size(); ++i) {
		if (m_clusters[i].group_id == target_lod) {
			selected_clusters.push_back(i);
		}
	}

	if (selected_clusters.is_empty()) {
		ERR_PRINT("NaniteBuilder::build_shadow_mesh: no clusters within shadow_lod_depth.");
		return false;
	}

	// Decode each cluster's meshlet micro-index into a flat triangle list
	// appended to the shadow surface arrays. Vertices are deduplicated
	// per-cluster (each cluster's vertices form a contiguous local block;
	// triangle indices within the cluster reference these local indices).
	PackedVector3Array shadow_vertices;
	PackedInt32Array shadow_indices;
	uint32_t vertex_base = 0;

	for (size_t i = 0; i < selected_clusters.size(); ++i) {
		const uint32_t cluster_idx = selected_clusters[i];
		if (cluster_idx >= m_clusters.size()) {
			continue;
		}
		const NaniteCluster &cluster = m_clusters[cluster_idx];
		if (cluster.triangle_count == 0 || cluster.vertex_count == 0) {
			continue;
		}

		// Append the cluster's vertices (decoded through m_meshlet_vertices)
		// to the shadow vertex buffer. Track the actual pushed count so that
		// triangle indices stay aligned even if a defensive skip fires.
		const uint32_t cluster_vertex_base = vertex_base;
		uint32_t pushed_this_cluster = 0;
		for (uint32_t v = 0; v < cluster.vertex_count; ++v) {
			const uint32_t pool_off = cluster.vertex_offset + v;
			if (pool_off >= m_meshlet_vertices.size()) {
				break;
			}
			const uint32_t global_v = m_meshlet_vertices[pool_off];
			if (global_v >= vertex_count) {
				break;
			}
			const float *vp = m_verts_pos.ptr() + global_v * 3;
			shadow_vertices.push_back(Vector3(vp[0], vp[1], vp[2]));
			++pushed_this_cluster;
		}

		// Append triangles (3 micro-indices each → local indices offset by
		// cluster_vertex_base). meshopt packs micro-indices as raw bytes
		// (0..255) into the cluster's local vertex range.
		for (uint32_t t = 0; t < cluster.triangle_count; ++t) {
			const uint32_t tri_off = cluster.triangle_offset + t * 3;
			if (tri_off + 2 >= m_meshlet_triangles.size()) {
				break;
			}
			const unsigned char l0 = m_meshlet_triangles[tri_off + 0];
			const unsigned char l1 = m_meshlet_triangles[tri_off + 1];
			const unsigned char l2 = m_meshlet_triangles[tri_off + 2];
			if (l0 >= pushed_this_cluster ||
					l1 >= pushed_this_cluster ||
					l2 >= pushed_this_cluster) {
				continue;
			}
			shadow_indices.push_back(static_cast<int32_t>(cluster_vertex_base + l0));
			shadow_indices.push_back(static_cast<int32_t>(cluster_vertex_base + l1));
			shadow_indices.push_back(static_cast<int32_t>(cluster_vertex_base + l2));
		}

		vertex_base += pushed_this_cluster;
	}

	if (shadow_indices.size() == 0 || shadow_vertices.size() == 0) {
		ERR_PRINT("NaniteBuilder::build_shadow_mesh: produced no geometry.");
		return false;
	}

	// Append the original mesh AABB's 8 corner points as "safety" vertices.
	// The shadow mesh only selects clusters from a single LOD level, which can
	// under-cover the original mesh's extent on some axes (e.g. the -X side).
	// Adding the source AABB corners to the vertex buffer WITHOUT adding them
	// to the index buffer guarantees the shadow mesh's AABB fully encloses the
	// original mesh's AABB, while keeping the triangle count unchanged (no
	// degenerate triangles are emitted). m_source_mesh is not retained, so the
	// AABB is recomputed from m_verts_pos by scanning min/max per axis.
	if (vertex_count > 0) {
		const float *vp = m_verts_pos.ptr();
		Vector3 v_min(vp[0], vp[1], vp[2]);
		Vector3 v_max = v_min;
		for (size_t i = 1; i < vertex_count; ++i) {
			const float *p = vp + i * 3;
			if (p[0] < v_min.x) v_min.x = p[0];
			if (p[1] < v_min.y) v_min.y = p[1];
			if (p[2] < v_min.z) v_min.z = p[2];
			if (p[0] > v_max.x) v_max.x = p[0];
			if (p[1] > v_max.y) v_max.y = p[1];
			if (p[2] > v_max.z) v_max.z = p[2];
		}
		const AABB src_aabb(v_min, v_max - v_min);
		const Vector3 corners[8] = {
			src_aabb.position,
			src_aabb.position + Vector3(src_aabb.size.x, 0, 0),
			src_aabb.position + Vector3(0, src_aabb.size.y, 0),
			src_aabb.position + Vector3(0, 0, src_aabb.size.z),
			src_aabb.position + Vector3(src_aabb.size.x, src_aabb.size.y, 0),
			src_aabb.position + Vector3(src_aabb.size.x, 0, src_aabb.size.z),
			src_aabb.position + Vector3(0, src_aabb.size.y, src_aabb.size.z),
			src_aabb.position + src_aabb.size,
		};
		for (int i = 0; i < 8; ++i) {
			shadow_vertices.push_back(corners[i]);
		}
	}

	Array arrays;
	arrays.resize(Mesh::ARRAY_MAX);
	arrays[Mesh::ARRAY_VERTEX] = shadow_vertices;
	arrays[Mesh::ARRAY_INDEX] = shadow_indices;

	m_shadow_mesh.instantiate();
	ERR_FAIL_COND_V(m_shadow_mesh.is_null(), false);
	m_shadow_mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);

	return true;
}

Ref<NaniteMeshResource> NaniteBuilder::finalize_resource() {
	ERR_FAIL_COND_V(m_cfg.is_null(), Ref<NaniteMeshResource>());

	Ref<NaniteMeshResource> res;
	res.instantiate();
	ERR_FAIL_COND_V(res.is_null(), Ref<NaniteMeshResource>());

	// ---- 1) Vertex pool: raw stride-32 (Task 1.16.4) -------------------
	// vertex_data is RAW (not meshopt-compressed) so the rasterize / material
	// shaders can index it with a fixed stride. Layout per vertex (32 bytes):
	//   float[3] position  (12 B)
	//   float[3] normal    (12 B)
	//   float[2] uv        (8 B)
	// m_verts_pos / m_verts_nrm / m_verts_uv are parallel arrays that share
	// the same dedup + fetch-optimize remap, so vertex i's attributes live at
	// i*{3,3,2} in the respective arrays.
	const size_t vertex_count = m_verts_pos.size() / 3;
	const size_t vertex_stride_32 = sizeof(float) * 8; // 3 + 3 + 2 = 32 bytes
	PackedByteArray vertex_data;
	vertex_data.resize(static_cast<int>(vertex_count * vertex_stride_32));
	if (vertex_count > 0) {
		float *w = reinterpret_cast<float *>(vertex_data.ptrw());
		for (size_t i = 0; i < vertex_count; ++i) {
			float *rec = w + i * 8;
			rec[0] = m_verts_pos[i * 3 + 0];
			rec[1] = m_verts_pos[i * 3 + 1];
			rec[2] = m_verts_pos[i * 3 + 2];
			rec[3] = m_verts_nrm[i * 3 + 0];
			rec[4] = m_verts_nrm[i * 3 + 1];
			rec[5] = m_verts_nrm[i * 3 + 2];
			rec[6] = m_verts_uv[i * 2 + 0];
			rec[7] = m_verts_uv[i * 2 + 1];
		}
	}
	res->set_vertex_data(vertex_data);

	// ---- 2) Clusters: metadata only (Task 1.16.4) ----------------------
	// clusters_data is the concatenation of NaniteCluster::serialize()
	// output (68 bytes each — fixed stride). The meshopt-encoded meshlet
	// geometry that used to be interleaved here was moved into
	// meshlet_vertices_data + meshlet_triangles_data (Section 7) so the cull
	// shader can index clusters as a fixed-stride array.
	LocalVector<uint8_t> clusters_buf;
	for (uint32_t ci = 0; ci < m_clusters.size(); ++ci) {
		const PackedByteArray meta = m_clusters[ci].serialize();
		for (int i = 0; i < meta.size(); ++i) {
			clusters_buf.push_back(meta.ptr()[i]);
		}
	}
	PackedByteArray clusters_data;
	clusters_data.resize(static_cast<int>(clusters_buf.size()));
	if (clusters_buf.size() > 0) {
		memcpy(clusters_data.ptrw(), clusters_buf.ptr(), clusters_buf.size());
	}
	res->set_clusters_data(clusters_data);

	// ---- 3) Nodes: per-node serialize() concatenation --------------------
	// Each NaniteClusterNode::serialize() returns a fixed 48-byte
	// little-endian blob; we concatenate them in m_nodes order.
	LocalVector<uint8_t> nodes_buf;
	for (uint32_t ni = 0; ni < m_nodes.size(); ++ni) {
		const PackedByteArray node_bytes = m_nodes[ni].serialize();
		for (int i = 0; i < node_bytes.size(); ++i) {
			nodes_buf.push_back(node_bytes.ptr()[i]);
		}
	}
	PackedByteArray nodes_data;
	nodes_data.resize(static_cast<int>(nodes_buf.size()));
	if (nodes_buf.size() > 0) {
		memcpy(nodes_data.ptrw(), nodes_buf.ptr(), nodes_buf.size());
	}
	res->set_nodes_data(nodes_data);

	// ---- 4) Page table: PagePacker::pack + PageTable::serialize ---------
	// PagePacker::pack modifies m_clusters in place (writes page_id back to
	// each cluster). We pass the live m_clusters so the page_id assignment
	// is visible to any subsequent debug inspection.
	PageTable page_table = PagePacker::pack(m_clusters, *m_cfg.ptr());
	res->set_page_table_data(page_table.serialize());

	// ---- 5) Metadata + counts -------------------------------------------
	res->set_shadow_mesh(m_shadow_mesh);
	res->set_build_config(m_cfg);
	res->set_cluster_count(static_cast<int>(m_clusters.size()));
	res->set_node_count(static_cast<int>(m_nodes.size()));
	res->set_page_count(static_cast<int>(page_table.pages.size()));

	// ---- 6) Materials: Task 1.16.3 — encode materials_data blob ---------
	// Layout (per material, 32 bytes = 2 × vec4, std430-friendly):
	//   vec4 base_color (r, g, b, a)
	//   vec4 metallic_roughness_pad (metallic, roughness, 0, 0)
	// Stage 1 simplified: only base_color is consumed by the Lambert shader;
	// metallic/roughness default to (0.0, 1.0) for a pure diffuse look. The
	// pad bytes are zero. If collect_materials() produced no entries
	// (defensive — should not happen), emit a single white material so
	// shader sampling at material_index=0 never reads out of bounds.
	if (m_material_base_colors.is_empty()) {
		PackedByteArray fallback;
		fallback.resize(32);
		memset(fallback.ptrw(), 0, 32);
		// base_color = (1,1,1,1)
		float white[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
		memcpy(fallback.ptrw(), white, 16);
		// metallic=0, roughness=1
		float mr[2] = { 0.0f, 1.0f };
		memcpy(fallback.ptrw() + 16, mr, 8);
		res->set_materials_data(fallback);
	} else {
		// Each material = 32 bytes; total = count * 32.
		const size_t mat_count = m_material_base_colors.size();
		PackedByteArray materials_data;
		materials_data.resize(static_cast<int>(mat_count * 32));
		uint8_t *wptr = materials_data.ptrw();
		memset(wptr, 0, mat_count * 32);
		for (size_t i = 0; i < mat_count; ++i) {
			const Color &c = m_material_base_colors[i];
			float *slot = reinterpret_cast<float *>(wptr + i * 32);
			slot[0] = c.r;
			slot[1] = c.g;
			slot[2] = c.b;
			slot[3] = c.a;
			// metallic=0, roughness=1, pad=0, pad=0
			slot[4] = 0.0f;
			slot[5] = 1.0f;
			// slot[6], slot[7] left as zero from memset.
		}
		res->set_materials_data(materials_data);
	}

	// ---- 7) Meshlet vertex indices + triangle micro-indices (Task 1.16.4)
	// meshlet_vertices_data: raw uint32[] — each entry is a global index into
	//   vertex_data (0..vertex_count-1). A cluster's vertex_offset/vertex_count
	//   indexes into this array directly.
	// meshlet_triangles_data: raw uint8[] — each triangle consumes 3 bytes
	//   (local vertex indices 0..255 into the cluster's own vertex range). A
	//   cluster's triangle_offset is a BYTE offset into this array.
	// Both pools are 4-byte aligned at the blob level so they can be uploaded
	// as SSBOs without driver alignment warnings.
	{
		PackedByteArray mv_data;
		const size_t mv_count = m_meshlet_vertices.size();
		const size_t mv_bytes = mv_count * sizeof(unsigned int);
		// Pad to 4-byte alignment.
		const size_t mv_padded = (mv_bytes + 3u) & ~size_t(3);
		mv_data.resize(static_cast<int>(mv_padded));
		memset(mv_data.ptrw(), 0, mv_padded);
		if (mv_count > 0) {
			memcpy(mv_data.ptrw(), m_meshlet_vertices.ptr(), mv_bytes);
		}
		res->set_meshlet_vertices_data(mv_data);
	}
	{
		PackedByteArray mt_data;
		const size_t mt_count = m_meshlet_triangles.size();
		const size_t mt_bytes = mt_count * sizeof(unsigned char);
		// Pad to 4-byte alignment.
		const size_t mt_padded = (mt_bytes + 3u) & ~size_t(3);
		mt_data.resize(static_cast<int>(mt_padded));
		memset(mt_data.ptrw(), 0, mt_padded);
		if (mt_count > 0) {
			memcpy(mt_data.ptrw(), m_meshlet_triangles.ptr(), mt_bytes);
		}
		res->set_meshlet_triangles_data(mt_data);
	}

	return res;
}

// ---------------------------------------------------------------------------
// ClassDB binding
// ---------------------------------------------------------------------------

void NaniteBuilder::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_config", "config"), &NaniteBuilder::set_config);
	ClassDB::bind_method(D_METHOD("get_config"), &NaniteBuilder::get_config);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "config", PROPERTY_HINT_RESOURCE_TYPE, "BuilderConfig"), "set_config", "get_config");

	// `build()` drives the full Stage-0 pipeline and returns the assembled
	// NaniteMeshResource. Bound now that NaniteMeshResource is a complete
	// GDCLASS type (GetTypeInfo<R> for Ref<NaniteMeshResource> resolves).
	ClassDB::bind_method(D_METHOD("build", "mesh"), &NaniteBuilder::build);

	// Task 0.12.1 — Resource conversion entry point. Static method so
	// GDScript can call NaniteBuilder.build_from_resource(res) without
	// instantiating a builder.
	ClassDB::bind_static_method("NaniteBuilder", D_METHOD("build_from_resource", "resource"),
			&NaniteBuilder::build_from_resource);
}
