/**************************************************************************/
/*  test_nanite_resource.h                                                */
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

#include "tests/test_macros.h"

#include "nanite/core/nanite_builder.h"
#include "nanite/core/nanite_resource.h"
#include "test_helpers.h"

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/math/math_funcs.h"
#include "core/object/object.h"
#include "core/templates/local_vector.h"
#include "scene/resources/mesh.h"

#include <thirdparty/meshoptimizer/meshoptimizer.h>

#include <cstring>

namespace TestNaniteResource {

// ---------------------------------------------------------------------------
// Test 1: meshopt_encodeMeshlet → meshopt_decodeMeshlet round-trip preserves
// vertex positions.
//
// Picks the first leaf cluster from a built sphere, encodes its meshlet
// (vertex indices + triangle micro-indices) with meshopt_encodeMeshlet,
// decodes it back with meshopt_decodeMeshlet, and verifies:
//   - Decoded vertex indices match the originals (i.e. they reference the
//     same global vertices).
//   - Decoded triangle micro-indices match the originals.
//   - The vertex positions looked up via the decoded indices match those
//     looked up via the original indices (is_equal_approx).
// ---------------------------------------------------------------------------
TEST_CASE("[Nanite][Resource] meshlet_encode_decode_roundtrip") {
	Ref<BuilderConfig> cfg;
	cfg.instantiate();
	NaniteBuilder builder(cfg);
	Ref<ArrayMesh> sphere = NaniteTestHelpers::create_sphere_mesh(32);
	REQUIRE(sphere.is_valid());

	Ref<NaniteMeshResource> res = builder.build(sphere);
	REQUIRE(res.is_valid());
	REQUIRE(builder.get_debug_cluster_count() > 0);

	const LocalVector<NaniteCluster> &clusters = builder.get_debug_clusters();
	const LocalVector<unsigned int> &meshlet_vertices = builder.get_debug_meshlet_vertices();
	const LocalVector<unsigned char> &meshlet_triangles = builder.get_debug_meshlet_triangles();
	const LocalVector<float> &verts_pos = builder.get_debug_verts_pos();
	const size_t vertex_count = verts_pos.size() / 3;
	const size_t stride = sizeof(float) * 3;

	// Pick the first cluster that has both vertices and triangles.
	size_t cluster_idx = 0;
	bool found = false;
	for (size_t i = 0; i < clusters.size(); ++i) {
		if (clusters[i].vertex_count > 0 && clusters[i].triangle_count > 0) {
			cluster_idx = i;
			found = true;
			break;
		}
	}
	REQUIRE(found);
	const NaniteCluster &cluster = clusters[cluster_idx];

	const unsigned int *in_vertices = meshlet_vertices.ptr() + cluster.vertex_offset;
	const unsigned char *in_triangles = meshlet_triangles.ptr() + cluster.triangle_offset;

	// Encode.
	const size_t bound = meshopt_encodeMeshletBound(cluster.vertex_count, cluster.triangle_count);
	LocalVector<unsigned char> encoded;
	encoded.resize(bound);
	const size_t encoded_size = meshopt_encodeMeshlet(
			encoded.ptr(), bound,
			in_vertices, cluster.vertex_count,
			in_triangles, cluster.triangle_count);
	REQUIRE(encoded_size > 0);

	// Decode. Per meshopt docs:
	//   - vertices output buffer must hold vertex_count * sizeof(uint32)
	//     (4-byte aligned — uint32 is naturally aligned).
	//   - triangles output buffer must hold align(triangle_count * 3, 4)
	//     bytes when triangle_size == 3 (8-bit indices).
	const size_t dec_v_bytes = cluster.vertex_count * sizeof(unsigned int);
	const size_t dec_t_bytes_raw = cluster.triangle_count * 3;
	const size_t dec_t_bytes = (dec_t_bytes_raw + 3u) & ~3u;

	LocalVector<unsigned int> dec_vertices;
	dec_vertices.resize(dec_v_bytes / sizeof(unsigned int));
	LocalVector<unsigned char> dec_triangles;
	dec_triangles.resize(dec_t_bytes);

	const int dec_err = meshopt_decodeMeshlet(
			dec_vertices.ptr(), cluster.vertex_count,
			dec_triangles.ptr(), cluster.triangle_count,
			encoded.ptr(), encoded_size);
	REQUIRE(dec_err == 0);

	// Verify vertex indices round-trip exactly.
	for (uint32_t i = 0; i < cluster.vertex_count; ++i) {
		INFO("vertex idx ", i, ": original=", in_vertices[i],
				" decoded=", dec_vertices[i]);
		CHECK(dec_vertices[i] == in_vertices[i]);
	}

	// Verify triangle micro-indices round-trip as sets. meshopt's
	// meshopt_encodeMeshlet rotates each triangle's vertex order for vertex
	// cache optimization (this is documented meshopt behavior), so the
	// decoded corner order may be a cyclic rotation of the original. We
	// compare each triangle as a sorted triplet — two triangles that share
	// the same three vertex indices (in any order) represent the same
	// geometric triangle.
	for (uint32_t t = 0; t < cluster.triangle_count; ++t) {
		unsigned char orig_sorted[3] = {
			in_triangles[t * 3 + 0],
			in_triangles[t * 3 + 1],
			in_triangles[t * 3 + 2],
		};
		unsigned char dec_sorted[3] = {
			dec_triangles[t * 3 + 0],
			dec_triangles[t * 3 + 1],
			dec_triangles[t * 3 + 2],
		};
		// Simple insertion sort for the 3-element arrays.
		for (int i = 1; i < 3; ++i) {
			const unsigned char vo = orig_sorted[i];
			int j = i - 1;
			while (j >= 0 && orig_sorted[j] > vo) {
				orig_sorted[j + 1] = orig_sorted[j];
				--j;
			}
			orig_sorted[j + 1] = vo;
		}
		for (int i = 1; i < 3; ++i) {
			const unsigned char vd = dec_sorted[i];
			int j = i - 1;
			while (j >= 0 && dec_sorted[j] > vd) {
				dec_sorted[j + 1] = dec_sorted[j];
				--j;
			}
			dec_sorted[j + 1] = vd;
		}
		INFO("tri ", t, ": original={", orig_sorted[0], ',', orig_sorted[1], ',', orig_sorted[2],
				"} decoded={", dec_sorted[0], ',', dec_sorted[1], ',', dec_sorted[2], "}");
		CHECK(dec_sorted[0] == orig_sorted[0]);
		CHECK(dec_sorted[1] == orig_sorted[1]);
		CHECK(dec_sorted[2] == orig_sorted[2]);
	}

	// Verify positions match via the decoded indices. The decoded indices
	// equal the original indices (verified above), so the positions are
	// looked up from the same m_verts_pos pool — this is a final sanity
	// check that the round-trip preserves vertex references.
	for (uint32_t i = 0; i < cluster.vertex_count; ++i) {
		const unsigned int global_v = dec_vertices[i];
		REQUIRE(global_v < vertex_count);
		const float *pos_dec = verts_pos.ptr() + global_v * 3;
		const float *pos_orig = verts_pos.ptr() + in_vertices[i] * 3;
		CHECK(Math::is_equal_approx(pos_dec[0], pos_orig[0]));
		CHECK(Math::is_equal_approx(pos_dec[1], pos_orig[1]));
		CHECK(Math::is_equal_approx(pos_dec[2], pos_orig[2]));
	}
	(void)stride; // stride kept for documentation; not directly used here.
}

// ---------------------------------------------------------------------------
// Test 2: Godot native resource round-trip via ResourceSaver/ResourceLoader.
//
// Builds a sphere, saves the resulting NaniteMeshResource to a .tres file
// under tmp/, loads it back, and verifies that cluster_count /
// node_count / page_count match.
// ---------------------------------------------------------------------------
TEST_CASE("[Nanite][Resource] tres_roundtrip") {
	Ref<BuilderConfig> cfg;
	cfg.instantiate();
	NaniteBuilder builder(cfg);
	Ref<ArrayMesh> sphere = NaniteTestHelpers::create_sphere_mesh(32);
	REQUIRE(sphere.is_valid());

	Ref<NaniteMeshResource> res = builder.build(sphere);
	REQUIRE(res.is_valid());
	REQUIRE(res->get_cluster_count() > 0);
	REQUIRE(res->get_node_count() > 0);
	REQUIRE(res->get_page_count() > 0);

	// Use a project-relative path under tmp/ rather than user:// so the
	// test runs under sandboxes that restrict writes to AppData (TRAE).
	// The CWD when running the test binary is the project root.
	DirAccess::make_dir_recursive_absolute("tmp");
	const String path = "tmp/test_nanite_res.tres";

	// Wipe any stale file from a prior run so the test starts clean.
	if (FileAccess::exists(path)) {
		DirAccess::remove_absolute(path);
	}

	Error save_err = ResourceSaver::save(res, path);
	REQUIRE(save_err == OK);

	Ref<Resource> loaded_base = ResourceLoader::load(path);
	REQUIRE(loaded_base.is_valid());

	NaniteMeshResource *loaded = Object::cast_to<NaniteMeshResource>(loaded_base.ptr());
	REQUIRE(loaded != nullptr);

	INFO("cluster_count: original=", res->get_cluster_count(),
			" loaded=", loaded->get_cluster_count());
	CHECK(loaded->get_cluster_count() == res->get_cluster_count());

	INFO("node_count: original=", res->get_node_count(),
			" loaded=", loaded->get_node_count());
	CHECK(loaded->get_node_count() == res->get_node_count());

	INFO("page_count: original=", res->get_page_count(),
			" loaded=", loaded->get_page_count());
	CHECK(loaded->get_page_count() == res->get_page_count());

	// Byte-array blobs must also survive the .tres round-trip.
	CHECK(loaded->get_vertex_data().size() == res->get_vertex_data().size());
	CHECK(loaded->get_clusters_data().size() == res->get_clusters_data().size());
	CHECK(loaded->get_nodes_data().size() == res->get_nodes_data().size());
	CHECK(loaded->get_page_table_data().size() == res->get_page_table_data().size());

	// Cleanup.
	DirAccess::remove_absolute(path);
}

// ---------------------------------------------------------------------------
// Test 3: .nanite binary round-trip via NaniteMeshResource::save() / load().
//
// Builds a sphere, saves with res->save("tmp/test_nanite.nanite"), loads
// with loaded->load(...), and verifies that the byte lengths of all four
// data blobs exactly match between res and loaded.
// ---------------------------------------------------------------------------
TEST_CASE("[Nanite][Resource] nanite_binary_roundtrip") {
	Ref<BuilderConfig> cfg;
	cfg.instantiate();
	NaniteBuilder builder(cfg);
	Ref<ArrayMesh> sphere = NaniteTestHelpers::create_sphere_mesh(32);
	REQUIRE(sphere.is_valid());

	Ref<NaniteMeshResource> res = builder.build(sphere);
	REQUIRE(res.is_valid());

	// Use a project-relative path under tmp/ rather than user:// so the
	// test runs under sandboxes that restrict writes to AppData (TRAE).
	DirAccess::make_dir_recursive_absolute("tmp");
	const String path = "tmp/test_nanite.nanite";

	if (FileAccess::exists(path)) {
		DirAccess::remove_absolute(path);
	}

	Error save_err = res->save(path);
	REQUIRE(save_err == OK);

	Ref<NaniteMeshResource> loaded;
	loaded.instantiate();
	REQUIRE(loaded.is_valid());

	Error load_err = loaded->load(path);
	REQUIRE(load_err == OK);

	INFO("vertex_data size: original=", res->get_vertex_data().size(),
			" loaded=", loaded->get_vertex_data().size());
	CHECK(loaded->get_vertex_data().size() == res->get_vertex_data().size());

	INFO("clusters_data size: original=", res->get_clusters_data().size(),
			" loaded=", loaded->get_clusters_data().size());
	CHECK(loaded->get_clusters_data().size() == res->get_clusters_data().size());

	INFO("nodes_data size: original=", res->get_nodes_data().size(),
			" loaded=", loaded->get_nodes_data().size());
	CHECK(loaded->get_nodes_data().size() == res->get_nodes_data().size());

	INFO("page_table_data size: original=", res->get_page_table_data().size(),
			" loaded=", loaded->get_page_table_data().size());
	CHECK(loaded->get_page_table_data().size() == res->get_page_table_data().size());

	// Counts must match the trailer.
	CHECK(loaded->get_cluster_count() == res->get_cluster_count());
	CHECK(loaded->get_node_count() == res->get_node_count());
	CHECK(loaded->get_page_count() == res->get_page_count());

	// Byte-for-byte equality of the blobs (not just length) — the .nanite
	// format is a faithful concatenation, so any byte mismatch would
	// indicate a serialization bug.
	const PackedByteArray &v_orig = res->get_vertex_data();
	const PackedByteArray &v_load = loaded->get_vertex_data();
	if (v_orig.size() == v_load.size() && v_orig.size() > 0) {
		CHECK(memcmp(v_orig.ptr(), v_load.ptr(), v_orig.size()) == 0);
	}
	const PackedByteArray &c_orig = res->get_clusters_data();
	const PackedByteArray &c_load = loaded->get_clusters_data();
	if (c_orig.size() == c_load.size() && c_orig.size() > 0) {
		CHECK(memcmp(c_orig.ptr(), c_load.ptr(), c_orig.size()) == 0);
	}
	const PackedByteArray &n_orig = res->get_nodes_data();
	const PackedByteArray &n_load = loaded->get_nodes_data();
	if (n_orig.size() == n_load.size() && n_orig.size() > 0) {
		CHECK(memcmp(n_orig.ptr(), n_load.ptr(), n_orig.size()) == 0);
	}
	const PackedByteArray &p_orig = res->get_page_table_data();
	const PackedByteArray &p_load = loaded->get_page_table_data();
	if (p_orig.size() == p_load.size() && p_orig.size() > 0) {
		CHECK(memcmp(p_orig.ptr(), p_load.ptr(), p_orig.size()) == 0);
	}

	// Cleanup.
	DirAccess::remove_absolute(path);
}

// ---------------------------------------------------------------------------
// Test 4: .nanite file magic + version are written correctly.
//
// Saves a resource to .nanite, then opens the file with FileAccess::READ and
// verifies the first 4 bytes are 'N', 'A', 'N', 'M' and the next uint32 is
// the format version (1).
// ---------------------------------------------------------------------------
TEST_CASE("[Nanite][Resource] nanite_magic_and_version") {
	Ref<BuilderConfig> cfg;
	cfg.instantiate();
	NaniteBuilder builder(cfg);
	Ref<ArrayMesh> sphere = NaniteTestHelpers::create_sphere_mesh(32);
	REQUIRE(sphere.is_valid());

	Ref<NaniteMeshResource> res = builder.build(sphere);
	REQUIRE(res.is_valid());

	// Use a project-relative path under tmp/ rather than user:// so the
	// test runs under sandboxes that restrict writes to AppData (TRAE).
	DirAccess::make_dir_recursive_absolute("tmp");
	const String path = "tmp/test_nanite_magic.nanite";

	if (FileAccess::exists(path)) {
		DirAccess::remove_absolute(path);
	}

	Error save_err = res->save(path);
	REQUIRE(save_err == OK);

	Error open_err = OK;
	Ref<FileAccess> f = FileAccess::open(path, FileAccess::READ, &open_err);
	REQUIRE(f.is_valid());
	REQUIRE(open_err == OK);

	const uint8_t m0 = f->get_8();
	const uint8_t m1 = f->get_8();
	const uint8_t m2 = f->get_8();
	const uint8_t m3 = f->get_8();

	INFO("magic bytes: ", m0, ' ', m1, ' ', m2, ' ', m3);
	CHECK(m0 == static_cast<uint8_t>('N'));
	CHECK(m1 == static_cast<uint8_t>('A'));
	CHECK(m2 == static_cast<uint8_t>('N'));
	CHECK(m3 == static_cast<uint8_t>('M'));

	// Version (uint32 little-endian).
	const uint32_t v0 = static_cast<uint32_t>(f->get_8());
	const uint32_t v1 = static_cast<uint32_t>(f->get_8());
	const uint32_t v2 = static_cast<uint32_t>(f->get_8());
	const uint32_t v3 = static_cast<uint32_t>(f->get_8());
	const uint32_t version = v0 | (v1 << 8) | (v2 << 16) | (v3 << 24);
	INFO("version: ", version);
	CHECK(version == 1u);

	// Cleanup.
	f.unref();
	DirAccess::remove_absolute(path);
}

} // namespace TestNaniteResource
