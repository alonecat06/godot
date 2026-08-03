/**************************************************************************/
/*  nanite_mesh_editor.cpp                                                */
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

#ifdef TOOLS_ENABLED

#include "nanite_mesh_editor.h"

#include "nanite/core/nanite_cluster.h"
#include "nanite/core/nanite_debug.h"
#include "nanite/core/nanite_resource.h"

#include "core/math/math_funcs.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/templates/hash_map.h"
#include "core/templates/hash_set.h"
#include "core/variant/variant.h"
#include "editor/editor_node.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/box_container.h"
#include "scene/main/viewport.h"
#include "scene/resources/3d/world_3d.h"
#include "scene/resources/material.h"
#include "scene/resources/shader.h"

#include <meshoptimizer.h>

// ---------------------------------------------------------------------------
// CPU-side cluster decoder
// ---------------------------------------------------------------------------
//
// Stage 0 preview rendering does NOT depend on the Nanite GPU pipeline
// (CompositorEffect / nanite_cull.glsl / nanite_rasterize.glsl /
// nanite_material_resolve.glsl). Instead it reads the encoded blobs from
// NaniteMeshResource on the CPU and builds a standard ArrayMesh that Godot's
// MeshInstance3D renders via the regular forward pipeline.
//
// Vertex layout (Task 1.16.4 — raw stride 32 B):
//   position.xyz (3f) + normal.xyz (3f) + uv.xy (2f) = 8 floats
//
// Index layout (Task 1.16.4):
//   meshlet_vertices_data: uint32[] of global vertex indices, one per
//     cluster-local vertex slot. cluster.vertex_offset indexes into this
//     array (count = cluster.vertex_count).
//   meshlet_triangles_data: uint8[] of micro-indices, 3 bytes per triangle,
//     each byte is a local vertex index (0..cluster.vertex_count-1) into
//     the cluster's own vertex range. cluster.triangle_offset is a BYTE
//     offset into this array (byte count = cluster.triangle_count * 3).
//
// For a "Force LOD Level" preview, clusters whose group_id != force_lod_level
// are skipped, and the surviving clusters' triangles are concatenated into a
// single ArrayMesh surface (optionally with per-cluster vertex colors).

namespace {

constexpr size_t kVertexStride = 32; // 8 floats: pos(3) + normal(3) + uv(2)

// Decode clusters from p_resource whose group_id == p_force_lod_level.
//
// Two output modes:
//   - p_emit_lines == false: produces a triangle mesh (3 verts + 3 indices per
//     triangle). If p_per_cluster_colors is true, also emits a per-cluster
//     HSV color per vertex (written to p_colors).
//   - p_emit_lines == true: produces a line mesh ready for PRIMITIVE_LINES
//     (6 verts per triangle = 2 per edge × 3 edges). p_indices / p_colors
//     are not touched in this mode; p_verts alone receives the line vertices.
//
// All output arrays are appended to (not cleared). Returns the number of
// vertices appended to p_verts.
int decode_clusters_for_lod(const NaniteMeshResource &p_resource,
		int p_force_lod_level,
		bool p_per_cluster_colors,
		bool p_emit_lines,
		PackedVector3Array &p_verts,
		PackedInt32Array &p_indices,
		PackedColorArray &p_colors,
		PackedVector3Array &p_normals,
		PackedVector2Array &p_uvs) {
	const PackedByteArray &clusters_data = p_resource.get_clusters_data();
	const PackedByteArray &vertex_data = p_resource.get_vertex_data();
	const PackedByteArray &meshlet_vertices_data = p_resource.get_meshlet_vertices_data();
	const PackedByteArray &meshlet_triangles_data = p_resource.get_meshlet_triangles_data();

	const int cluster_count = p_resource.get_cluster_count();
	const size_t cluster_stride = NaniteCluster::get_serialized_size(); // 68

	print_line(vformat("[nanite-decode] force_lod=%d emit_lines=%d per_cluster_colors=%d",
			p_force_lod_level, (int)p_emit_lines, (int)p_per_cluster_colors));
	print_line(vformat("[nanite-decode] cluster_count=%d clusters_data=%d vertex_data=%d mv_data=%d mt_data=%d",
			cluster_count, clusters_data.size(), vertex_data.size(),
			meshlet_vertices_data.size(), meshlet_triangles_data.size()));

	if (cluster_count <= 0 || clusters_data.size() < (int)(cluster_count * cluster_stride)) {
		print_line("[nanite-decode] FAIL: cluster_count or clusters_data size mismatch");
		return 0;
	}
	if (vertex_data.size() < (int)kVertexStride || meshlet_vertices_data.size() < 4 ||
			meshlet_triangles_data.size() < 3) {
		print_line(vformat("[nanite-decode] FAIL: meshlet data too small (vd=%d mv=%d mt=%d)",
				vertex_data.size(), meshlet_vertices_data.size(), meshlet_triangles_data.size()));
		return 0;
	}

	const int total_vertex_count = vertex_data.size() / kVertexStride;
	const uint32_t *mv = reinterpret_cast<const uint32_t *>(meshlet_vertices_data.ptr());
	const int mv_count = meshlet_vertices_data.size() / 4;
	const uint8_t *tri_base = meshlet_triangles_data.ptr();
	const int tri_byte_count = meshlet_triangles_data.size();
	const float *vp_base = reinterpret_cast<const float *>(vertex_data.ptr());

	int lod_matched = 0, lod_skipped = 0, lod_no_tris = 0, lod_bounds_fail = 0;
	for (int ci = 0; ci < cluster_count; ++ci) {
		NaniteCluster c = NaniteCluster::deserialize(clusters_data,
				(uint32_t)(ci * cluster_stride));
		if ((int)c.group_id != p_force_lod_level) {
			lod_skipped++;
			continue;
		}
		lod_matched++;
		if (c.vertex_count == 0 || c.triangle_count == 0) {
			lod_no_tris++;
			continue;
		}
		if ((int)c.vertex_offset < 0 || (int)(c.vertex_offset + c.vertex_count) > mv_count) {
			lod_bounds_fail++;
			continue;
		}
		const uint32_t tri_bytes_needed = c.triangle_count * 3;
		if ((uint64_t)c.triangle_offset + tri_bytes_needed > (uint64_t)tri_byte_count) {
			lod_bounds_fail++;
			continue;
		}

		Color cluster_color;
		if (p_per_cluster_colors) {
			// Knuth-style hash on ci to spread hues evenly.
			uint32_t h = (uint32_t)ci * 2654435761u;
			float hue = (float)(h % 360u) / 360.0f;
			cluster_color = Color::from_hsv(hue, 0.65f, 0.95f);
		} else {
			cluster_color = Color(1, 1, 1);
		}

		for (uint32_t ti = 0; ti < c.triangle_count; ++ti) {
			const uint8_t *tri_ptr = tri_base + c.triangle_offset + ti * 3;
			uint8_t i0 = tri_ptr[0];
			uint8_t i1 = tri_ptr[1];
			uint8_t i2 = tri_ptr[2];
			if (i0 >= c.vertex_count || i1 >= c.vertex_count || i2 >= c.vertex_count) {
				continue;
			}
			uint32_t g0 = mv[c.vertex_offset + i0];
			uint32_t g1 = mv[c.vertex_offset + i1];
			uint32_t g2 = mv[c.vertex_offset + i2];
			if (g0 >= (uint32_t)total_vertex_count ||
					g1 >= (uint32_t)total_vertex_count ||
					g2 >= (uint32_t)total_vertex_count) {
				continue;
			}

			const float *vp0 = vp_base + g0 * (kVertexStride / sizeof(float));
			const float *vp1 = vp_base + g1 * (kVertexStride / sizeof(float));
			const float *vp2 = vp_base + g2 * (kVertexStride / sizeof(float));
			const Vector3 v0(vp0[0], vp0[1], vp0[2]);
			const Vector3 v1(vp1[0], vp1[1], vp1[2]);
			const Vector3 v2(vp2[0], vp2[1], vp2[2]);
			const Vector3 n0(vp0[3], vp0[4], vp0[5]);
			const Vector3 n1(vp1[3], vp1[4], vp1[5]);
			const Vector3 n2(vp2[3], vp2[4], vp2[5]);
			const Vector2 uv0(vp0[6], vp0[7]);
			const Vector2 uv1(vp1[6], vp1[7]);
			const Vector2 uv2(vp2[6], vp2[7]);

			if (p_emit_lines) {
				// 3 edges per triangle: (v0,v1), (v1,v2), (v2,v0)
				p_verts.push_back(v0);
				p_verts.push_back(v1);
				p_verts.push_back(v1);
				p_verts.push_back(v2);
				p_verts.push_back(v2);
				p_verts.push_back(v0);
			} else {
				int base = p_verts.size();
				p_verts.push_back(v0);
				p_verts.push_back(v1);
				p_verts.push_back(v2);
				p_normals.push_back(n0);
				p_normals.push_back(n1);
				p_normals.push_back(n2);
				p_uvs.push_back(uv0);
				p_uvs.push_back(uv1);
				p_uvs.push_back(uv2);
				p_indices.push_back(base);
				p_indices.push_back(base + 1);
				p_indices.push_back(base + 2);
				if (p_per_cluster_colors) {
					p_colors.push_back(cluster_color);
					p_colors.push_back(cluster_color);
					p_colors.push_back(cluster_color);
				}
			}
		}
	}
	print_line(vformat("[nanite-decode] result: lod_matched=%d no_tris=%d bounds_fail=%d skipped_other_lod=%d verts=%d indices=%d",
			lod_matched, lod_no_tris, lod_bounds_fail, lod_skipped, p_verts.size(), p_indices.size()));
	return p_verts.size();
}

// Build a triangle ArrayMesh from the decoded clusters. Empty on failure.
Ref<ArrayMesh> build_cluster_mesh(const NaniteMeshResource &p_resource,
		int p_force_lod_level,
		bool p_per_cluster_colors) {
	PackedVector3Array verts;
	PackedInt32Array indices;
	PackedColorArray colors;
	PackedVector3Array normals;
	PackedVector2Array uvs;
	decode_clusters_for_lod(p_resource, p_force_lod_level, p_per_cluster_colors,
			/*p_emit_lines=*/false, verts, indices, colors, normals, uvs);
	if (verts.is_empty() || indices.is_empty()) {
		return Ref<ArrayMesh>();
	}
	Array arrays;
	arrays.resize(Mesh::ARRAY_MAX);
	arrays[Mesh::ARRAY_VERTEX] = verts;
	arrays[Mesh::ARRAY_NORMAL] = normals;
	arrays[Mesh::ARRAY_TEX_UV] = uvs;
	arrays[Mesh::ARRAY_INDEX] = indices;
	if (p_per_cluster_colors && !colors.is_empty()) {
		arrays[Mesh::ARRAY_COLOR] = colors;
	}
	Ref<ArrayMesh> mesh;
	mesh.instantiate();
	mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
	return mesh;
}

// Build a wireframe (PRIMITIVE_LINES) ArrayMesh from the decoded clusters.
// Empty on failure.
Ref<ArrayMesh> build_cluster_wire_mesh(const NaniteMeshResource &p_resource,
		int p_force_lod_level) {
	PackedVector3Array verts;
	PackedInt32Array indices;
	PackedColorArray colors;
	PackedVector3Array normals;
	PackedVector2Array uvs;
	decode_clusters_for_lod(p_resource, p_force_lod_level, /*p_per_cluster_colors=*/false,
			/*p_emit_lines=*/true, verts, indices, colors, normals, uvs);
	if (verts.is_empty()) {
		return Ref<ArrayMesh>();
	}
	Array arrays;
	arrays.resize(Mesh::ARRAY_MAX);
	arrays[Mesh::ARRAY_VERTEX] = verts;
	Ref<ArrayMesh> mesh;
	mesh.instantiate();
	mesh->add_surface_from_arrays(Mesh::PRIMITIVE_LINES, arrays);
	return mesh;
}

// Build a wireframe (PRIMITIVE_LINES) ArrayMesh from any ArrayMesh whose
// surfaces are PRIMITIVE_TRIANGLES. Used to overlay wireframe on the
// shadow_mesh in Normal + Wireframe mode.
Ref<ArrayMesh> build_wire_from_array_mesh(const Ref<ArrayMesh> &p_src) {
	if (p_src.is_null() || p_src->get_surface_count() == 0) {
		return Ref<ArrayMesh>();
	}
	Ref<ArrayMesh> out;
	out.instantiate();
	bool any_added = false;
	for (int s = 0; s < p_src->get_surface_count(); ++s) {
		if (p_src->surface_get_primitive_type(s) != Mesh::PRIMITIVE_TRIANGLES) {
			continue;
		}
		Array arrays = p_src->surface_get_arrays(s);
		if (arrays.size() <= Mesh::ARRAY_VERTEX) {
			continue;
		}
		PackedVector3Array verts = arrays[Mesh::ARRAY_VERTEX];
		if (verts.is_empty()) {
			continue;
		}
		PackedInt32Array indices = arrays[Mesh::ARRAY_INDEX];
		PackedVector3Array line_verts;
		if (indices.is_empty()) {
			// Non-indexed surface: every 3 verts = 1 triangle.
			for (int j = 0; j + 2 < verts.size(); j += 3) {
				line_verts.push_back(verts[j]);
				line_verts.push_back(verts[j + 1]);
				line_verts.push_back(verts[j + 1]);
				line_verts.push_back(verts[j + 2]);
				line_verts.push_back(verts[j + 2]);
				line_verts.push_back(verts[j]);
			}
		} else {
			for (int j = 0; j + 2 < indices.size(); j += 3) {
				Vector3 v0 = verts[indices[j]];
				Vector3 v1 = verts[indices[j + 1]];
				Vector3 v2 = verts[indices[j + 2]];
				line_verts.push_back(v0);
				line_verts.push_back(v1);
				line_verts.push_back(v1);
				line_verts.push_back(v2);
				line_verts.push_back(v2);
				line_verts.push_back(v0);
			}
		}
		if (line_verts.is_empty()) {
			continue;
		}
		Array line_arrays;
		line_arrays.resize(Mesh::ARRAY_MAX);
		line_arrays[Mesh::ARRAY_VERTEX] = line_verts;
		out->add_surface_from_arrays(Mesh::PRIMITIVE_LINES, line_arrays);
		any_added = true;
	}
	if (!any_added) {
		return Ref<ArrayMesh>();
	}
	return out;
}

// Build partition border wireframes for the CURRENT LOD level.
// Clusters at p_current_lod_level are grouped into partitions of ~4 using
// topology-based adjacency (vertex_positions=nullptr to avoid the
// mergeSpatial pass that can merge topologically disconnected clusters).
// For each partition, the outer boundary edges of the merged cluster set
// are extracted (edges appearing in exactly 1 triangle).
//
// If p_filter_cluster_idx >= 0, only the partition containing that cluster
// is rendered; all other partitions are skipped.
//
// Returns a PRIMITIVE_LINES ArrayMesh with the border edges, or empty Ref
// on failure. The same mesh is used for both the solid (surface) and dashed
// (occluded) overlay instances.
Ref<ArrayMesh> build_partition_border_wire(const NaniteMeshResource &p_resource,
		int p_current_lod_level, int p_filter_cluster_idx = -1) {
	const int max_lod = p_resource.get_max_lod_level();
	if (p_current_lod_level > max_lod) {
		return Ref<ArrayMesh>();
	}

	const PackedByteArray &clusters_data = p_resource.get_clusters_data();
	const PackedByteArray &vertex_data = p_resource.get_vertex_data();
	const PackedByteArray &meshlet_vertices_data = p_resource.get_meshlet_vertices_data();
	const PackedByteArray &meshlet_triangles_data = p_resource.get_meshlet_triangles_data();

	const int cluster_count = p_resource.get_cluster_count();
	const size_t cluster_stride = NaniteCluster::get_serialized_size(); // 68

	if (cluster_count <= 0 || clusters_data.size() < (int)(cluster_count * cluster_stride)) {
		return Ref<ArrayMesh>();
	}
	if (vertex_data.size() < (int)kVertexStride || meshlet_vertices_data.size() < 4 ||
			meshlet_triangles_data.size() < 3) {
		return Ref<ArrayMesh>();
	}

	const int total_vertex_count = vertex_data.size() / kVertexStride;
	const uint32_t *mv = reinterpret_cast<const uint32_t *>(meshlet_vertices_data.ptr());
	const int mv_count = meshlet_vertices_data.size() / 4;
	const uint8_t *tri_base = meshlet_triangles_data.ptr();
	const int tri_byte_count = meshlet_triangles_data.size();
	const float *vp_base = reinterpret_cast<const float *>(vertex_data.ptr());

	// 1) Collect clusters at the CURRENT LOD level.
	struct ClusterInfo {
		uint32_t cluster_idx;
		LocalVector<unsigned int> tri_indices;
	};
	LocalVector<ClusterInfo> cur_lod_clusters;

	for (int ci = 0; ci < cluster_count; ++ci) {
		NaniteCluster c = NaniteCluster::deserialize(clusters_data, (uint32_t)(ci * cluster_stride));
		if ((int)c.group_id != p_current_lod_level) {
			continue;
		}
		if (c.vertex_count == 0 || c.triangle_count == 0) {
			continue;
		}
		if ((int)c.vertex_offset < 0 || (int)(c.vertex_offset + c.vertex_count) > mv_count) {
			continue;
		}
		const uint32_t tri_bytes_needed = c.triangle_count * 3;
		if ((uint64_t)c.triangle_offset + tri_bytes_needed > (uint64_t)tri_byte_count) {
			continue;
		}

		ClusterInfo info;
		info.cluster_idx = (uint32_t)ci;
		for (uint32_t t = 0; t < c.triangle_count; ++t) {
			const uint8_t *tri_ptr = tri_base + c.triangle_offset + t * 3;
			for (int k = 0; k < 3; ++k) {
				uint8_t local_idx = tri_ptr[k];
				if (local_idx >= c.vertex_count) {
					continue;
				}
				unsigned int global_v = mv[c.vertex_offset + local_idx];
				if (global_v >= (uint32_t)total_vertex_count) {
					continue;
				}
				info.tri_indices.push_back(global_v);
			}
		}
		if (info.tri_indices.size() > 0) {
			cur_lod_clusters.push_back(info);
		}
	}

	const size_t cl_cluster_count = cur_lod_clusters.size();
	if (cl_cluster_count <= 1) {
		return Ref<ArrayMesh>();
	}

	// 2) Partition current LOD clusters. Prefer stored partition_ids from
	// the resource (accurate); fall back to meshopt recompute if unavailable.
	LocalVector<unsigned int> partition_ids;
	partition_ids.resize(cl_cluster_count);
	size_t partition_count = 0;

	PackedByteArray pid_blob = p_resource.get_partition_ids_data();
	if (pid_blob.size() >= (int)(cluster_count * sizeof(uint32_t))) {
		// Read stored partition_ids — these are the exact IDs from build time.
		const uint32_t *pid_base = reinterpret_cast<const uint32_t *>(pid_blob.ptr());
		for (size_t i = 0; i < cl_cluster_count; ++i) {
			uint32_t cidx = cur_lod_clusters[i].cluster_idx;
			uint32_t pid = pid_base[cidx];
			partition_ids[i] = pid;
			if (pid != UINT32_MAX && (size_t)(pid + 1) > partition_count) {
				partition_count = pid + 1;
			}
		}
	} else {
		// Fall back: recompute partitions with meshopt (less accurate).
		LocalVector<unsigned int> cluster_indices;
		LocalVector<unsigned int> cluster_index_counts;
		cluster_index_counts.resize(cl_cluster_count);
		for (size_t i = 0; i < cl_cluster_count; ++i) {
			const ClusterInfo &info = cur_lod_clusters[i];
			cluster_index_counts[i] = (unsigned int)info.tri_indices.size();
			for (unsigned int idx : info.tri_indices) {
				cluster_indices.push_back(idx);
			}
		}
		size_t safe_vertex_count = (size_t)total_vertex_count;
		for (size_t i = 0; i < cluster_indices.size(); ++i) {
			unsigned int v = cluster_indices[i];
			if (v >= safe_vertex_count) {
				safe_vertex_count = (size_t)v + 1;
			}
		}
		partition_count = meshopt_partitionClusters(
				partition_ids.ptr(),
				cluster_indices.ptr(),
				cluster_indices.size(),
				cluster_index_counts.ptr(),
				cl_cluster_count,
				nullptr,
				safe_vertex_count,
				0,
				3);
	}

	if (partition_count == 0 || partition_count == cl_cluster_count) {
		return Ref<ArrayMesh>();
	}

	// 4) Group clusters by partition_id.
	LocalVector<LocalVector<size_t>> partitions;
	partitions.resize(partition_count);
	for (size_t i = 0; i < cl_cluster_count; ++i) {
		uint32_t pid = partition_ids[i];
		if (pid < partition_count) {
			partitions[pid].push_back(i);
		}
	}

	// 4b) If filtering by a specific cluster, find which partition it
	// belongs to and restrict rendering to that partition only.
	int filter_partition_id = -1; // -1 = render all partitions
	if (p_filter_cluster_idx >= 0) {
		for (size_t i = 0; i < cl_cluster_count; ++i) {
			if (cur_lod_clusters[i].cluster_idx == (uint32_t)p_filter_cluster_idx) {
				filter_partition_id = (int)partition_ids[i];
				break;
			}
		}
		if (filter_partition_id < 0) {
			// Selected cluster not found in current LOD — show nothing.
			return Ref<ArrayMesh>();
		}
	}

	// 5) For each partition, find the outer boundary edges of the merged
	// cluster set. An edge is a boundary edge if it appears in exactly one
	// triangle across all clusters in the partition, i.e. it is not shared
	// by any other triangle within the same partition.
	PackedVector3Array line_verts;

	for (size_t pid = 0; pid < partition_count; ++pid) {
		const LocalVector<size_t> &p_clusters = partitions[pid];
		if (p_clusters.size() <= 1) {
			continue;
		}
		if (filter_partition_id >= 0 && (int)pid != filter_partition_id) {
			continue; // Not the partition of the selected cluster.
		}

		// 5a) Count edge occurrences across all triangles in the partition.
		// An edge key is (min_vertex << 32) | max_vertex.
		HashMap<uint64_t, uint32_t> edge_count;
		for (size_t ci : p_clusters) {
			const ClusterInfo &info = cur_lod_clusters[ci];
			for (size_t t = 0; t + 2 < info.tri_indices.size(); t += 3) {
				unsigned int v0 = info.tri_indices[t];
				unsigned int v1 = info.tri_indices[t + 1];
				unsigned int v2 = info.tri_indices[t + 2];

				auto count_edge = [&](unsigned int a, unsigned int b) {
					uint64_t key = (uint64_t)(a < b ? a : b) << 32 | (uint64_t)(a < b ? b : a);
					HashMap<uint64_t, uint32_t>::Iterator it = edge_count.find(key);
					if (it != edge_count.end()) {
						it->value += 1;
					} else {
						edge_count[key] = 1;
					}
				};
				count_edge(v0, v1);
				count_edge(v1, v2);
				count_edge(v2, v0);
			}
		}

		// 5b) Emit edges that appear exactly once (outer boundary edges).
		for (const KeyValue<uint64_t, uint32_t> &E : edge_count) {
			if (E.value != 1) {
				continue;
			}
			uint64_t key = E.key;
			unsigned int a = (unsigned int)(key >> 32);
			unsigned int b = (unsigned int)(key & 0xFFFFFFFF);
			const float *pa = vp_base + a * (kVertexStride / sizeof(float));
			const float *pb = vp_base + b * (kVertexStride / sizeof(float));
			line_verts.push_back(Vector3(pa[0], pa[1], pa[2]));
			line_verts.push_back(Vector3(pb[0], pb[1], pb[2]));
		}
	}

	if (line_verts.is_empty()) {
		return Ref<ArrayMesh>();
	}

	print_line(vformat("[nanite-partition-border] lod=%d clusters=%d partitions=%d border_edges=%d",
			p_current_lod_level, (uint64_t)cl_cluster_count, (uint64_t)partition_count, line_verts.size() / 2));

	Array arrays;
	arrays.resize(Mesh::ARRAY_MAX);
	arrays[Mesh::ARRAY_VERTEX] = line_verts;
	Ref<ArrayMesh> mesh;
	mesh.instantiate();
	mesh->add_surface_from_arrays(Mesh::PRIMITIVE_LINES, arrays);
	return mesh;
}

// Möller-Trumbore ray-triangle intersection. Returns the distance t along
// the ray where the intersection occurs, or -1.0 if no intersection.
// p_ray_origin and p_ray_dir are in the same coordinate space as the
// triangle vertices.
float ray_triangle_intersect(const Vector3 &p_ray_origin,
		const Vector3 &p_ray_dir,
		const Vector3 &p_v0, const Vector3 &p_v1, const Vector3 &p_v2) {
	const float kEpsilon = 1e-7f;
	Vector3 edge1 = p_v1 - p_v0;
	Vector3 edge2 = p_v2 - p_v0;
	Vector3 h = p_ray_dir.cross(edge2);
	float a = edge1.dot(h);
	if (a > -kEpsilon && a < kEpsilon) {
		return -1.0f; // Ray parallel to triangle.
	}
	float f = 1.0f / a;
	Vector3 s = p_ray_origin - p_v0;
	float u = f * s.dot(h);
	if (u < 0.0f || u > 1.0f) {
		return -1.0f;
	}
	Vector3 q = s.cross(edge1);
	float v = f * p_ray_dir.dot(q);
	if (v < 0.0f || u + v > 1.0f) {
		return -1.0f;
	}
	float t = f * edge2.dot(q);
	if (t > kEpsilon) {
		return t;
	}
	return -1.0f;
}

// Build a triangle ArrayMesh for a single cluster (by global cluster index).
// Returns empty Ref on failure.
Ref<ArrayMesh> build_single_cluster_mesh(const NaniteMeshResource &p_resource,
		int p_force_lod_level,
		int p_cluster_index) {
	const PackedByteArray &clusters_data = p_resource.get_clusters_data();
	const PackedByteArray &vertex_data = p_resource.get_vertex_data();
	const PackedByteArray &meshlet_vertices_data = p_resource.get_meshlet_vertices_data();
	const PackedByteArray &meshlet_triangles_data = p_resource.get_meshlet_triangles_data();

	const int cluster_count = p_resource.get_cluster_count();
	const size_t cluster_stride = NaniteCluster::get_serialized_size();

	if (p_cluster_index < 0 || p_cluster_index >= cluster_count) {
		return Ref<ArrayMesh>();
	}
	if (vertex_data.size() < (int)kVertexStride || meshlet_vertices_data.size() < 4 ||
			meshlet_triangles_data.size() < 3) {
		return Ref<ArrayMesh>();
	}

	NaniteCluster c = NaniteCluster::deserialize(clusters_data, (uint32_t)(p_cluster_index * cluster_stride));
	if ((int)c.group_id != p_force_lod_level || c.vertex_count == 0 || c.triangle_count == 0) {
		return Ref<ArrayMesh>();
	}

	const int total_vertex_count = vertex_data.size() / kVertexStride;
	const uint32_t *mv = reinterpret_cast<const uint32_t *>(meshlet_vertices_data.ptr());
	const int mv_count = meshlet_vertices_data.size() / 4;
	const uint8_t *tri_base = meshlet_triangles_data.ptr();
	const int tri_byte_count = meshlet_triangles_data.size();
	const float *vp_base = reinterpret_cast<const float *>(vertex_data.ptr());

	if ((int)c.vertex_offset < 0 || (int)(c.vertex_offset + c.vertex_count) > mv_count) {
		return Ref<ArrayMesh>();
	}
	const uint32_t tri_bytes_needed = c.triangle_count * 3;
	if ((uint64_t)c.triangle_offset + tri_bytes_needed > (uint64_t)tri_byte_count) {
		return Ref<ArrayMesh>();
	}

	// Knuth-style hash for cluster color.
	uint32_t h = (uint32_t)p_cluster_index * 2654435761u;
	float hue = (float)(h % 360u) / 360.0f;
	Color cluster_color = Color::from_hsv(hue, 0.65f, 0.95f);

	PackedVector3Array verts;
	PackedInt32Array indices;
	PackedColorArray colors;
	PackedVector3Array normals;
	PackedVector2Array uvs;

	for (uint32_t ti = 0; ti < c.triangle_count; ++ti) {
		const uint8_t *tri_ptr = tri_base + c.triangle_offset + ti * 3;
		uint8_t i0 = tri_ptr[0], i1 = tri_ptr[1], i2 = tri_ptr[2];
		if (i0 >= c.vertex_count || i1 >= c.vertex_count || i2 >= c.vertex_count) {
			continue;
		}
		uint32_t g0 = mv[c.vertex_offset + i0];
		uint32_t g1 = mv[c.vertex_offset + i1];
		uint32_t g2 = mv[c.vertex_offset + i2];
		if (g0 >= (uint32_t)total_vertex_count || g1 >= (uint32_t)total_vertex_count || g2 >= (uint32_t)total_vertex_count) {
			continue;
		}
		const float *vp0 = vp_base + g0 * (kVertexStride / sizeof(float));
		const float *vp1 = vp_base + g1 * (kVertexStride / sizeof(float));
		const float *vp2 = vp_base + g2 * (kVertexStride / sizeof(float));

		int base = verts.size();
		verts.push_back(Vector3(vp0[0], vp0[1], vp0[2]));
		verts.push_back(Vector3(vp1[0], vp1[1], vp1[2]));
		verts.push_back(Vector3(vp2[0], vp2[1], vp2[2]));
		normals.push_back(Vector3(vp0[3], vp0[4], vp0[5]));
		normals.push_back(Vector3(vp1[3], vp1[4], vp1[5]));
		normals.push_back(Vector3(vp2[3], vp2[4], vp2[5]));
		uvs.push_back(Vector2(vp0[6], vp0[7]));
		uvs.push_back(Vector2(vp1[6], vp1[7]));
		uvs.push_back(Vector2(vp2[6], vp2[7]));
		indices.push_back(base);
		indices.push_back(base + 1);
		indices.push_back(base + 2);
		colors.push_back(cluster_color);
		colors.push_back(cluster_color);
		colors.push_back(cluster_color);
	}

	if (verts.is_empty() || indices.is_empty()) {
		return Ref<ArrayMesh>();
	}

	Array arrays;
	arrays.resize(Mesh::ARRAY_MAX);
	arrays[Mesh::ARRAY_VERTEX] = verts;
	arrays[Mesh::ARRAY_NORMAL] = normals;
	arrays[Mesh::ARRAY_TEX_UV] = uvs;
	arrays[Mesh::ARRAY_INDEX] = indices;
	arrays[Mesh::ARRAY_COLOR] = colors;
	Ref<ArrayMesh> mesh;
	mesh.instantiate();
	mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
	return mesh;
}

// Build a triangle ArrayMesh for all clusters at the given LOD, EXCLUDING
// the cluster at p_exclude_index. The returned mesh uses no per-cluster
// colors — the dimmed material controls the color and alpha.
Ref<ArrayMesh> build_cluster_mesh_excluding(const NaniteMeshResource &p_resource,
		int p_force_lod_level,
		int p_exclude_index) {
	PackedVector3Array verts;
	PackedInt32Array indices;
	PackedColorArray colors;
	PackedVector3Array normals;
	PackedVector2Array uvs;

	const PackedByteArray &clusters_data = p_resource.get_clusters_data();
	const PackedByteArray &vertex_data = p_resource.get_vertex_data();
	const PackedByteArray &meshlet_vertices_data = p_resource.get_meshlet_vertices_data();
	const PackedByteArray &meshlet_triangles_data = p_resource.get_meshlet_triangles_data();

	const int cluster_count = p_resource.get_cluster_count();
	const size_t cluster_stride = NaniteCluster::get_serialized_size();

	if (cluster_count <= 0 || clusters_data.size() < (int)(cluster_count * cluster_stride)) {
		return Ref<ArrayMesh>();
	}
	if (vertex_data.size() < (int)kVertexStride || meshlet_vertices_data.size() < 4 ||
			meshlet_triangles_data.size() < 3) {
		return Ref<ArrayMesh>();
	}

	const int total_vertex_count = vertex_data.size() / kVertexStride;
	const uint32_t *mv = reinterpret_cast<const uint32_t *>(meshlet_vertices_data.ptr());
	const int mv_count = meshlet_vertices_data.size() / 4;
	const uint8_t *tri_base = meshlet_triangles_data.ptr();
	const int tri_byte_count = meshlet_triangles_data.size();
	const float *vp_base = reinterpret_cast<const float *>(vertex_data.ptr());

	for (int ci = 0; ci < cluster_count; ++ci) {
		if (ci == p_exclude_index) {
			continue;
		}
		NaniteCluster c = NaniteCluster::deserialize(clusters_data, (uint32_t)(ci * cluster_stride));
		if ((int)c.group_id != p_force_lod_level || c.vertex_count == 0 || c.triangle_count == 0) {
			continue;
		}
		if ((int)c.vertex_offset < 0 || (int)(c.vertex_offset + c.vertex_count) > mv_count) {
			continue;
		}
		const uint32_t tri_bytes_needed = c.triangle_count * 3;
		if ((uint64_t)c.triangle_offset + tri_bytes_needed > (uint64_t)tri_byte_count) {
			continue;
		}

		for (uint32_t ti = 0; ti < c.triangle_count; ++ti) {
			const uint8_t *tri_ptr = tri_base + c.triangle_offset + ti * 3;
			uint8_t i0 = tri_ptr[0], i1 = tri_ptr[1], i2 = tri_ptr[2];
			if (i0 >= c.vertex_count || i1 >= c.vertex_count || i2 >= c.vertex_count) {
				continue;
			}
			uint32_t g0 = mv[c.vertex_offset + i0];
			uint32_t g1 = mv[c.vertex_offset + i1];
			uint32_t g2 = mv[c.vertex_offset + i2];
			if (g0 >= (uint32_t)total_vertex_count || g1 >= (uint32_t)total_vertex_count || g2 >= (uint32_t)total_vertex_count) {
				continue;
			}
			const float *vp0 = vp_base + g0 * (kVertexStride / sizeof(float));
			const float *vp1 = vp_base + g1 * (kVertexStride / sizeof(float));
			const float *vp2 = vp_base + g2 * (kVertexStride / sizeof(float));

			int base = verts.size();
			verts.push_back(Vector3(vp0[0], vp0[1], vp0[2]));
			verts.push_back(Vector3(vp1[0], vp1[1], vp1[2]));
			verts.push_back(Vector3(vp2[0], vp2[1], vp2[2]));
			normals.push_back(Vector3(vp0[3], vp0[4], vp0[5]));
			normals.push_back(Vector3(vp1[3], vp1[4], vp1[5]));
			normals.push_back(Vector3(vp2[3], vp2[4], vp2[5]));
			uvs.push_back(Vector2(vp0[6], vp0[7]));
			uvs.push_back(Vector2(vp1[6], vp1[7]));
			uvs.push_back(Vector2(vp2[6], vp2[7]));
			indices.push_back(base);
			indices.push_back(base + 1);
			indices.push_back(base + 2);
		}
	}

	if (verts.is_empty() || indices.is_empty()) {
		return Ref<ArrayMesh>();
	}

	Array arrays;
	arrays.resize(Mesh::ARRAY_MAX);
	arrays[Mesh::ARRAY_VERTEX] = verts;
	arrays[Mesh::ARRAY_NORMAL] = normals;
	arrays[Mesh::ARRAY_TEX_UV] = uvs;
	arrays[Mesh::ARRAY_INDEX] = indices;
	Ref<ArrayMesh> mesh;
	mesh.instantiate();
	mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
	return mesh;
}

// Build a triangle ArrayMesh for the clusters in the same partition as
// p_selected_cluster at the given LOD, EXCLUDING the selected cluster.
// (The "siblings" of the selected cluster — the other ~3 clusters that
// will merge with it into one at the next LOD level.)
// The returned mesh uses no per-cluster colors; the dimmed material
// controls the color and alpha. Returns empty Ref if no siblings found.
Ref<ArrayMesh> build_partition_sibling_mesh(const NaniteMeshResource &p_resource,
		int p_force_lod_level,
		int p_selected_cluster) {
	const PackedByteArray &clusters_data = p_resource.get_clusters_data();
	const PackedByteArray &vertex_data = p_resource.get_vertex_data();
	const PackedByteArray &meshlet_vertices_data = p_resource.get_meshlet_vertices_data();
	const PackedByteArray &meshlet_triangles_data = p_resource.get_meshlet_triangles_data();

	const int cluster_count = p_resource.get_cluster_count();
	const size_t cluster_stride = NaniteCluster::get_serialized_size();

	if (cluster_count <= 0 || clusters_data.size() < (int)(cluster_count * cluster_stride)) {
		return Ref<ArrayMesh>();
	}
	if (vertex_data.size() < (int)kVertexStride || meshlet_vertices_data.size() < 4 ||
			meshlet_triangles_data.size() < 3) {
		return Ref<ArrayMesh>();
	}

	const int total_vertex_count = vertex_data.size() / kVertexStride;
	const uint32_t *mv = reinterpret_cast<const uint32_t *>(meshlet_vertices_data.ptr());
	const int mv_count = meshlet_vertices_data.size() / 4;
	const uint8_t *tri_base = meshlet_triangles_data.ptr();
	const int tri_byte_count = meshlet_triangles_data.size();
	const float *vp_base = reinterpret_cast<const float *>(vertex_data.ptr());

	// 1) Collect clusters at the current LOD and build triangle index lists.
	struct ClusterInfo {
		uint32_t cluster_idx;
		LocalVector<unsigned int> tri_indices;
	};
	LocalVector<ClusterInfo> cur_lod_clusters;

	for (int ci = 0; ci < cluster_count; ++ci) {
		NaniteCluster c = NaniteCluster::deserialize(clusters_data, (uint32_t)(ci * cluster_stride));
		if ((int)c.group_id != p_force_lod_level) {
			continue;
		}
		if (c.vertex_count == 0 || c.triangle_count == 0) {
			continue;
		}
		if ((int)c.vertex_offset < 0 || (int)(c.vertex_offset + c.vertex_count) > mv_count) {
			continue;
		}
		const uint32_t tri_bytes_needed = c.triangle_count * 3;
		if ((uint64_t)c.triangle_offset + tri_bytes_needed > (uint64_t)tri_byte_count) {
			continue;
		}

		ClusterInfo info;
		info.cluster_idx = (uint32_t)ci;
		for (uint32_t t = 0; t < c.triangle_count; ++t) {
			const uint8_t *tri_ptr = tri_base + c.triangle_offset + t * 3;
			for (int k = 0; k < 3; ++k) {
				uint8_t local_idx = tri_ptr[k];
				if (local_idx >= c.vertex_count) {
					continue;
				}
				unsigned int global_v = mv[c.vertex_offset + local_idx];
				if (global_v >= (uint32_t)total_vertex_count) {
					continue;
				}
				info.tri_indices.push_back(global_v);
			}
		}
		if (info.tri_indices.size() > 0) {
			cur_lod_clusters.push_back(info);
		}
	}

	const size_t cl_cluster_count = cur_lod_clusters.size();
	if (cl_cluster_count <= 1) {
		return Ref<ArrayMesh>();
	}

	// 2) Partition clusters. Prefer stored partition_ids from the resource.
	LocalVector<unsigned int> partition_ids;
	partition_ids.resize(cl_cluster_count);
	size_t partition_count = 0;

	PackedByteArray pid_blob = p_resource.get_partition_ids_data();
	if (pid_blob.size() >= (int)(cluster_count * sizeof(uint32_t))) {
		const uint32_t *pid_base = reinterpret_cast<const uint32_t *>(pid_blob.ptr());
		for (size_t i = 0; i < cl_cluster_count; ++i) {
			uint32_t cidx = cur_lod_clusters[i].cluster_idx;
			uint32_t pid = pid_base[cidx];
			partition_ids[i] = pid;
			if (pid != UINT32_MAX && (size_t)(pid + 1) > partition_count) {
				partition_count = pid + 1;
			}
		}
	} else {
		// Fall back: recompute partitions with meshopt.
		LocalVector<unsigned int> cluster_indices;
		LocalVector<unsigned int> cluster_index_counts;
		cluster_index_counts.resize(cl_cluster_count);
		for (size_t i = 0; i < cl_cluster_count; ++i) {
			const ClusterInfo &info = cur_lod_clusters[i];
			cluster_index_counts[i] = (unsigned int)info.tri_indices.size();
			for (unsigned int idx : info.tri_indices) {
				cluster_indices.push_back(idx);
			}
		}
		size_t safe_vc = (size_t)total_vertex_count;
		for (size_t i = 0; i < cluster_indices.size(); ++i) {
			unsigned int v = cluster_indices[i];
			if (v >= safe_vc) {
				safe_vc = (size_t)v + 1;
			}
		}
		partition_count = meshopt_partitionClusters(
				partition_ids.ptr(),
				cluster_indices.ptr(),
				cluster_indices.size(),
				cluster_index_counts.ptr(),
				cl_cluster_count,
				nullptr,
				safe_vc,
				0,
				3);
	}

	if (partition_count == 0 || partition_count == cl_cluster_count) {
		return Ref<ArrayMesh>();
	}

	// 3) Find the partition containing the selected cluster.
	int target_partition = -1;
	for (size_t i = 0; i < cl_cluster_count; ++i) {
		if (cur_lod_clusters[i].cluster_idx == (uint32_t)p_selected_cluster) {
			target_partition = (int)partition_ids[i];
			break;
		}
	}
	if (target_partition < 0) {
		print_line(vformat("[nanite-sibling] selected cluster %d not found in LOD %d clusters",
				p_selected_cluster, p_force_lod_level));
		return Ref<ArrayMesh>();
	}

	// 4) Collect sibling cluster indices in the same partition.
	HashSet<int> sibling_set;
	for (size_t i = 0; i < cl_cluster_count; ++i) {
		if ((int)partition_ids[i] == target_partition) {
			sibling_set.insert((int)cur_lod_clusters[i].cluster_idx);
		}
	}
	// Remove the selected cluster itself.
	sibling_set.erase(p_selected_cluster);
	print_line(vformat("[nanite-sibling] lod=%d selected=%d target_partition=%d siblings=%d",
			p_force_lod_level, p_selected_cluster, target_partition, (int)sibling_set.size()));
	if (sibling_set.is_empty()) {
		return Ref<ArrayMesh>();
	}

	// 5) Build mesh from sibling clusters.
	PackedVector3Array verts;
	PackedInt32Array indices;
	PackedVector3Array normals;
	PackedVector2Array uvs;

	for (int ci = 0; ci < cluster_count; ++ci) {
		if (!sibling_set.has(ci)) {
			continue;
		}
		NaniteCluster c = NaniteCluster::deserialize(clusters_data, (uint32_t)(ci * cluster_stride));
		if ((int)c.group_id != p_force_lod_level || c.vertex_count == 0 || c.triangle_count == 0) {
			continue;
		}
		if ((int)c.vertex_offset < 0 || (int)(c.vertex_offset + c.vertex_count) > mv_count) {
			continue;
		}
		const uint32_t tri_bytes_needed = c.triangle_count * 3;
		if ((uint64_t)c.triangle_offset + tri_bytes_needed > (uint64_t)tri_byte_count) {
			continue;
		}

		for (uint32_t ti = 0; ti < c.triangle_count; ++ti) {
			const uint8_t *tri_ptr = tri_base + c.triangle_offset + ti * 3;
			uint8_t i0 = tri_ptr[0], i1 = tri_ptr[1], i2 = tri_ptr[2];
			if (i0 >= c.vertex_count || i1 >= c.vertex_count || i2 >= c.vertex_count) {
				continue;
			}
			uint32_t g0 = mv[c.vertex_offset + i0];
			uint32_t g1 = mv[c.vertex_offset + i1];
			uint32_t g2 = mv[c.vertex_offset + i2];
			if (g0 >= (uint32_t)total_vertex_count || g1 >= (uint32_t)total_vertex_count || g2 >= (uint32_t)total_vertex_count) {
				continue;
			}
			const float *vp0 = vp_base + g0 * (kVertexStride / sizeof(float));
			const float *vp1 = vp_base + g1 * (kVertexStride / sizeof(float));
			const float *vp2 = vp_base + g2 * (kVertexStride / sizeof(float));

			int base = verts.size();
			verts.push_back(Vector3(vp0[0], vp0[1], vp0[2]));
			verts.push_back(Vector3(vp1[0], vp1[1], vp1[2]));
			verts.push_back(Vector3(vp2[0], vp2[1], vp2[2]));
			normals.push_back(Vector3(vp0[3], vp0[4], vp0[5]));
			normals.push_back(Vector3(vp1[3], vp1[4], vp1[5]));
			normals.push_back(Vector3(vp2[3], vp2[4], vp2[5]));
			uvs.push_back(Vector2(vp0[6], vp0[7]));
			uvs.push_back(Vector2(vp1[6], vp1[7]));
			uvs.push_back(Vector2(vp2[6], vp2[7]));
			indices.push_back(base);
			indices.push_back(base + 1);
			indices.push_back(base + 2);
		}
	}

	if (verts.is_empty() || indices.is_empty()) {
		print_line("[nanite-sibling] mesh is empty — no sibling triangles");
		return Ref<ArrayMesh>();
	}

	print_line(vformat("[nanite-sibling] built mesh with %d verts, %d tris",
			verts.size(), indices.size() / 3));

	Array arrays;
	arrays.resize(Mesh::ARRAY_MAX);
	arrays[Mesh::ARRAY_VERTEX] = verts;
	arrays[Mesh::ARRAY_NORMAL] = normals;
	arrays[Mesh::ARRAY_TEX_UV] = uvs;
	arrays[Mesh::ARRAY_INDEX] = indices;
	Ref<ArrayMesh> mesh2;
	mesh2.instantiate();
	mesh2->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
	return mesh2;
}

} // namespace

// ---------------------------------------------------------------------------
// NaniteMeshEditor
// ---------------------------------------------------------------------------

void NaniteMeshEditor::_bind_methods() {
}

void NaniteMeshEditor::_update_rotation() {
	Transform3D t;
	t.basis.rotate(Vector3(0, 1, 0), -rot_y);
	t.basis.rotate(Vector3(1, 0, 0), -rot_x);
	rotation_node->set_transform(t);
	_update_camera_transform();
}

void NaniteMeshEditor::_update_camera_transform() {
	// Camera position in rotation_node local space:
	//   (pan_offset.x, pan_offset.y, camera_distance)
	// The camera always looks at (0, 0, 0) in rotation_node local space.
	Transform3D ct;
	ct.origin = Vector3(pan_offset.x, pan_offset.y, camera_distance);
	ct.basis = Basis::looking_at(-ct.origin.normalized(), Vector3(0, 1, 0));
	camera->set_transform(ct);
}

void NaniteMeshEditor::_focus_on_model() {
	if (current_resource.is_null()) {
		return;
	}

	Ref<ArrayMesh> shadow_mesh = current_resource->get_shadow_mesh();
	if (shadow_mesh.is_valid()) {
		AABB aabb = shadow_mesh->get_aabb();
		if (aabb.size.length() > 0.0) {
			camera_distance = aabb.size.length() * 1.2f;
		} else {
			camera_distance = 3.0f;
		}
	} else {
		camera_distance = 3.0f;
	}

	pan_offset = Vector2();
	rot_x = Math::deg_to_rad(-15.0f);
	rot_y = Math::deg_to_rad(30.0f);
	_update_rotation();
	_update_camera_transform();
}

int NaniteMeshEditor::_ray_pick_cluster(const Vector2 &p_screen_pos) {
	if (current_resource.is_null()) {
		return -1;
	}

	const PackedByteArray &clusters_data = current_resource->get_clusters_data();
	const PackedByteArray &vertex_data = current_resource->get_vertex_data();
	const PackedByteArray &meshlet_vertices_data = current_resource->get_meshlet_vertices_data();
	const PackedByteArray &meshlet_triangles_data = current_resource->get_meshlet_triangles_data();

	const int cluster_count = current_resource->get_cluster_count();
	const size_t cluster_stride = NaniteCluster::get_serialized_size();
	const int force_lod = force_lod_level;

	if (cluster_count <= 0 || clusters_data.size() < (int)(cluster_count * cluster_stride)) {
		return -1;
	}
	if (vertex_data.size() < (int)kVertexStride || meshlet_vertices_data.size() < 4 ||
			meshlet_triangles_data.size() < 3) {
		return -1;
	}

	// Get ray in world space from the camera.
	Vector3 ray_origin = camera->project_ray_origin(p_screen_pos);
	Vector3 ray_dir = camera->project_ray_normal(p_screen_pos);

	// Transform ray to rotation_node local space (where mesh instances live).
	Transform3D inv_transform = rotation_node->get_global_transform().affine_inverse();
	Vector3 local_origin = inv_transform.xform(ray_origin);
	Vector3 local_dir = inv_transform.basis.xform(ray_dir).normalized();

	const int total_vertex_count = vertex_data.size() / kVertexStride;
	const uint32_t *mv = reinterpret_cast<const uint32_t *>(meshlet_vertices_data.ptr());
	const int mv_count = meshlet_vertices_data.size() / 4;
	const uint8_t *tri_base = meshlet_triangles_data.ptr();
	const int tri_byte_count = meshlet_triangles_data.size();
	const float *vp_base = reinterpret_cast<const float *>(vertex_data.ptr());

	int best_cluster = -1;
	float best_t = 1e30f;

	for (int ci = 0; ci < cluster_count; ++ci) {
		NaniteCluster c = NaniteCluster::deserialize(clusters_data, (uint32_t)(ci * cluster_stride));
		if ((int)c.group_id != force_lod || c.vertex_count == 0 || c.triangle_count == 0) {
			continue;
		}
		if ((int)c.vertex_offset < 0 || (int)(c.vertex_offset + c.vertex_count) > mv_count) {
			continue;
		}
		const uint32_t tri_bytes_needed = c.triangle_count * 3;
		if ((uint64_t)c.triangle_offset + tri_bytes_needed > (uint64_t)tri_byte_count) {
			continue;
		}

		for (uint32_t ti = 0; ti < c.triangle_count; ++ti) {
			const uint8_t *tri_ptr = tri_base + c.triangle_offset + ti * 3;
			uint8_t i0 = tri_ptr[0], i1 = tri_ptr[1], i2 = tri_ptr[2];
			if (i0 >= c.vertex_count || i1 >= c.vertex_count || i2 >= c.vertex_count) {
				continue;
			}
			uint32_t g0 = mv[c.vertex_offset + i0];
			uint32_t g1 = mv[c.vertex_offset + i1];
			uint32_t g2 = mv[c.vertex_offset + i2];
			if (g0 >= (uint32_t)total_vertex_count || g1 >= (uint32_t)total_vertex_count || g2 >= (uint32_t)total_vertex_count) {
				continue;
			}
			const float *vp0 = vp_base + g0 * (kVertexStride / sizeof(float));
			const float *vp1 = vp_base + g1 * (kVertexStride / sizeof(float));
			const float *vp2 = vp_base + g2 * (kVertexStride / sizeof(float));
			Vector3 v0(vp0[0], vp0[1], vp0[2]);
			Vector3 v1(vp1[0], vp1[1], vp1[2]);
			Vector3 v2(vp2[0], vp2[1], vp2[2]);

			float t = ray_triangle_intersect(local_origin, local_dir, v0, v1, v2);
			if (t > 0.0f && t < best_t) {
				best_t = t;
				best_cluster = ci;
			}
		}
	}

	return best_cluster;
}

void NaniteMeshEditor::_cycle_cluster_in_partition(int p_direction) {
	// Only active in Cluster Solid + Partition Border mode with a selection.
	const int display_id = current_display_mode;
	if (display_id != NaniteDebug::CLUSTER_SOLID_WITH_PARTITION_BORDER) {
		return;
	}
	if (selected_cluster_index < 0 || current_resource.is_null()) {
		return;
	}

	const int cluster_count = current_resource->get_cluster_count();
	if (selected_cluster_index >= cluster_count) {
		return;
	}

	// Need stored partition_ids — without them we can't reliably identify
	// partition siblings (meshopt recompute is order-sensitive and may
	// differ from build-time data).
	PackedByteArray pid_blob = current_resource->get_partition_ids_data();
	if (pid_blob.size() < (int)(cluster_count * sizeof(uint32_t))) {
		print_line("[nanite-cycle] no stored partition_ids_data — cannot cycle");
		return;
	}
	const uint32_t *pid_base = reinterpret_cast<const uint32_t *>(pid_blob.ptr());
	const uint32_t target_pid = pid_base[selected_cluster_index];
	if (target_pid == UINT32_MAX) {
		print_line("[nanite-cycle] selected cluster has no partition_id");
		return;
	}

	// Collect all clusters in the same LOD + same partition (sorted by
	// global cluster index for stable navigation).
	const int force_lod = force_lod_level;
	const size_t cluster_stride = NaniteCluster::get_serialized_size();
	const PackedByteArray &clusters_data = current_resource->get_clusters_data();

	LocalVector<int> siblings;
	for (int ci = 0; ci < cluster_count; ++ci) {
		if (pid_base[ci] != target_pid) {
			continue;
		}
		NaniteCluster c = NaniteCluster::deserialize(clusters_data,
				(uint32_t)(ci * cluster_stride));
		if ((int)c.group_id != force_lod) {
			continue;
		}
		siblings.push_back(ci);
	}

	if (siblings.size() <= 1) {
		print_line(vformat("[nanite-cycle] partition %d has %d siblings — nothing to cycle",
				(int)target_pid, (int)siblings.size()));
		return;
	}

	// Find current position and advance (wrapping).
	int cur_pos = -1;
	for (size_t i = 0; i < siblings.size(); ++i) {
		if (siblings[i] == selected_cluster_index) {
			cur_pos = (int)i;
			break;
		}
	}
	if (cur_pos < 0) {
		return; // Should not happen, defensive.
	}

	int new_pos = cur_pos + (p_direction > 0 ? 1 : -1);
	if (new_pos < 0) {
		new_pos = (int)siblings.size() - 1; // Wrap backwards.
	} else if (new_pos >= (int)siblings.size()) {
		new_pos = 0; // Wrap forwards.
	}

	int new_cluster = siblings[new_pos];
	print_line(vformat("[nanite-cycle] partition=%d cur=%d(pos %d/%d) -> new=%d(pos %d)",
			(int)target_pid, selected_cluster_index, cur_pos, (int)siblings.size(),
			new_cluster, new_pos));
	selected_cluster_index = new_cluster;
	_rebuild_preview();
}

void NaniteMeshEditor::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_FOCUS_ENTER: {
			// No-op: Stage 0 preview rendering is fully local; no
			// NaniteServer debug state needs to be touched.
		} break;
		case NOTIFICATION_FOCUS_EXIT: {
			// No-op for the same reason.
		} break;
		default: {
			// No-op for other notifications.
		} break;
	}
}

void NaniteMeshEditor::_on_display_mode_selected(int p_index) {
	if (updating_ui) {
		return;
	}
	current_display_mode = display_mode_btn->get_item_id(p_index);
	selected_cluster_index = -1; // Clear selection on mode change.
	_rebuild_preview();
}

void NaniteMeshEditor::_on_lod_mode_selected(int p_index) {
	if (updating_ui) {
		return;
	}
	int mode = lod_mode_btn->get_item_id(p_index);
	if (mode == NaniteDebug::NANITE_AUTO) {
		// Stage 0 has no Nanite GPU cull/LOD pipeline; warn and revert.
		WARN_PRINT("Nanite auto cull + LOD selection is a Stage 1 feature. "
				   "Falling back to Force LOD Level.");
		updating_ui = true;
		lod_mode_btn->select(NaniteDebug::FORCE_LOD_LEVEL);
		updating_ui = false;
		return;
	}
	current_lod_mode = NaniteDebug::FORCE_LOD_LEVEL;
	selected_cluster_index = -1;
	_refresh_lod_buttons();
	_rebuild_preview();
}

void NaniteMeshEditor::_on_lod_minus_pressed() {
	if (force_lod_level > 0) {
		force_lod_level--;
		selected_cluster_index = -1;
		_refresh_lod_buttons();
		_rebuild_preview();
	}
}

void NaniteMeshEditor::_on_lod_plus_pressed() {
	if (force_lod_level < max_lod_level) {
		force_lod_level++;
		selected_cluster_index = -1;
		_refresh_lod_buttons();
		_rebuild_preview();
	}
}

void NaniteMeshEditor::_refresh_lod_buttons() {
	// Show the [-] [N] [+] row only when LOD Mode == Force LOD Level.
	bool show_row = (current_lod_mode == NaniteDebug::FORCE_LOD_LEVEL);
	if (lod_buttons_row) {
		lod_buttons_row->set_visible(show_row);
	}
	if (lod_value_label) {
		lod_value_label->set_text(vformat("%d", force_lod_level));
	}
	if (lod_minus_button) {
		lod_minus_button->set_disabled(force_lod_level <= 0);
	}
	if (lod_plus_button) {
		lod_plus_button->set_disabled(force_lod_level >= max_lod_level);
	}
}

void NaniteMeshEditor::gui_input(const Ref<InputEvent> &p_event) {
	ERR_FAIL_COND(p_event.is_null());

	// --- Keyboard ---
	Ref<InputEventKey> k = p_event;
	if (k.is_valid() && k->is_pressed()) {
		// F / ESC / Page Up / Page Down: single press only (no echo / key repeat).
		if (!k->is_echo()) {
			if (k->get_keycode() == Key::F) {
				_focus_on_model();
				accept_event();
				return;
			}
			if (k->get_keycode() == Key::ESCAPE) {
				if (selected_cluster_index >= 0) {
					selected_cluster_index = -1;
					_rebuild_preview();
					accept_event();
					return;
				}
			}
			// Page Up / Page Down: cycle selection within the same partition
			// in Cluster Solid + Partition Border mode.
			if (k->get_keycode() == Key::PAGEUP) {
				_cycle_cluster_in_partition(-1);
				accept_event();
				return;
			}
			if (k->get_keycode() == Key::PAGEDOWN) {
				_cycle_cluster_in_partition(+1);
				accept_event();
				return;
			}
		}

		// WASD / QE / Arrow keys for camera movement.
		// Echo (key repeat) is allowed so holding a key moves continuously.
		Vector3 cam_pos(pan_offset.x, pan_offset.y, camera_distance);
		Vector3 cam_forward = -cam_pos.normalized();
		Vector3 cam_right = cam_forward.cross(Vector3(0, 1, 0)).normalized();
		if (cam_right.length_squared() < 0.001f) {
			cam_right = Vector3(1, 0, 0);
		}
		Vector3 cam_up = cam_right.cross(cam_forward).normalized();
		const float move_speed = camera_distance * 0.05f;
		Vector3 move_delta;

		Key keycode = k->get_keycode();
		if (keycode == Key::W || keycode == Key::UP) {
			move_delta += cam_forward * move_speed;
		} else if (keycode == Key::S || keycode == Key::DOWN) {
			move_delta -= cam_forward * move_speed;
		} else if (keycode == Key::A || keycode == Key::LEFT) {
			move_delta -= cam_right * move_speed;
		} else if (keycode == Key::D || keycode == Key::RIGHT) {
			move_delta += cam_right * move_speed;
		} else if (keycode == Key::Q) {
			move_delta += cam_up * move_speed;
		} else if (keycode == Key::E) {
			move_delta -= cam_up * move_speed;
		}

		if (move_delta.length_squared() > 0.0f) {
			pan_offset.x += move_delta.x;
			pan_offset.y += move_delta.y;
			float forward_dot = move_delta.dot(cam_forward);
			camera_distance = MAX(camera_distance - forward_dot, 0.01f);
			_update_camera_transform();
			accept_event();
			return;
		}
	}

	// --- Mouse wheel: zoom ---
	Ref<InputEventMouseButton> mb = p_event;
	if (mb.is_valid()) {
		// Skip wheel events over the top-right UI overlay so the controls
		// (dropdowns / LOD adjust row) can receive them.
		if (ui_bar && ui_bar->get_global_rect().has_point(mb->get_global_position())) {
			// ui_bar spans the full rect (PRESET_FULL_RECT); narrow the skip
			// to the actual VBox area on the right by checking its children.
			bool over_control = false;
			if (display_mode_btn && display_mode_btn->get_global_rect().has_point(mb->get_global_position())) {
				over_control = true;
			} else if (lod_mode_btn && lod_mode_btn->get_global_rect().has_point(mb->get_global_position())) {
				over_control = true;
			} else if (lod_buttons_row && lod_buttons_row->is_visible() &&
					lod_buttons_row->get_global_rect().has_point(mb->get_global_position())) {
				over_control = true;
			}
			if (over_control) {
				return;
			}
		}

		if (mb->get_button_index() == MouseButton::WHEEL_UP) {
			camera_distance = MAX(camera_distance * 0.9f, 0.01f);
			_update_camera_transform();
			accept_event();
			return;
		}
		if (mb->get_button_index() == MouseButton::WHEEL_DOWN) {
			camera_distance = MAX(camera_distance * 1.1f, 0.01f);
			_update_camera_transform();
			accept_event();
			return;
		}

		if (mb->get_button_index() == MouseButton::LEFT) {
			if (mb->is_pressed()) {
				click_pos = mb->get_position();
				// Check Shift modifier for pan mode.
				if (mb->is_shift_pressed()) {
					shift_panning = true;
					accept_event();
				} else {
					dragging = true;
					accept_event();
				}
			} else {
				// Button released.
				if (shift_panning) {
					shift_panning = false;
					accept_event();
				} else if (dragging) {
					// Check if this was a click (not a drag) — if so, try
					// cluster selection in Cluster Solid modes.
					float drag_dist = (mb->get_position() - click_pos).length();
					constexpr float kClickThreshold = 5.0f;
					if (drag_dist < kClickThreshold) {
						const int display_id = current_display_mode;
						if (display_id == NaniteDebug::CLUSTER_SOLID ||
								display_id == NaniteDebug::CLUSTER_SOLID_WIREFRAME ||
								display_id == NaniteDebug::CLUSTER_SOLID_WITH_PARTITION_BORDER) {
							int hit = _ray_pick_cluster(mb->get_position());
							if (hit >= 0) {
								selected_cluster_index = hit;
								print_line(vformat("[nanite-pick] selected cluster %d", hit));
							} else {
								selected_cluster_index = -1;
								print_line("[nanite-pick] no cluster hit — deselected");
							}
							_rebuild_preview();
						}
					}
					dragging = false;
					accept_event();
				}
			}
			return;
		}

		if (mb->get_button_index() == MouseButton::MIDDLE) {
			if (mb->is_pressed()) {
				panning = true;
				accept_event();
			} else {
				panning = false;
				accept_event();
			}
			return;
		}

		return;
	}

	// --- Mouse motion: pan / rotate ---
	Ref<InputEventMouseMotion> mm = p_event;
	if (mm.is_valid()) {
		if (panning || shift_panning) {
			// Screen-space pan: compute camera's right/up axes in
			// rotation_node local space, then move the camera position
			// along those axes. This ensures pan direction always matches
			// mouse movement regardless of rotation_node orientation.
			const float pan_speed = camera_distance * 0.002f;
			Vector3 cam_pos(pan_offset.x, pan_offset.y, camera_distance);
			Vector3 cam_forward = -cam_pos.normalized();
			Vector3 cam_right = cam_forward.cross(Vector3(0, 1, 0)).normalized();
			if (cam_right.length_squared() < 0.001f) {
				cam_right = Vector3(1, 0, 0);
			}
			Vector3 cam_up = cam_right.cross(cam_forward).normalized();
			// Mouse moves right → scene moves right → camera moves left.
			Vector3 delta = -cam_right * mm->get_relative().x * pan_speed
							- cam_up * mm->get_relative().y * pan_speed;
			pan_offset.x += delta.x;
			pan_offset.y += delta.y;
			_update_camera_transform();
			accept_event();
			return;
		}
		if (dragging) {
			// Left-button orbit rotation (existing behavior).
			rot_x -= mm->get_relative().y * 0.01;
			rot_y -= mm->get_relative().x * 0.01;

			rot_x = CLAMP(rot_x, -Math::PI / 2, Math::PI / 2);
			_update_rotation();
			accept_event();
		}
	}
}

void NaniteMeshEditor::_rebuild_preview() {
	if (!current_resource.is_valid()) {
		print_line("[nanite-preview] no current_resource — clearing meshes");
		solid_instance->set_mesh(Ref<Mesh>());
		wire_instance->set_mesh(Ref<Mesh>());
		partition_border_solid_instance->set_mesh(Ref<Mesh>());
		partition_border_instance->set_mesh(Ref<Mesh>());
		dimmed_instance->set_mesh(Ref<Mesh>());
		dimmed_instance->set_visible(false);
		return;
	}

	const int display_id = current_display_mode;
	const int lod_mode = current_lod_mode;
	const int force_lod = force_lod_level;

	print_line(vformat("[nanite-preview] display_id=%d lod_mode=%d force_lod=%d selected_cluster=%d",
			display_id, lod_mode, force_lod, selected_cluster_index));

	// Decide what to render in the solid pass + wireframe pass.
	// - When LOD mode is FORCE_LOD_LEVEL: use cluster decode for ALL
	//   display modes. The force_lod spinner controls which LOD to render.
	//   This is the Stage 0 path.
	// - When LOD mode is NANITE_AUTO: use the resource's shadow_mesh (a
	//   coarse ArrayMesh). This is the Stage 1 placeholder.
	// - NORMAL / NORMAL_WIREFRAME: render the decoded mesh with Lambert
	//   shading (no per-cluster colors).
	// - CLUSTER_SOLID / CLUSTER_SOLID_WIREFRAME: render with per-cluster
	//   HSV vertex colors.
	// - WIREFRAME_ONLY: render only the wireframe overlay.
	Ref<ArrayMesh> solid_mesh;
	Ref<ArrayMesh> wire_mesh; // PRIMITIVE_LINES for wireframe overlay
	Ref<ArrayMesh> dimmed_mesh; // Dimmed mesh for non-selected clusters
	bool wire_visible = false;
	bool solid_visible = true;
	bool partition_border_visible = false;
	bool dimmed_visible = false;

	if (lod_mode == NaniteDebug::FORCE_LOD_LEVEL) {
		// Stage 0: use cluster decode for all display modes.
		const bool is_cluster_solid_mode =
				(display_id == NaniteDebug::CLUSTER_SOLID ||
						display_id == NaniteDebug::CLUSTER_SOLID_WIREFRAME ||
						display_id == NaniteDebug::CLUSTER_SOLID_WITH_PARTITION_BORDER);
		const bool per_cluster_colors = is_cluster_solid_mode;

		// Handle cluster selection in Cluster Solid modes.
		if (is_cluster_solid_mode && selected_cluster_index >= 0) {
			// Render only the selected cluster as solid.
			solid_mesh = build_single_cluster_mesh(*current_resource.ptr(), force_lod, selected_cluster_index);
			if (display_id == NaniteDebug::CLUSTER_SOLID_WITH_PARTITION_BORDER) {
				// In Partition Border mode: only same-partition siblings
				// are rendered semi-transparent; all other clusters are hidden.
				dimmed_mesh = build_partition_sibling_mesh(*current_resource.ptr(), force_lod, selected_cluster_index);
			} else {
				// In other Cluster Solid modes: all non-selected clusters
				// are rendered semi-transparent.
				dimmed_mesh = build_cluster_mesh_excluding(*current_resource.ptr(), force_lod, selected_cluster_index);
			}
			dimmed_visible = !dimmed_mesh.is_null();
		} else {
			solid_mesh = build_cluster_mesh(*current_resource.ptr(), force_lod, per_cluster_colors);
		}

		wire_visible = (display_id == NaniteDebug::NORMAL_WIREFRAME ||
						display_id == NaniteDebug::CLUSTER_SOLID_WIREFRAME ||
						display_id == NaniteDebug::WIREFRAME_ONLY);
		partition_border_visible = (display_id == NaniteDebug::CLUSTER_SOLID_WITH_PARTITION_BORDER);
		if (partition_border_visible) {
			// When a cluster is selected, only show the partition border
			// for the partition containing that cluster.
			wire_mesh = build_partition_border_wire(*current_resource.ptr(), force_lod,
					selected_cluster_index);
			partition_border_visible = !wire_mesh.is_null();
		} else if (wire_visible) {
			wire_mesh = build_cluster_wire_mesh(*current_resource.ptr(), force_lod);
		}
		if (display_id == NaniteDebug::WIREFRAME_ONLY) {
			solid_visible = false;
		}
	} else {
		// NANITE_AUTO (Stage 1 placeholder): use shadow_mesh.
		solid_mesh = current_resource->get_shadow_mesh();
		wire_visible = (display_id == NaniteDebug::NORMAL_WIREFRAME);
		if (wire_visible && solid_mesh.is_valid()) {
			wire_mesh = build_wire_from_array_mesh(solid_mesh);
		}
	}

	solid_instance->set_mesh(solid_mesh);
	wire_instance->set_mesh(wire_mesh);
	wire_instance->set_visible(wire_visible && !wire_mesh.is_null());
	solid_instance->set_visible(solid_visible && !solid_mesh.is_null());
	// Partition border uses two dedicated overlay instances:
	// - solid (depth-test enabled, unshaded yellow): surface lines.
	// - dashed (depth-test disabled, stipple shader): occluded lines.
	// Both share the same border-edge mesh.
	partition_border_solid_instance->set_mesh(partition_border_visible ? Ref<Mesh>(wire_mesh) : Ref<Mesh>());
	partition_border_solid_instance->set_visible(partition_border_visible);
	partition_border_instance->set_mesh(partition_border_visible ? Ref<Mesh>(wire_mesh) : Ref<Mesh>());
	partition_border_instance->set_visible(partition_border_visible);
	// Dimmed instance: render ghosted clusters when a cluster is selected.
	dimmed_instance->set_mesh(dimmed_mesh);
	dimmed_instance->set_visible(dimmed_visible);
	if (dimmed_visible) {
		AABB aabb = dimmed_mesh->get_aabb();
		print_line(vformat("[nanite-dimmed] visible=true mesh_valid=%d surface_count=%d aabb=(%.2f,%.2f,%.2f)-(%.2f,%.2f,%.2f) size=%.2f",
				(int)dimmed_mesh.is_valid(),
				dimmed_mesh.is_valid() ? dimmed_mesh->get_surface_count() : 0,
				aabb.position.x, aabb.position.y, aabb.position.z,
				aabb.position.x + aabb.size.x, aabb.position.y + aabb.size.y, aabb.position.z + aabb.size.z,
				aabb.size.length()));
	}

	// Reset wireframe material tint to white (no longer needed for partition
	// border mode — that uses its own dedicated instance + shader).
	{
		Ref<Material> wire_mat = wire_instance->get_material_override();
		StandardMaterial3D *smat = Object::cast_to<StandardMaterial3D>(wire_mat.ptr());
		if (smat) {
			smat->set_albedo(Color(1.0f, 1.0f, 1.0f));
		}
	}

	print_line(vformat("[nanite-preview] final: solid_mesh_valid=%d wire_mesh_valid=%d solid_visible=%d wire_visible=%d partition_border_visible=%d",
			(int)solid_mesh.is_valid(), (int)wire_mesh.is_valid(), (int)solid_visible, (int)wire_visible, (int)partition_border_visible));

	// Refresh stats label so selected cluster / partition details stay in
	// sync with the current selection + display mode + force_lod.
	_update_stats_label();
}

void NaniteMeshEditor::_update_stats_label() {
	if (stats_label == nullptr) {
		return;
	}
	if (current_resource.is_null()) {
		stats_label->set_text("");
		return;
	}

	const int cluster_count = current_resource->get_cluster_count();
	const int node_count = current_resource->get_node_count();
	const int page_count = current_resource->get_page_count();
	const int max_lod = current_resource->get_max_lod_level();

	int shadow_tri_count = 0;
	Ref<ArrayMesh> shadow_mesh = current_resource->get_shadow_mesh();
	if (shadow_mesh.is_valid()) {
		const int surface_count = shadow_mesh->get_surface_count();
		for (int i = 0; i < surface_count; ++i) {
			const int index_count = shadow_mesh->surface_get_array_index_len(i);
			if (index_count > 0) {
				shadow_tri_count += index_count / 3;
			} else {
				const int vert_count = shadow_mesh->surface_get_array_len(i);
				shadow_tri_count += vert_count / 3;
			}
		}
	}

	const uint64_t vertex_bytes = current_resource->get_vertex_data().size();
	const uint64_t clusters_bytes = current_resource->get_clusters_data().size();
	const uint64_t nodes_bytes = current_resource->get_nodes_data().size();
	const uint64_t page_table_bytes = current_resource->get_page_table_data().size();
	const uint64_t total_bytes = vertex_bytes + clusters_bytes + nodes_bytes + page_table_bytes;
	const double total_mb = static_cast<double>(total_bytes) / (1024.0 * 1024.0);

	String stats_text = vformat(
			"Clusters: %d  |  Nodes: %d  |  Pages: %d\n"
			"Shadow Tris: %d  |  Est. GPU: %.2f MB  |  Max LOD: %d",
			cluster_count, node_count, page_count,
			shadow_tri_count, total_mb, max_lod);

	// Append selected cluster + partition details when a cluster is picked
	// in a Cluster Solid mode.
	const int display_id = current_display_mode;
	const bool is_cluster_solid_mode =
			(display_id == NaniteDebug::CLUSTER_SOLID ||
					display_id == NaniteDebug::CLUSTER_SOLID_WIREFRAME ||
					display_id == NaniteDebug::CLUSTER_SOLID_WITH_PARTITION_BORDER);

	if (is_cluster_solid_mode && selected_cluster_index >= 0 &&
			selected_cluster_index < cluster_count) {
		const size_t cluster_stride = NaniteCluster::get_serialized_size();
		const PackedByteArray &clusters_data = current_resource->get_clusters_data();
		NaniteCluster c = NaniteCluster::deserialize(clusters_data,
				(uint32_t)(selected_cluster_index * cluster_stride));

		// Read partition_id from the stored blob (matches build-time data).
		// UINT32_MAX means no partition data available (e.g. optimize_size export).
		uint32_t partition_id = UINT32_MAX;
		PackedByteArray pid_blob = current_resource->get_partition_ids_data();
		const uint32_t *pid_base = nullptr;
		if (pid_blob.size() >= (int)(cluster_count * sizeof(uint32_t))) {
			pid_base = reinterpret_cast<const uint32_t *>(pid_blob.ptr());
			partition_id = pid_base[selected_cluster_index];
		}

		// Count sibling clusters in the same LOD + same partition.
		int partition_cluster_count = 0;
		if (partition_id != UINT32_MAX && pid_base != nullptr) {
			const int force_lod = force_lod_level;
			for (int ci = 0; ci < cluster_count; ++ci) {
				NaniteCluster oc = NaniteCluster::deserialize(clusters_data,
						(uint32_t)(ci * cluster_stride));
				if ((int)oc.group_id != force_lod) {
					continue;
				}
				if (pid_base[ci] == partition_id) {
					partition_cluster_count++;
				}
			}
		}

		const AABB &b = c.bounds;
		stats_text += vformat("\n--- Selected Cluster ---\n"
				"Index: %d  |  LOD: %d  |  Material: %d\n"
				"Verts: %d  |  Tris: %d  |  Error: %.4f\n"
				"Bounds: (%.2f,%.2f,%.2f) Size: (%.2f,%.2f,%.2f)\n"
				"Partition ID: %d  |  Clusters in Partition: %d",
				selected_cluster_index,
				c.group_id,
				c.material_index,
				c.vertex_count,
				c.triangle_count,
				c.error,
				b.position.x, b.position.y, b.position.z,
				b.size.x, b.size.y, b.size.z,
				(partition_id == UINT32_MAX ? -1 : (int)partition_id),
				partition_cluster_count);
	}

	stats_label->set_text(stats_text);
}

void NaniteMeshEditor::edit(const Ref<NaniteMeshResource> &p_resource) {
	current_resource = p_resource;

	if (current_resource.is_null()) {
		print_line("[nanite-edit] resource is null");
		solid_instance->set_mesh(Ref<Mesh>());
		wire_instance->set_mesh(Ref<Mesh>());
		partition_border_solid_instance->set_mesh(Ref<Mesh>());
		partition_border_instance->set_mesh(Ref<Mesh>());
		stats_label->set_text("");
		return;
	}

	print_line(vformat("[nanite-edit] resource loaded: cluster_count=%d node_count=%d page_count=%d",
			current_resource->get_cluster_count(), current_resource->get_node_count(), current_resource->get_page_count()));
	print_line(vformat("[nanite-edit] blob sizes: vd=%d cd=%d nd=%d pt=%d md=%d mvd=%d mtd=%d",
			current_resource->get_vertex_data().size(),
			current_resource->get_clusters_data().size(),
			current_resource->get_nodes_data().size(),
			current_resource->get_page_table_data().size(),
			current_resource->get_materials_data().size(),
			current_resource->get_meshlet_vertices_data().size(),
			current_resource->get_meshlet_triangles_data().size()));
	print_line(vformat("[nanite-edit] shadow_mesh valid=%d", (int)current_resource->get_shadow_mesh().is_valid()));

	// Refresh the Force LOD range from the resource's max LOD level.
	max_lod_level = current_resource->get_max_lod_level();
	if (force_lod_level > max_lod_level) {
		force_lod_level = MAX(max_lod_level, 0);
	}
	_refresh_lod_buttons();

	// Build stats label text (general resource stats; selected cluster
	// details are appended by _update_stats_label() when applicable).
	_update_stats_label();

	// Auto-fit camera distance based on shadow mesh AABB (if present).
	_focus_on_model();

	_rebuild_preview();
}

NaniteMeshEditor::NaniteMeshEditor() {
	viewport = memnew(SubViewport);
	Ref<World3D> world_3d;
	world_3d.instantiate();
	viewport->set_world_3d(world_3d); // Use own world.
	add_child(viewport);
	viewport->set_disable_input(true);
	viewport->set_msaa_3d(Viewport::MSAA_4X);
	set_stretch(true);

	// NOTE: Stage 0 preview deliberately does NOT attach the Nanite
	// CompositorEffect to this SubViewport — the preview is rendered with
	// Godot's standard forward pipeline via MeshInstance3D so it works
	// regardless of whether a Nanite bridge is compiled in.

	camera = memnew(Camera3D);
	camera->set_transform(Transform3D(Basis(), Vector3(0, 0, 3)));
	camera->set_perspective(45, 0.1, 100);
	viewport->add_child(camera);

	light1 = memnew(DirectionalLight3D);
	light1->set_transform(Transform3D().looking_at(Vector3(-1, -1, -1), Vector3(0, 1, 0)));
	viewport->add_child(light1);

	light2 = memnew(DirectionalLight3D);
	light2->set_transform(Transform3D().looking_at(Vector3(0, 1, 0), Vector3(0, 0, 1)));
	light2->set_color(Color(0.7, 0.7, 0.7));
	viewport->add_child(light2);

	rotation_node = memnew(Node3D);
	viewport->add_child(rotation_node);

	// Solid instance: default Lambert-shaded StandardMaterial3D that picks
	// up vertex colors when present (FLAG_ALBEDO_FROM_VERTEX_COLOR).
	solid_instance = memnew(MeshInstance3D);
	{
		Ref<StandardMaterial3D> mat;
		mat.instantiate();
		mat->set_albedo(Color(0.75, 0.78, 0.82));
		mat->set_flag(BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
		mat->set_cull_mode(BaseMaterial3D::CULL_DISABLED); // see both sides of debug mesh
		solid_instance->set_material_override(mat);
	}
	rotation_node->add_child(solid_instance);

	// Wire instance: unshaded white line mesh (PRIMITIVE_LINES). Visible
	// only in NORMAL_WIREFRAME / CLUSTER_SOLID_WIREFRAME / WIREFRAME_ONLY
	// modes. The mesh itself is built as PRIMITIVE_LINES so no wireframe
	// material flag is needed.
	wire_instance = memnew(MeshInstance3D);
	{
		Ref<StandardMaterial3D> wmat;
		wmat.instantiate();
		wmat->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
		wmat->set_albedo(Color(1, 1, 1));
		wmat->set_flag(BaseMaterial3D::FLAG_DISABLE_DEPTH_TEST, false);
		wmat->set_cull_mode(BaseMaterial3D::CULL_DISABLED);
		wire_instance->set_material_override(wmat);
	}
	rotation_node->add_child(wire_instance);
	wire_instance->set_visible(false);

	// Partition border solid instance: solid yellow lines on the model
	// surface (depth-test enabled). Only shown in
	// CLUSTER_SOLID_WITH_PARTITION_BORDER mode.
	// Paired with partition_border_instance (dashed, depth-test disabled)
	// so that occluded border edges are still visible as dashed lines.
	partition_border_solid_instance = memnew(MeshInstance3D);
	{
		Ref<StandardMaterial3D> smat;
		smat.instantiate();
		smat->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
		smat->set_albedo(Color(1.0f, 0.85f, 0.1f));
		smat->set_flag(BaseMaterial3D::FLAG_DISABLE_DEPTH_TEST, false);
		smat->set_cull_mode(BaseMaterial3D::CULL_DISABLED);
		partition_border_solid_instance->set_material_override(smat);
	}
	rotation_node->add_child(partition_border_solid_instance);
	partition_border_solid_instance->set_visible(false);

	// Partition border dashed instance: always-on-top dashed (stippled)
	// yellow wireframe overlay for occluded edges (depth-test disabled).
	// Only shown in CLUSTER_SOLID_WITH_PARTITION_BORDER mode.
	partition_border_instance = memnew(MeshInstance3D);
	{
		Ref<ShaderMaterial> pmat;
		pmat.instantiate();
		Ref<Shader> pshader;
		pshader.instantiate();
		pshader->set_code(R"(
shader_type spatial;
render_mode unshaded, cull_disabled, depth_test_disabled;

uniform vec4 albedo : source_color;

void fragment() {
	// Stipple pattern: discard every other 2-pixel block in a
	// checkerboard pattern, producing a dashed-line appearance.
	vec2 pixel = FRAGCOORD.xy;
	float pattern = mod(pixel.x + pixel.y, 4.0);
	if (pattern >= 2.0) {
		discard;
	}
	ALBEDO = albedo.rgb;
}
)");
		pmat->set_shader(pshader);
		pmat->set_shader_parameter("albedo", Color(1.0f, 0.85f, 0.1f, 1.0f));
		partition_border_instance->set_material_override(pmat);
	}
	rotation_node->add_child(partition_border_instance);
	partition_border_instance->set_visible(false);

	// Dimmed instance: semi-transparent overlay for non-selected sibling
	// clusters in the same partition. Uses default (per-pixel) shading
	// with TRANSPARENCY_ALPHA to ensure alpha is properly handled by the
	// rendering pipeline. Depth test is disabled so the ghosted geometry
	// renders on top of the opaque solid mesh.
	dimmed_instance = memnew(MeshInstance3D);
	{
		Ref<StandardMaterial3D> dmat;
		dmat.instantiate();
		dmat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
		dmat->set_flag(BaseMaterial3D::FLAG_DISABLE_DEPTH_TEST, true);
		dmat->set_cull_mode(BaseMaterial3D::CULL_DISABLED);
		dmat->set_albedo(Color(1.0f, 1.0f, 1.0f, 0.75f));
		dimmed_instance->set_material_override(dmat);
		print_line("[nanite-dimmed] StandardMaterial3D with TRANSPARENCY_ALPHA, depth_test=disabled, white alpha=0.75");
	}
	rotation_node->add_child(dimmed_instance);
	dimmed_instance->set_visible(false);

	set_custom_minimum_size(Size2(0, 150) * EDSCALE);

	// Top-right overlay: HBoxContainer with PRESET_FULL_RECT + leading
	// spacer pushes a VBoxContainer (holding the two dropdowns + the
	// conditional LOD adjust row) to the top-right corner.
	HBoxContainer *hb = memnew(HBoxContainer);
	ui_bar = hb;
	add_child(hb);
	hb->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT, Control::PRESET_MODE_MINSIZE, 0);
	hb->add_spacer();

	VBoxContainer *vb = memnew(VBoxContainer);
	hb->add_child(vb);

	// Dropdown 1: Display Mode.
	display_mode_btn = memnew(OptionButton);
	display_mode_btn->set_flat(true);
	display_mode_btn->add_item("Normal", NaniteDebug::NORMAL);
	display_mode_btn->add_item("Normal + Wireframe", NaniteDebug::NORMAL_WIREFRAME);
	display_mode_btn->add_item("Cluster Solid", NaniteDebug::CLUSTER_SOLID);
	display_mode_btn->add_item("Cluster Solid + Wireframe", NaniteDebug::CLUSTER_SOLID_WIREFRAME);
	display_mode_btn->add_item("Wireframe Only", NaniteDebug::WIREFRAME_ONLY);
	display_mode_btn->add_item("Cluster Solid + Partition Border", NaniteDebug::CLUSTER_SOLID_WITH_PARTITION_BORDER);
	display_mode_btn->select(0);
	display_mode_btn->set_custom_minimum_size(Size2(180, 0) * EDSCALE);
	vb->add_child(display_mode_btn);
	display_mode_btn->connect(SceneStringName(item_selected),
			callable_mp(this, &NaniteMeshEditor::_on_display_mode_selected));

	// Dropdown 2: LOD Mode.
	lod_mode_btn = memnew(OptionButton);
	lod_mode_btn->set_flat(true);
	lod_mode_btn->add_item("Nanite (auto) [Stage 1]", NaniteDebug::NANITE_AUTO);
	lod_mode_btn->add_item("Force LOD Level", NaniteDebug::FORCE_LOD_LEVEL);
	lod_mode_btn->select(NaniteDebug::FORCE_LOD_LEVEL);
	lod_mode_btn->set_custom_minimum_size(Size2(180, 0) * EDSCALE);
	vb->add_child(lod_mode_btn);
	lod_mode_btn->connect(SceneStringName(item_selected),
			callable_mp(this, &NaniteMeshEditor::_on_lod_mode_selected));

	// Conditional LOD adjust row: [-] [N] [+], shown only when LOD Mode ==
	// Force LOD Level.
	lod_buttons_row = memnew(HBoxContainer);
	vb->add_child(lod_buttons_row);

	lod_minus_button = memnew(Button);
	lod_minus_button->set_text("-");
	lod_minus_button->set_tooltip_text("Decrease Force LOD Level");
	lod_minus_button->connect(SceneStringName(pressed),
			callable_mp(this, &NaniteMeshEditor::_on_lod_minus_pressed));
	lod_buttons_row->add_child(lod_minus_button);

	lod_value_label = memnew(Label);
	lod_value_label->set_text("0");
	lod_value_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	lod_value_label->set_custom_minimum_size(Size2(24, 0) * EDSCALE);
	lod_buttons_row->add_child(lod_value_label);

	lod_plus_button = memnew(Button);
	lod_plus_button->set_text("+");
	lod_plus_button->set_tooltip_text("Increase Force LOD Level");
	lod_plus_button->connect(SceneStringName(pressed),
			callable_mp(this, &NaniteMeshEditor::_on_lod_plus_pressed));
	lod_buttons_row->add_child(lod_plus_button);

	_refresh_lod_buttons();

	// Stats label overlay in the top-left of the preview area.
	stats_label = memnew(Label);
	add_child(stats_label);
	stats_label->set_anchors_and_offsets_preset(Control::PRESET_TOP_LEFT, Control::PRESET_MODE_MINSIZE, 4);
	stats_label->set_text("");

	rot_x = 0.0f;
	rot_y = 0.0f;

	EditorNode::get_singleton()->register_hdr_viewport(viewport);
}

NaniteMeshEditor::~NaniteMeshEditor() {
}

#endif // TOOLS_ENABLED
