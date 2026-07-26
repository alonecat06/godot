/**************************************************************************/
/*  test_shadow_mesh.h                                                    */
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

#include "core/math/aabb.h"
#include "core/math/math_funcs.h"
#include "core/templates/local_vector.h"
#include "scene/resources/mesh.h"

#include <cstdint>

namespace TestNaniteShadowMesh {

// Shadow mesh triangle count must be strictly less than the original mesh's
// triangle count, but > 0. Per spec 0.7 "粗 LOD Shadow Mesh 生成" scenario
// "三角形数减少": selecting clusters from BVH nodes at depth <=
// shadow_lod_depth yields a coarsened geometry — never the full leaf layer.
// The shadow surface is a single PRIMITIVE_TRIANGLES surface.
TEST_CASE("[NaniteShadowMesh] shadow_triangle_count_less_than_original") {
	Ref<ArrayMesh> sphere = NaniteTestHelpers::create_sphere_mesh(32);
	REQUIRE(sphere.is_valid());
	REQUIRE(sphere->get_surface_count() >= 1);

	Array original_arrays = sphere->surface_get_arrays(0);
	PackedInt32Array original_indices = original_arrays[Mesh::ARRAY_INDEX];
	const int64_t original_tri_count = original_indices.size() / 3;
	REQUIRE(original_tri_count > 0);

	Ref<BuilderConfig> cfg;
	cfg.instantiate();
	NaniteBuilder builder(cfg);
	builder.build(sphere);

	Ref<ArrayMesh> shadow = builder.get_debug_shadow_mesh();
	REQUIRE(shadow.is_valid());
	REQUIRE(shadow->get_surface_count() == 1);

	Array shadow_arrays = shadow->surface_get_arrays(0);
	PackedInt32Array shadow_indices = shadow_arrays[Mesh::ARRAY_INDEX];
	const int64_t shadow_tri_count = shadow_indices.size() / 3;

	INFO("original triangles: ", original_tri_count,
			" shadow triangles: ", shadow_tri_count);
	CHECK(shadow_tri_count > 0);
	CHECK(shadow_tri_count < original_tri_count);
}

// Shadow mesh AABB must contain the original mesh AABB (within a small
// tolerance). Per spec 0.7 scenario "Shadow mesh bounds 包含原 mesh": the
// shadow clusters' bounds (bounding spheres from meshopt_computeMeshletBounds)
// generally over-estimate the actual geometry, but float rounding and LOD
// simplification can shave off tiny margins. Grow the shadow AABB by 0.01
// before checking containment to absorb edge-case rejection.
TEST_CASE("[NaniteShadowMesh] shadow_bounds_contain_original") {
	Ref<ArrayMesh> sphere = NaniteTestHelpers::create_sphere_mesh(32);
	REQUIRE(sphere.is_valid());

	Ref<BuilderConfig> cfg;
	cfg.instantiate();
	NaniteBuilder builder(cfg);
	builder.build(sphere);

	Ref<ArrayMesh> shadow = builder.get_debug_shadow_mesh();
	REQUIRE(shadow.is_valid());

	const AABB shadow_aabb = shadow->get_aabb();
	const AABB sphere_aabb = sphere->get_aabb();
	const AABB grown_shadow = shadow_aabb.grow(0.01);

	INFO("shadow aabb: ", shadow_aabb, " sphere aabb: ", sphere_aabb,
			" grown shadow aabb: ", grown_shadow);
	CHECK(grown_shadow.encloses(sphere_aabb));
}

// The shadow mesh's RID must be valid so it can be passed to
// RenderingServer::mesh_set_shadow_mesh() in Stage 1. We do NOT actually
// call mesh_set_shadow_mesh() here — that requires a live RenderingServer,
// which isn't available in the unit-test harness. We only verify the RID
// itself is valid.
TEST_CASE("[NaniteShadowMesh] rid_creation_works") {
	Ref<ArrayMesh> sphere = NaniteTestHelpers::create_sphere_mesh(32);
	REQUIRE(sphere.is_valid());

	Ref<BuilderConfig> cfg;
	cfg.instantiate();
	NaniteBuilder builder(cfg);
	builder.build(sphere);

	Ref<ArrayMesh> shadow = builder.get_debug_shadow_mesh();
	REQUIRE(shadow.is_valid());
	REQUIRE(shadow->get_surface_count() == 1);

	const RID shadow_rid = shadow->get_rid();
	CHECK(shadow_rid.is_valid());
}

} // namespace TestNaniteShadowMesh
