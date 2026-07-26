/**************************************************************************/
/*  test_nanite_builder_leaf.h                                             */
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
#include "test_helpers.h"

#include "core/math/math_funcs.h"

namespace TestNaniteBuilderLeaf {

// preprocess_mesh() must dedup the cube's 24 face-duplicated vertices down
// to 8 unique positions (cube corners). build() runs the full pipeline so
// the debug accessors reflect the post-preprocess state.
TEST_CASE("[NaniteBuilder] preprocess_mesh dedups") {
	Ref<ArrayMesh> cube = NaniteTestHelpers::create_cube_mesh();
	REQUIRE(cube.is_valid());
	CHECK(cube->get_surface_count() >= 1);

	Array arrays = cube->surface_get_arrays(0);
	PackedVector3Array original_verts = arrays[Mesh::ARRAY_VERTEX];
	const size_t original_vertex_count = static_cast<size_t>(original_verts.size());
	CHECK(original_vertex_count == 24); // sanity: cube has 24 face-duplicated verts

	Ref<NaniteBuilder> builder;
	builder.instantiate();
	builder->build(cube);

	CHECK(builder->get_debug_vertex_count() < original_vertex_count);
	CHECK(builder->get_debug_vertex_count() == 8); // cube has 8 unique corner positions
}

// A 12-triangle cube must yield at least one leaf cluster. (meshopt may
// produce a single cluster with triangle_count < min_triangles when the
// input doesn't fill out a full meshlet — that's allowed per the spec.)
TEST_CASE("[NaniteBuilder] cube mesh yields >=1 cluster") {
	Ref<ArrayMesh> cube = NaniteTestHelpers::create_cube_mesh();
	Ref<NaniteBuilder> builder;
	builder.instantiate();
	builder->build(cube);

	CHECK(builder->get_debug_cluster_count() >= 1);
}

// A UV-sphere with segments=32 has ~2048 triangles; with the default config
// (max_triangles=128) this should produce >= 8 clusters.
TEST_CASE("[NaniteBuilder] sphere mesh(2000 tri) yields >=8 clusters") {
	Ref<ArrayMesh> sphere = NaniteTestHelpers::create_sphere_mesh(32);
	Ref<NaniteBuilder> builder;
	builder.instantiate();
	builder->build(sphere);

	CHECK(builder->get_debug_cluster_count() >= 8);
}

// Every leaf cluster's AABB must have positive volume on all three axes.
// Uses the sphere mesh — its clusters span non-trivial 3D regions.
TEST_CASE("[NaniteBuilder] cluster bounds have_volume") {
	Ref<ArrayMesh> sphere = NaniteTestHelpers::create_sphere_mesh(32);
	Ref<NaniteBuilder> builder;
	builder.instantiate();
	builder->build(sphere);

	const LocalVector<NaniteCluster> &clusters = builder->get_debug_clusters();
	CHECK(clusters.size() > 0);
	for (size_t i = 0; i < clusters.size(); ++i) {
		INFO("cluster ", i, " bounds size: ", clusters[i].bounds.size);
		CHECK(clusters[i].bounds.has_volume());
	}
}

// meshopt's normal-cone axis isn't guaranteed to be unit length, so the
// builder normalizes it. For sphere clusters (which span a limited solid
// angle of normals), the axis should be a well-defined unit vector.
TEST_CASE("[NaniteBuilder] cone_axis is normalized") {
	Ref<ArrayMesh> sphere = NaniteTestHelpers::create_sphere_mesh(32);
	Ref<NaniteBuilder> builder;
	builder.instantiate();
	builder->build(sphere);

	const LocalVector<NaniteCluster> &clusters = builder->get_debug_clusters();
	CHECK(clusters.size() > 0);
	for (size_t i = 0; i < clusters.size(); ++i) {
		INFO("cluster ", i, " cone_axis: ", clusters[i].cone_axis,
				" length: ", clusters[i].cone_axis.length());
		CHECK(clusters[i].cone_axis.length() > 0.9f);
	}
}

// Per the spec note, only the upper bounds are enforced — the last cluster
// in a run may legitimately have triangle_count < min_triangles.
TEST_CASE("[NaniteBuilder] cluster constraints respected") {
	Ref<ArrayMesh> sphere = NaniteTestHelpers::create_sphere_mesh(32);
	Ref<NaniteBuilder> builder;
	builder.instantiate();
	builder->build(sphere);

	Ref<BuilderConfig> cfg = builder->get_config();
	REQUIRE(cfg.is_valid());
	const uint32_t max_vertices = static_cast<uint32_t>(cfg->max_vertices);
	const uint32_t max_triangles = static_cast<uint32_t>(cfg->max_triangles);

	const LocalVector<NaniteCluster> &clusters = builder->get_debug_clusters();
	CHECK(clusters.size() > 0);
	for (size_t i = 0; i < clusters.size(); ++i) {
		INFO("cluster ", i,
				" vertex_count=", clusters[i].vertex_count,
				" triangle_count=", clusters[i].triangle_count);
		CHECK(clusters[i].vertex_count <= max_vertices);
		CHECK(clusters[i].triangle_count <= max_triangles);
	}
}

// All leaf clusters (LOD 0) have zero simplification error — error only
// becomes non-zero once build_hierarchy() starts collapsing clusters.
TEST_CASE("[NaniteBuilder] cluster error is 0") {
	Ref<ArrayMesh> sphere = NaniteTestHelpers::create_sphere_mesh(32);
	Ref<NaniteBuilder> builder;
	builder.instantiate();
	builder->build(sphere);

	const LocalVector<NaniteCluster> &clusters = builder->get_debug_clusters();
	CHECK(clusters.size() > 0);
	for (size_t i = 0; i < clusters.size(); ++i) {
		if (clusters[i].group_id != 0) {
			continue; // Only L0 leaf clusters have error == 0.
		}
		INFO("cluster ", i, " error: ", clusters[i].error);
		CHECK(Math::is_equal_approx(clusters[i].error, 0.0f));
	}
}

} // namespace TestNaniteBuilderLeaf
