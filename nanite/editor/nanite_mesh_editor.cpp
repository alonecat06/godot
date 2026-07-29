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

// Build a partition border wireframe for the next LOD level.
// For each partition (group of ~4 clusters that will merge into 1 at the next
// LOD level), draws edges where both vertices are shared between clusters
// (locked/border vertices). These edges show the partition boundaries that
// will be locked during simplification to guarantee crack-free LOD transitions.
// Returns a PRIMITIVE_LINES ArrayMesh, or empty Ref on failure.
Ref<ArrayMesh> build_partition_border_wire(const NaniteMeshResource &p_resource,
		int p_current_lod_level) {
	const int next_lod = p_current_lod_level + 1;
	const int max_lod = p_resource.get_max_lod_level();

	// No next level to show partitions for.
	if (next_lod > max_lod) {
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

	// 1) Collect clusters at next_lod and build their triangle index lists.
	struct ClusterInfo {
		uint32_t cluster_idx; // global cluster index in the resource
		LocalVector<unsigned int> tri_indices; // global vertex indices for all triangles
	};
	LocalVector<ClusterInfo> next_lod_clusters;

	for (int ci = 0; ci < cluster_count; ++ci) {
		NaniteCluster c = NaniteCluster::deserialize(clusters_data, (uint32_t)(ci * cluster_stride));
		if ((int)c.group_id != next_lod) {
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
			next_lod_clusters.push_back(info);
		}
	}

	const size_t nl_cluster_count = next_lod_clusters.size();
	if (nl_cluster_count <= 1) {
		// No partitions to show (single cluster or none).
		return Ref<ArrayMesh>();
	}

	// 2) Build cluster_indices and cluster_index_counts for meshopt_partitionClusters.
	LocalVector<unsigned int> cluster_indices;
	LocalVector<unsigned int> cluster_index_counts;
	cluster_index_counts.resize(nl_cluster_count);
	for (size_t i = 0; i < nl_cluster_count; ++i) {
		const ClusterInfo &info = next_lod_clusters[i];
		cluster_index_counts[i] = (unsigned int)info.tri_indices.size();
		for (unsigned int idx : info.tri_indices) {
			cluster_indices.push_back(idx);
		}
	}

	// 3) Partition clusters into groups of ~4 spatially adjacent clusters.
	LocalVector<unsigned int> partition_ids;
	partition_ids.resize(nl_cluster_count);
	size_t partition_count = meshopt_partitionClusters(
			partition_ids.ptr(),
			cluster_indices.ptr(),
			cluster_indices.size(),
			cluster_index_counts.ptr(),
			nl_cluster_count,
			vp_base,
			total_vertex_count,
			kVertexStride,
			4); // target = 4 clusters per partition

	if (partition_count == 0 || partition_count == nl_cluster_count) {
		// No meaningful partitions (all singletons or partition failed).
		return Ref<ArrayMesh>();
	}

	// 4) Group clusters by partition_id.
	LocalVector<LocalVector<size_t>> partitions;
	partitions.resize(partition_count);
	for (size_t i = 0; i < nl_cluster_count; ++i) {
		uint32_t pid = partition_ids[i];
		if (pid < partition_count) {
			partitions[pid].push_back(i);
		}
	}

	// 5) For each partition, compute locked vertices and border edges.
	PackedVector3Array line_verts;

	for (size_t pid = 0; pid < partition_count; ++pid) {
		const LocalVector<size_t> &p_clusters = partitions[pid];
		if (p_clusters.size() <= 1) {
			continue;
		}

		// 5a) Count vertex references across clusters within this partition.
		HashMap<unsigned int, uint32_t> vertex_ref_count;
		for (size_t ci : p_clusters) {
			const ClusterInfo &info = next_lod_clusters[ci];
			HashSet<unsigned int> cluster_verts;
			for (unsigned int v : info.tri_indices) {
				cluster_verts.insert(v);
			}
			for (unsigned int v : cluster_verts) {
				HashMap<unsigned int, uint32_t>::Iterator it = vertex_ref_count.find(v);
				if (it != vertex_ref_count.end()) {
					it->value += 1;
				} else {
					vertex_ref_count[v] = 1;
				}
			}
		}

		// 5b) Build locked vertex set (vertices appearing in >=2 clusters).
		HashSet<unsigned int> locked_verts;
		for (const KeyValue<unsigned int, uint32_t> &E : vertex_ref_count) {
			if (E.value >= 2) {
				locked_verts.insert(E.key);
			}
		}
		if (locked_verts.size() < 2) {
			continue;
		}

		// 5c) Find border edges (edges where both vertices are locked).
		// Use a hash set to deduplicate edges (same edge may appear in
		// multiple triangles).
		HashSet<uint64_t> edge_set;
		for (size_t ci : p_clusters) {
			const ClusterInfo &info = next_lod_clusters[ci];
			for (size_t t = 0; t + 2 < info.tri_indices.size(); t += 3) {
				unsigned int v0 = info.tri_indices[t];
				unsigned int v1 = info.tri_indices[t + 1];
				unsigned int v2 = info.tri_indices[t + 2];

				// Helper to emit a line segment for edge (a,b) if both are locked.
				auto emit_edge = [&](unsigned int a, unsigned int b) {
					if (locked_verts.has(a) && locked_verts.has(b)) {
						uint64_t key = (uint64_t)(a < b ? a : b) << 32 | (uint64_t)(a < b ? b : a);
						if (!edge_set.has(key)) {
							edge_set.insert(key);
							const float *pa = vp_base + a * (kVertexStride / sizeof(float));
							const float *pb = vp_base + b * (kVertexStride / sizeof(float));
							line_verts.push_back(Vector3(pa[0], pa[1], pa[2]));
							line_verts.push_back(Vector3(pb[0], pb[1], pb[2]));
						}
					}
				};
				emit_edge(v0, v1);
				emit_edge(v1, v2);
				emit_edge(v2, v0);
			}
		}
	}

	if (line_verts.is_empty()) {
		return Ref<ArrayMesh>();
	}

	print_line(vformat("[nanite-partition-border] next_lod=%d clusters=%d partitions=%d border_edges=%d",
			next_lod, (uint64_t)nl_cluster_count, (uint64_t)partition_count, line_verts.size() / 2));

	Array arrays;
	arrays.resize(Mesh::ARRAY_MAX);
	arrays[Mesh::ARRAY_VERTEX] = line_verts;
	Ref<ArrayMesh> mesh;
	mesh.instantiate();
	mesh->add_surface_from_arrays(Mesh::PRIMITIVE_LINES, arrays);
	return mesh;
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
	(void)p_index;
	_rebuild_preview();
}

void NaniteMeshEditor::_on_lod_mode_selected(int p_index) {
	if (lod_mode_updating) {
		return; // Prevent recursive signal from programmatic select().
	}
	int mode = lod_mode_btn->get_item_id(p_index);
	if (mode == NaniteDebug::NANITE_AUTO) {
		// Stage 0 has no Nanite GPU cull/LOD pipeline; warn and fall back.
		WARN_PRINT("Nanite auto cull + LOD selection is a Stage 1 feature. "
				   "Falling back to Force LOD Level 0.");
		lod_mode_updating = true;
		lod_mode_btn->select(NaniteDebug::FORCE_LOD_LEVEL);
		lod_mode_updating = false;
		force_lod_spinner->set_visible(true);
		force_lod_spinner->set_value(0);
	} else {
		force_lod_spinner->set_visible(true);
	}
	_rebuild_preview();
}

void NaniteMeshEditor::_on_force_lod_changed(double p_value) {
	(void)p_value;
	_rebuild_preview();
}

void NaniteMeshEditor::gui_input(const Ref<InputEvent> &p_event) {
	ERR_FAIL_COND(p_event.is_null());

	Ref<InputEventMouseButton> mb = p_event;
	if (mb.is_valid()) {
		if (mb->get_button_index() == MouseButton::LEFT) {
			// When the click is on the UI bar (HBoxContainer), skip drag
			// handling so that child controls (OptionButton, SpinBox) can
			// process the event. The event is NOT accepted here so it
			// propagates normally to the child controls.
			if (ui_bar && ui_bar->get_global_rect().has_point(mb->get_global_position())) {
				return;
			}
			if (mb->is_pressed()) {
				dragging = true;
				accept_event();
			} else {
				dragging = false;
				accept_event();
			}
		}
		return;
	}

	Ref<InputEventMouseMotion> mm = p_event;
	if (mm.is_valid() && dragging) {
		rot_x -= mm->get_relative().y * 0.01;
		rot_y -= mm->get_relative().x * 0.01;

		rot_x = CLAMP(rot_x, -Math::PI / 2, Math::PI / 2);
		_update_rotation();
		accept_event();
	}
}

void NaniteMeshEditor::_rebuild_preview() {
	if (!current_resource.is_valid()) {
		print_line("[nanite-preview] no current_resource — clearing meshes");
		solid_instance->set_mesh(Ref<Mesh>());
		wire_instance->set_mesh(Ref<Mesh>());
		return;
	}

	const int display_id = display_mode_btn->get_selected_id();
	const int lod_mode = lod_mode_btn->get_selected_id();
	const int force_lod = (int)force_lod_spinner->get_value();

	print_line(vformat("[nanite-preview] display_id=%d lod_mode=%d force_lod=%d", display_id, lod_mode, force_lod));

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
	bool wire_visible = false;
	bool solid_visible = true;

	if (lod_mode == NaniteDebug::FORCE_LOD_LEVEL) {
		// Stage 0: use cluster decode for all display modes.
		const bool per_cluster_colors =
				(display_id == NaniteDebug::CLUSTER_SOLID ||
						display_id == NaniteDebug::CLUSTER_SOLID_WIREFRAME ||
						display_id == NaniteDebug::CLUSTER_SOLID_WITH_PARTITION_BORDER);
		solid_mesh = build_cluster_mesh(*current_resource.ptr(), force_lod, per_cluster_colors);
		wire_visible = (display_id == NaniteDebug::NORMAL_WIREFRAME ||
						display_id == NaniteDebug::CLUSTER_SOLID_WIREFRAME ||
						display_id == NaniteDebug::WIREFRAME_ONLY ||
						display_id == NaniteDebug::CLUSTER_SOLID_WITH_PARTITION_BORDER);
		if (wire_visible) {
			if (display_id == NaniteDebug::CLUSTER_SOLID_WITH_PARTITION_BORDER) {
				wire_mesh = build_partition_border_wire(*current_resource.ptr(), force_lod);
				// Hide wire if partition border generation failed (e.g., at last LOD).
				if (wire_mesh.is_null()) {
					wire_visible = false;
				}
			} else {
				wire_mesh = build_cluster_wire_mesh(*current_resource.ptr(), force_lod);
			}
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

	// Tint the wireframe material yellow for partition border mode to
	// distinguish partition boundaries from regular cluster wireframes.
	{
		Ref<Material> wire_mat = wire_instance->get_material_override();
		StandardMaterial3D *smat = Object::cast_to<StandardMaterial3D>(wire_mat.ptr());
		if (smat) {
			bool is_partition_border = (display_id == NaniteDebug::CLUSTER_SOLID_WITH_PARTITION_BORDER);
			smat->set_albedo(is_partition_border ? Color(1.0f, 0.85f, 0.1f) : Color(1.0f, 1.0f, 1.0f));
		}
	}

	print_line(vformat("[nanite-preview] final: solid_mesh_valid=%d wire_mesh_valid=%d solid_visible=%d wire_visible=%d",
			(int)solid_mesh.is_valid(), (int)wire_mesh.is_valid(), (int)solid_visible, (int)wire_visible));
}

void NaniteMeshEditor::edit(const Ref<NaniteMeshResource> &p_resource) {
	current_resource = p_resource;

	if (current_resource.is_null()) {
		print_line("[nanite-edit] resource is null");
		solid_instance->set_mesh(Ref<Mesh>());
		wire_instance->set_mesh(Ref<Mesh>());
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

	// Refresh the Force LOD SpinBox range from the resource's max LOD.
	const int max_lod = current_resource->get_max_lod_level();
	force_lod_spinner->set_min(0);
	force_lod_spinner->set_max(MAX(max_lod, 0));
	if ((int)force_lod_spinner->get_value() > max_lod) {
		force_lod_spinner->set_value(max_lod);
	}

	// Default to Force LOD 0 if this is a fresh load.
	if (!force_lod_spinner->is_visible()) {
		force_lod_spinner->set_visible(true);
		force_lod_spinner->set_value(0);
	}

	// Build stats label text: cluster/node/page counts + shadow triangle
	// count + estimated memory (sum of all blob sizes in MB) + max LOD level.
	const int cluster_count = current_resource->get_cluster_count();
	const int node_count = current_resource->get_node_count();
	const int page_count = current_resource->get_page_count();

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
	stats_label->set_text(stats_text);

	// Auto-fit camera distance based on shadow mesh AABB (if present).
	if (shadow_mesh.is_valid()) {
		AABB aabb = shadow_mesh->get_aabb();
		if (aabb.size.length() > 0.0) {
			float distance = aabb.size.length() * 1.2f;
			camera->set_position(Vector3(0, 0, distance));
		}
	}

	// Reset rotation to a pleasant default orientation.
	rot_x = Math::deg_to_rad(-15.0f);
	rot_y = Math::deg_to_rad(30.0f);
	_update_rotation();

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

	set_custom_minimum_size(Size2(0, 150) * EDSCALE);

	// Overlay HBoxContainer at the bottom holding the two dropdowns +
	// SpinBox + spacer. Mirrors the layout in the design doc Section 9.5.
	HBoxContainer *hb = memnew(HBoxContainer);
	ui_bar = hb;
	add_child(hb);
	hb->set_anchors_and_offsets_preset(Control::PRESET_BOTTOM_WIDE, Control::PRESET_MODE_MINSIZE, 2);

	// List 1: Display Mode.
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
	hb->add_child(display_mode_btn);
	display_mode_btn->connect("item_selected",
			callable_mp(this, &NaniteMeshEditor::_on_display_mode_selected));

	// List 2: LOD Mode.
	lod_mode_btn = memnew(OptionButton);
	lod_mode_btn->set_flat(true);
	lod_mode_btn->add_item("Nanite (auto cull + LOD)  [Stage 1]", NaniteDebug::NANITE_AUTO);
	lod_mode_btn->add_item("Force LOD Level", NaniteDebug::FORCE_LOD_LEVEL);
	lod_mode_btn->select(NaniteDebug::FORCE_LOD_LEVEL);
	lod_mode_btn->set_custom_minimum_size(Size2(220, 0) * EDSCALE);
	hb->add_child(lod_mode_btn);
	lod_mode_btn->connect("item_selected",
			callable_mp(this, &NaniteMeshEditor::_on_lod_mode_selected));

	// List 2 child: Force LOD Level SpinBox.
	force_lod_spinner = memnew(SpinBox);
	force_lod_spinner->set_min(0);
	force_lod_spinner->set_max(0);
	force_lod_spinner->set_value(0);
	force_lod_spinner->set_visible(true);
	force_lod_spinner->set_custom_minimum_size(Size2(80, 0) * EDSCALE);
	hb->add_child(force_lod_spinner);
	force_lod_spinner->connect("value_changed",
			callable_mp(this, &NaniteMeshEditor::_on_force_lod_changed));

	hb->add_spacer();

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
