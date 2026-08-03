/**************************************************************************/
/*  nanite_resource.h                                                     */
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

#include "core/io/resource.h"
#include "core/object/ref_counted.h"
#include "core/templates/local_vector.h"
#include "core/variant/variant.h" // PackedByteArray

class ArrayMesh;
class BuilderConfig;

// NaniteMeshResource is the serializable artifact produced by NaniteBuilder.
// It holds the encoded vertex pool, the per-cluster meshlet data, the
// linearized BVH node array, and the page table — all as opaque
// PackedByteArray blobs for cheap Godot-side serialization. A coarse
// ArrayMesh shadow mesh is referenced separately for use by the standard
// shadow rendering path (stage 1 will call mesh_set_shadow_mesh with it).
//
// Two serialization paths are supported:
//   (a) Godot native: ResourceSaver saves all properties as Variant-typed
//       PackedByteArray fields (.tres / .res). No special _get_property /
//       _set_property needed since all fields are typed and bound via
//       GDPROPERTY.
//   (b) Custom .nanite binary format: see save() / load() below.
class NaniteMeshResource : public Resource {
	GDCLASS(NaniteMeshResource, Resource);

private:
	// Encoded data blobs (all little-endian, 4-byte aligned internally).
	// Task 1.16.4 — vertex_data is RAW (not meshopt-compressed), stride 32 bytes
	// per vertex: position.xyz (3f) + normal.xyz (3f) + uv.xy (2f) = 8 floats.
	// Consumed directly by nanite_rasterize.glsl / nanite_material_resolve.glsl.
	PackedByteArray vertex_data; // raw vertex pool (stride 32 B, see Task 1.16.4)
	// Task 1.16.4 — clusters_data is ONLY NaniteCluster::serialize() output
	// concatenated (fixed 68-byte stride). The variable-length meshopt-encoded
	// meshlet geometry previously interleaved here was moved into
	// meshlet_vertices_data + meshlet_triangles_data so the cull shader can
	// index it as a fixed-stride array.
	PackedByteArray clusters_data; // N × NaniteCluster::serialize() (68 B each)
	PackedByteArray nodes_data; // concatenation of NaniteClusterNode::serialize()
	PackedByteArray page_table_data; // PageTable::serialize() output

	// Task 1.16.4 — meshlet vertex index pool (raw uint32 per entry). Each
	// cluster's vertex_offset/vertex_count indexes into this array; each entry
	// is a global index into vertex_data (0..vertex_count-1). 4-byte aligned.
	PackedByteArray meshlet_vertices_data; // raw uint32[] (m_meshlet_vertices)
	// Task 1.16.4 — meshlet triangle micro-index pool (raw uint8 per entry).
	// Each cluster's triangle_offset is a BYTE offset into this array; each
	// triangle consumes 3 bytes (local vertex indices 0..255 into the
	// cluster's own vertex range). 4-byte aligned at the blob level.
	PackedByteArray meshlet_triangles_data; // raw uint8[] (m_meshlet_triangles)

	// Task 1.16.2 — materials blob consumed by nanite_material_resolve.glsl.
	// Layout (per material, 32 bytes = 2 × vec4, std430-friendly):
	//   vec4 base_color (r, g, b, a)
	//   vec4 metallic_roughness_pad (metallic, roughness, 0, 0)
	// Stage 1 simplified: emissive/IBL left to Stage 2+.
	PackedByteArray materials_data;

	// v4 — partition_ids_data: one uint32 per cluster (global cluster index),
	// storing the partition_id assigned during build_hierarchy(). Empty when
	// the resource was built with optimize_size=true or loaded from a v3 file.
	// Used by the editor viewer for accurate partition visualization.
	PackedByteArray partition_ids_data;

	Ref<ArrayMesh> shadow_mesh;
	Ref<BuilderConfig> build_config;

	int cluster_count = 0;
	int node_count = 0;
	int page_count = 0;

protected:
	static void _bind_methods();

public:
	// Setters / getters for all bound properties.
	void set_vertex_data(const PackedByteArray &p_data);
	PackedByteArray get_vertex_data() const;
	void set_clusters_data(const PackedByteArray &p_data);
	PackedByteArray get_clusters_data() const;
	void set_nodes_data(const PackedByteArray &p_data);
	PackedByteArray get_nodes_data() const;
	void set_page_table_data(const PackedByteArray &p_data);
	PackedByteArray get_page_table_data() const;

	// Task 1.16.4 — meshlet vertex index + triangle micro-index pools.
	void set_meshlet_vertices_data(const PackedByteArray &p_data);
	PackedByteArray get_meshlet_vertices_data() const;
	void set_meshlet_triangles_data(const PackedByteArray &p_data);
	PackedByteArray get_meshlet_triangles_data() const;

	// Task 1.16.2 — materials_data blob (see layout comment above).
	void set_materials_data(const PackedByteArray &p_data);
	PackedByteArray get_materials_data() const;

	// v4 — partition IDs blob (uint32 per cluster). Empty when optimize_size
	// was used or loaded from a v3 file.
	void set_partition_ids_data(const PackedByteArray &p_data);
	PackedByteArray get_partition_ids_data() const;
	bool has_partition_ids() const;

	void set_shadow_mesh(const Ref<ArrayMesh> &p_mesh);
	Ref<ArrayMesh> get_shadow_mesh() const;

	void set_build_config(const Ref<BuilderConfig> &p_cfg);
	Ref<BuilderConfig> get_build_config() const;

	void set_cluster_count(int p_count);
	int get_cluster_count() const;
	void set_node_count(int p_count);
	int get_node_count() const;
	void set_page_count(int p_count);
	int get_page_count() const;

	// Stage 0 helper — scans clusters_data (fixed 68-byte stride) and
	// returns the maximum `group_id` field across all clusters, i.e.
	// the coarsest LOD level available. Returns 0 when there are no
	// clusters. Used by the preview UI to size the Force LOD Level
	// SpinBox (range 0..max_lod_level). Not bound to ClassDB since it
	// is editor-only and cheap to recompute on demand.
	int get_max_lod_level() const;

	// .nanite binary format. Magic = "NANM" (4 bytes), Version = 4 (uint32).
	// Layout (v4):
	//   char[4]   magic = "NANM"
	//   uint32    version = 4
	//   uint32    vertex_data_size, then vertex_data bytes   (raw stride 32 B)
	//   uint32    clusters_data_size, then clusters_data bytes (68 B per cluster)
	//   uint32    nodes_data_size, then nodes_data bytes
	//   uint32    page_table_data_size, then page_table_data bytes
	//   uint32    materials_data_size, then materials_data bytes   (v2+)
	//   uint32    meshlet_vertices_data_size, then bytes            (v3+)
	//   uint32    meshlet_triangles_data_size, then bytes           (v3+)
	//   uint32    partition_ids_data_size, then bytes               (v4+)
	//   uint32    cluster_count, node_count, page_count (trailer)
	//   (build_config + shadow_mesh intentionally NOT included in .nanite;
	//    they are Godot-side metadata only — re-build from source mesh if needed)
	// Version history:
	//   v1: original (NaniteCluster 64 B, no materials, meshopt-compressed
	//       vertex_data, meshlet data interleaved into clusters_data).
	//   v2: Task 1.16.1 — NaniteCluster 68 B (added material_index); Task 1.16.2
	//       — added materials_data blob.
	//   v3: Task 1.16.4 — vertex_data now raw (stride 32 B: pos+normal+uv);
	//       clusters_data now metadata-only (fixed 68 B stride); meshlet
	//       geometry moved to meshlet_vertices_data + meshlet_triangles_data.
	//   v4: added partition_ids_data blob (uint32 per cluster). Empty when
	//       built with optimize_size=true. Old v3 files load with empty
	//       partition_ids (viewer falls back to recompute).
	Error save(const String &p_path) const;
	Error load(const String &p_path);
};
