/**************************************************************************/
/*  test_nanite_builder_hierarchy.h                                        */
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

#include "../core/nanite_builder.h"
#include "test_helpers.h"

#include "core/math/math_funcs.h"
#include "core/templates/local_vector.h"

#include <cstdint>

namespace TestNaniteBuilderHierarchy {

// 8000-triangle sphere (segments=64) must yield a multi-node BVH. The spec
// floor is 3 nodes; in practice the partition + simplification loop produces
// far more (one node per cluster plus internal chain + merge nodes).
TEST_CASE("[NaniteBuilderHierarchy] 8000 tri sphere yields >=3 nodes") {
	Ref<ArrayMesh> sphere = NaniteTestHelpers::create_sphere_mesh(64);
	REQUIRE(sphere.is_valid());
	REQUIRE(sphere->get_surface_count() >= 1);

	Ref<NaniteBuilder> builder;
	builder.instantiate();
	builder->build(sphere);

	CHECK(builder->get_debug_node_count() >= 3);
}

// Root error must dominate every other node's error. The root sits at the
// top of the BVH and its error is computed bottom-up as the max of its
// subtree, so by construction root.error >= nodes[i].error for all i.
TEST_CASE("[NaniteBuilderHierarchy] Root error >= all other errors") {
	Ref<ArrayMesh> sphere = NaniteTestHelpers::create_sphere_mesh(64);
	Ref<NaniteBuilder> builder;
	builder.instantiate();
	builder->build(sphere);

	const int node_count = builder->get_debug_node_count();
	REQUIRE(node_count > 0);

	const float root_error = builder->get_debug_node(0).error;
	for (int i = 1; i < node_count; ++i) {
		INFO("node ", i, " error: ", builder->get_debug_node(i).error,
				" root error: ", root_error);
		CHECK(root_error >= builder->get_debug_node(i).error);
	}
}

// Every internal node's bounds must enclose both its children's bounds.
// build_bvh computes parent.bounds = left.bounds.merge(right.bounds), so this
// is guaranteed by construction; the test pins the invariant. A small
// tolerance absorbs the 1-ULP float rounding that occurs because AABB::merge
// stores `size = max - min`, and `position + size` may differ from `max` by
// 1 ULP — which would make AABB::encloses (an exact comparison) fail.
TEST_CASE("[NaniteBuilderHierarchy] Parent bounds contain children") {
	Ref<ArrayMesh> sphere = NaniteTestHelpers::create_sphere_mesh(64);
	Ref<NaniteBuilder> builder;
	builder.instantiate();
	builder->build(sphere);

	const int node_count = builder->get_debug_node_count();
	REQUIRE(node_count > 0);

	const real_t tolerance = 1e-4f;

	for (int i = 0; i < node_count; ++i) {
		const NaniteClusterNode &n = builder->get_debug_node(i);
		if (n.is_leaf()) {
			continue;
		}

		REQUIRE(n.left_child != UINT32_MAX);
		const NaniteClusterNode &left = builder->get_debug_node(n.left_child);
		const AABB grown_parent_l = n.bounds.grow(tolerance);
		INFO("node ", i, " left child bounds not contained. parent: ", n.bounds,
				" child: ", left.bounds);
		CHECK(grown_parent_l.encloses(left.bounds));

		if (n.right_child != UINT32_MAX) {
			const NaniteClusterNode &right = builder->get_debug_node(n.right_child);
			const AABB grown_parent_r = n.bounds.grow(tolerance);
			INFO("node ", i, " right child bounds not contained. parent: ", n.bounds,
					" child: ", right.bounds);
			CHECK(grown_parent_r.encloses(right.bounds));
		}
	}
}

// The BVH root's bounds must contain the entire source mesh AABB. The root
// bounds is the union of all leaf cluster AABBs (each a meshlet bounding
// sphere converted to an enclosing cube), which collectively cover every
// preprocessed vertex. A small tolerance accounts for float rounding.
TEST_CASE("[NaniteBuilderHierarchy] Root bounds contain mesh AABB") {
	Ref<ArrayMesh> sphere = NaniteTestHelpers::create_sphere_mesh(64);
	Ref<NaniteBuilder> builder;
	builder.instantiate();
	builder->build(sphere);

	REQUIRE(builder->get_debug_node_count() > 0);

	const AABB root_bounds = builder->get_debug_node(0).bounds;
	const AABB mesh_aabb = sphere->get_aabb();

	// Grow root by a small tolerance to absorb float rounding before checking
	// that it fully encloses the source mesh AABB.
	const real_t tolerance = 1e-4f;
	const AABB grown_root = root_bounds.grow(tolerance);

	INFO("root bounds: ", root_bounds, " mesh aabb: ", mesh_aabb);
	CHECK(grown_root.encloses(mesh_aabb));
}

// Simplification must reduce total triangle count: the sum of triangle_count
// across leaf clusters (group_id == 0) must exceed the sum across parent
// clusters (group_id > 0). Each partition merge targets half the source
// triangle count, so even with topology constraints the parent total stays
// below the leaf total.
TEST_CASE("[NaniteBuilderHierarchy] Simplification reduces triangle count") {
	Ref<ArrayMesh> sphere = NaniteTestHelpers::create_sphere_mesh(64);
	Ref<NaniteBuilder> builder;
	builder.instantiate();
	builder->build(sphere);

	const LocalVector<NaniteCluster> &clusters = builder->get_debug_clusters();
	REQUIRE(clusters.size() > 0);

	uint64_t leaf_triangles = 0;
	uint64_t parent_triangles = 0;
	for (size_t i = 0; i < clusters.size(); ++i) {
		if (clusters[i].group_id == 0) {
			leaf_triangles += clusters[i].triangle_count;
		} else {
			parent_triangles += clusters[i].triangle_count;
		}
	}

	INFO("leaf triangles: ", leaf_triangles, " parent triangles: ", parent_triangles);
	REQUIRE(leaf_triangles > 0);
	REQUIRE(parent_triangles > 0);
	CHECK(leaf_triangles > parent_triangles);
}

// Error monotonicity: every internal node's error must be >= the max of its
// children's errors (within a small tolerance). build_hierarchy enforces
// parent_error = MAX(result_error, max_child_error), and build_bvh propagates
// error upward as MAX(left.error, right.error), so this holds by construction.
TEST_CASE("[NaniteBuilderHierarchy] Parent error monotonic") {
	Ref<ArrayMesh> sphere = NaniteTestHelpers::create_sphere_mesh(64);
	Ref<NaniteBuilder> builder;
	builder.instantiate();
	builder->build(sphere);

	const int node_count = builder->get_debug_node_count();
	REQUIRE(node_count > 0);

	const float tolerance = 0.001f;

	for (int i = 0; i < node_count; ++i) {
		const NaniteClusterNode &n = builder->get_debug_node(i);
		if (n.is_leaf()) {
			continue;
		}

		REQUIRE(n.left_child != UINT32_MAX);
		const NaniteClusterNode &left = builder->get_debug_node(n.left_child);
		INFO("node ", i, " error: ", n.error, " < left child error: ", left.error);
		CHECK(n.error >= left.error - tolerance);

		if (n.right_child != UINT32_MAX) {
			const NaniteClusterNode &right = builder->get_debug_node(n.right_child);
			INFO("node ", i, " error: ", n.error, " < right child error: ", right.error);
			CHECK(n.error >= right.error - tolerance);
		}
	}
}

} // namespace TestNaniteBuilderHierarchy
