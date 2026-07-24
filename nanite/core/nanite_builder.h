/**************************************************************************/
/*  nanite_builder.h                                                       */
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

#include "builder_config.h"
#include "nanite_bvh.h"
#include "nanite_cluster.h"
#include "nanite_resource.h" // NaniteMeshResource full type — needed for Ref<NaniteMeshResource> in build() / finalize_resource().

#include "core/object/ref_counted.h"
#include "core/templates/local_vector.h"
#include "core/variant/variant.h" // PackedInt32Array, PackedVector3Array typedefs
#include "scene/resources/mesh.h" // Ref<ArrayMesh> member requires the full type.
#include "scene/resources/packed_scene.h" // Ref<PackedScene> for build_from_resource().
#include "scene/3d/mesh_instance_3d.h" // MeshInstance3D for build_from_resource().

#include <thirdparty/meshoptimizer/meshoptimizer.h>

// NaniteBuilder is the offline pipeline driver that turns an ArrayMesh into a
// NaniteMeshResource. The full Stage-0 pipeline runs end-to-end:
// preprocess_mesh → build_leaf_clusters → build_hierarchy → build_bvh →
// build_shadow_mesh → finalize_resource. The returned NaniteMeshResource
// carries the encoded vertex pool, serialized cluster/BVH/page blobs, and
// a coarse shadow ArrayMesh suitable for `mesh_set_shadow_mesh` in Stage 1.
//
// The class is registered to ClassDB so GDScript can drive the pipeline:
//   var builder = NaniteBuilder.new()
//   builder.config = BuilderConfig.new()
//   var res = builder.build(mesh)
class NaniteBuilder : public RefCounted {
	GDCLASS(NaniteBuilder, RefCounted);

private:
	// Internal tree representation used by build_hierarchy / build_bvh.
	// Each HierarchyNode is either:
	//   - LEAF: represents a single NaniteCluster (leaf or parent). `cluster_idx`
	//           points into m_clusters. `is_leaf == true`.
	//   - MERGE: represents a partition-merge event from build_hierarchy. Has
	//           `source_node_indices` (the source HierarchyNodes from the
	//           previous level that were grouped into this partition) and
	//           `parent_leaf_node_indices` (LEAF HierarchyNodes for the parent
	//           clusters produced by simplifying+reclustering the partition).
	//           `is_leaf == false`; `cluster_idx` is unused.
	// The tree is built bottom-up by build_hierarchy and walked by
	// build_bvh to produce the flat `m_nodes` array.
	struct HierarchyNode {
		bool is_leaf = true;
		uint32_t cluster_idx = UINT32_MAX; // For LEAF: index into m_clusters.
		LocalVector<uint32_t> source_node_indices; // For MERGE: input HierarchyNodes.
		LocalVector<uint32_t> parent_leaf_node_indices; // For MERGE: parent cluster LEAF HierarchyNodes.
	};

	Ref<BuilderConfig> m_cfg;

	// Preprocessed vertex positions (3 floats per vertex, tightly packed).
	LocalVector<float> m_verts_pos;

	// Preprocessed triangle-list indices (uint32 per index).
	LocalVector<unsigned int> m_indices;

	// Cluster + meshlet storage. The leaf-layer pass fills the initial
	// entries; build_hierarchy appends parent clusters + their meshlets.
	LocalVector<NaniteCluster> m_clusters;
	LocalVector<struct meshopt_Meshlet> m_meshlets; // raw meshopt output, parallel to m_clusters
	LocalVector<unsigned int> m_meshlet_vertices; // global meshlet vertex index pool
	LocalVector<unsigned char> m_meshlet_triangles; // global meshlet triangle data (3 bytes per triangle, with padding to 4)

	// Hierarchy tree (built by build_hierarchy) + linearized BVH (built by
	// build_bvh).
	LocalVector<HierarchyNode> m_hierarchy_tree;
	uint32_t m_root_node_idx = UINT32_MAX;
	LocalVector<NaniteClusterNode> m_nodes;

	// Coarse LOD shadow mesh (built by build_shadow_mesh). Kept as a member
	// so unit tests can inspect it via get_debug_shadow_mesh() and so
	// finalize_resource() can hand it off to NaniteMeshResource.
	Ref<ArrayMesh> m_shadow_mesh;

	// Pipeline stages. Each returns true on success.
	bool preprocess_mesh(const PackedVector3Array &p_vertices, const PackedInt32Array &p_indices);
	bool build_leaf_clusters();
	bool build_hierarchy();
	bool build_bvh();
	bool build_shadow_mesh(); // Task 0.7 — coarse LOD shadow mesh extraction.
	Ref<NaniteMeshResource> finalize_resource(); // Assembles the final NaniteMeshResource from m_clusters / m_nodes / m_meshlet_* / m_shadow_mesh.

	// BVH assembly helpers (Task 0.5.2). Each returns an index into m_nodes.
	uint32_t linearize_bvh_recursive(uint32_t p_hierarchy_node_idx, uint32_t p_depth);
	uint32_t build_binary_chain(const LocalVector<uint32_t> &p_hierarchy_indices, uint32_t p_depth);

protected:
	static void _bind_methods();

public:
	NaniteBuilder();
	explicit NaniteBuilder(Ref<BuilderConfig> p_cfg);

	void set_config(const Ref<BuilderConfig> &p_cfg);
	Ref<BuilderConfig> get_config() const;

	// Drives the full Stage-0 pipeline and returns the assembled
	// NaniteMeshResource (or a null Ref on failure). On success the resource
	// carries every blob needed for runtime streaming; on failure the leaf
	// / hierarchy / BVH state may still be partially populated and is
	// inspectable via the debug accessors below.
	Ref<NaniteMeshResource> build(Ref<ArrayMesh> p_mesh);

	// Task 0.12.1 — Resource conversion entry point. Accepts ArrayMesh /
	// PackedScene (gltf/glb/fbx imported scenes) / MeshInstance3D node,
	// merges all surfaces into a single ArrayMesh (vertex offsets applied to
	// indices), then delegates to build(). Returns null on empty/invalid
	// input. Exposed to GDScript as NaniteBuilder.build_from_resource(res).
	static Ref<NaniteMeshResource> build_from_resource(Ref<Resource> p_resource);

	// Debug accessors — for unit tests only. Not bound to GDScript.
	_FORCE_INLINE_ size_t get_debug_vertex_count() const { return m_verts_pos.size() / 3; }
	_FORCE_INLINE_ size_t get_debug_index_count() const { return m_indices.size(); }
	_FORCE_INLINE_ size_t get_debug_cluster_count() const { return m_clusters.size(); }
	_FORCE_INLINE_ const LocalVector<NaniteCluster> &get_debug_clusters() const { return m_clusters; }
	_FORCE_INLINE_ const LocalVector<float> &get_debug_verts_pos() const { return m_verts_pos; }
	_FORCE_INLINE_ const LocalVector<unsigned int> &get_debug_indices() const { return m_indices; }
	// Meshlet vertex/triangle pools — used by tests that need to drive
	// meshopt_encodeMeshlet / meshopt_decodeMeshlet directly against a
	// real cluster's data.
	_FORCE_INLINE_ const LocalVector<unsigned int> &get_debug_meshlet_vertices() const { return m_meshlet_vertices; }
	_FORCE_INLINE_ const LocalVector<unsigned char> &get_debug_meshlet_triangles() const { return m_meshlet_triangles; }

	// Hierarchy / BVH debug accessors (Task 0.5).
	_FORCE_INLINE_ int get_debug_node_count() const { return static_cast<int>(m_nodes.size()); }
	_FORCE_INLINE_ const NaniteClusterNode &get_debug_node(int p_idx) const { return m_nodes[p_idx]; }
	_FORCE_INLINE_ int get_debug_cluster_count_total() const { return static_cast<int>(m_clusters.size()); }

	// Shadow mesh debug accessor (Task 0.7). For unit tests only — not bound
	// to GDScript. Returns null if build_shadow_mesh() has not yet run or
	// produced no geometry.
	_FORCE_INLINE_ Ref<ArrayMesh> get_debug_shadow_mesh() const { return m_shadow_mesh; }
};
