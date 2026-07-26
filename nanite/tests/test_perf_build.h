/**************************************************************************/
/*  test_perf_build.h                                                     */
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

#include "core/os/time.h"
#include "scene/resources/mesh.h"

namespace TestNanitePerf {

// Stage 0 performance baseline benchmark (Task 0.10.2).
//
// For each input size (1K, 10K, 50K, 100K triangles), build a large test
// mesh, run NaniteBuilder::build(), measure elapsed ms, and log the
// resulting cluster/node/page counts. Per spec 0.10 "性能基准": there is
// no hard ms upper limit — these numbers serve as baseline data for
// regression comparison in later stages.
//
// Only structural assertions are made (counts > 0, shadow_mesh valid);
// timing is logged via MESSAGE for doctest's output capture.
//
// Note: the 100K case may take several seconds. If partition/simplification
// is broken it could exhaust memory; in that case REQUIRE will fail the
// test case (doctest catches the failure gracefully — it does not crash
// the process unless there is an actual segfault).
TEST_CASE("[Nanite][Perf] build_benchmarks") {
	struct BenchCase {
		String name;
		int target_tris;
	};

	const BenchCase cases[] = {
		{"1K", 1000},
		{"10K", 10000},
		{"50K", 50000},
		{"100K", 100000},
	};

	for (const BenchCase &c : cases) {
		Ref<BuilderConfig> cfg;
		cfg.instantiate();
		NaniteBuilder builder(cfg);

		// Use create_large_test_mesh for predictable triangle counts at scale
		// (sphere segment counts don't map linearly to triangle counts).
		Ref<ArrayMesh> mesh = NaniteTestHelpers::create_large_test_mesh(c.target_tris);
		REQUIRE(mesh.is_valid());
		REQUIRE(mesh->get_surface_count() >= 1);
		// surface_get_array_index_len returns the index count; /3 gives
		// triangles. (surface_get_array_len would return the vertex count,
		// which is ~6x smaller for these grid meshes.)
		const int actual_tris = mesh->surface_get_array_index_len(0) / 3;

		const uint64_t start = Time::get_singleton()->get_ticks_msec();
		Ref<NaniteMeshResource> res = builder.build(mesh);
		const uint64_t elapsed = Time::get_singleton()->get_ticks_msec() - start;

		REQUIRE(res.is_valid());

		// Log baseline data — doctest's MESSAGE macro emits to stdout and
		// appears in the test runner output (visible with --report-level=D
		// or when a CHECK in this case fails).
		MESSAGE(c.name, " tris: input=", actual_tris,
				" cluster_count=", res->get_cluster_count(),
				" node_count=", res->get_node_count(),
				" page_count=", res->get_page_count(),
				" build_ms=", elapsed);

		// Structural sanity (no hard ms limits per spec).
		CHECK(res->get_cluster_count() > 0);
		CHECK(res->get_node_count() > 0);
		CHECK(res->get_page_count() > 0);
		CHECK(res->get_shadow_mesh().is_valid());
	}
}

} // namespace TestNanitePerf
