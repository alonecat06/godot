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
	PackedByteArray vertex_data; // meshopt_encodeVertexBuffer output
	PackedByteArray clusters_data; // concatenation of NaniteCluster::serialize()
	PackedByteArray nodes_data; // concatenation of NaniteClusterNode::serialize()
	PackedByteArray page_table_data; // PageTable::serialize() output

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

	// .nanite binary format. Magic = "NANM" (4 bytes), Version = 1 (uint32).
	// Layout:
	//   char[4]   magic = "NANM"
	//   uint32    version = 1
	//   uint32    vertex_data_size, then vertex_data bytes
	//   uint32    clusters_data_size, then clusters_data bytes
	//   uint32    nodes_data_size, then nodes_data bytes
	//   uint32    page_table_data_size, then page_table_data bytes
	//   uint32    cluster_count, node_count, page_count (trailer)
	//   (build_config + shadow_mesh intentionally NOT included in .nanite;
	//    they are Godot-side metadata only — re-build from source mesh if needed)
	Error save(const String &p_path) const;
	Error load(const String &p_path);
};
