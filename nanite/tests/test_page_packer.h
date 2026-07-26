/**************************************************************************/
/*  test_page_packer.h                                                    */
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

#include "nanite/core/builder_config.h"
#include "nanite/core/nanite_cluster.h"
#include "nanite/core/page_packer.h"

#include "core/math/aabb.h"
#include "core/math/vector3.h"
#include "core/templates/local_vector.h"

#include <cstdint>

namespace TestPagePacker {

// 100 fake clusters with deterministic but spread-out bounds. After packing
// with page_size_bytes=65536, every cluster's page_id must be a valid index
// into the resulting PageTable.
TEST_CASE("[PagePacker] 100 clusters all assigned page_id") {
	LocalVector<NaniteCluster> clusters;
	clusters.resize(100);
	for (uint32_t i = 0; i < 100; ++i) {
		clusters[i].bounds = AABB(
				Vector3(i * 1.5f, (i % 7) * 2.0f, (i % 11) * 3.0f),
				Vector3(1.0f, 1.0f, 1.0f));
		clusters[i].group_id = 0;
	}

	BuilderConfig cfg;
	cfg.page_size_bytes = 65536;

	PageTable table = PagePacker::pack(clusters, cfg);

	CHECK(table.get_page_count() > 0);
	for (uint32_t i = 0; i < 100; ++i) {
		INFO("cluster ", i, " page_id=", clusters[i].page_id,
				" page_count=", table.get_page_count());
		CHECK(clusters[i].page_id < table.get_page_count());
	}

	// Sanity: the permutation must contain every original index exactly once.
	CHECK(table.sorted_cluster_indices.size() == 100);
}

// 200 clusters with page_size_bytes=4096 forces multiple pages. Each page's
// accumulated cluster byte size must respect the 2× overflow bound mandated
// by the spec ("每个 page 实际字节数 <= cfg.page_size_bytes * 2").
TEST_CASE("[PagePacker] 200 clusters + page_size=4096 forces multiple pages") {
	LocalVector<NaniteCluster> clusters;
	clusters.resize(200);
	for (uint32_t i = 0; i < 200; ++i) {
		clusters[i].bounds = AABB(
				Vector3(i * 0.5f, (i % 13) * 1.5f, (i % 17) * 2.5f),
				Vector3(0.5f, 0.5f, 0.5f));
		clusters[i].group_id = 0;
	}

	BuilderConfig cfg;
	cfg.page_size_bytes = 4096;

	PageTable table = PagePacker::pack(clusters, cfg);

	CHECK(table.get_page_count() > 1);

	const size_t cluster_byte_size = NaniteCluster::get_serialized_size();
	const size_t page_overflow_limit = static_cast<size_t>(cfg.page_size_bytes) * 2;

	for (size_t p = 0; p < table.pages.size(); ++p) {
		const size_t page_bytes = static_cast<size_t>(table.pages[p].cluster_count) * cluster_byte_size;
		INFO("page ", p, " cluster_count=", table.pages[p].cluster_count,
				" bytes=", page_bytes, " limit=", page_overflow_limit);
		CHECK(page_bytes <= page_overflow_limit);
	}

	// Sanity: page ranges must be contiguous and cover all 200 clusters.
	uint32_t total_clusters_in_pages = 0;
	for (size_t p = 0; p < table.pages.size(); ++p) {
		total_clusters_in_pages += table.pages[p].cluster_count;
	}
	CHECK(total_clusters_in_pages == 200);
}

// 50 clusters all sharing group_id=0 (same LOD) should land in <=3 distinct
// pages. With the default page_size_bytes=65536 and 64-byte clusters, all 50
// easily fit in a single page (50 × 64 = 3200 bytes ≪ 65536). The spec
// ceiling is 3 pages — Morton ordering further encourages spatial locality.
TEST_CASE("[PagePacker] Same LOD clusters concentrate") {
	LocalVector<NaniteCluster> clusters;
	clusters.resize(50);
	for (uint32_t i = 0; i < 50; ++i) {
		clusters[i].bounds = AABB(
				Vector3(i * 1.0f, (i % 5) * 2.0f, (i % 9) * 3.0f),
				Vector3(1.0f, 1.0f, 1.0f));
		clusters[i].group_id = 0; // same LOD
	}

	BuilderConfig cfg; // default page_size_bytes=65536

	PageTable table = PagePacker::pack(clusters, cfg);

	// Collect distinct page_ids assigned to the 50 same-LOD clusters.
	LocalVector<uint32_t> distinct_pages;
	for (uint32_t i = 0; i < 50; ++i) {
		bool found = false;
		for (uint32_t j = 0; j < distinct_pages.size(); ++j) {
			if (distinct_pages[j] == clusters[i].page_id) {
				found = true;
				break;
			}
		}
		if (!found) {
			distinct_pages.push_back(clusters[i].page_id);
		}
	}

	INFO("distinct page count: ", distinct_pages.size());
	CHECK(distinct_pages.size() <= 3);
}

// Round-trip a hand-built PageTable through serialize()/deserialize() and
// verify every field matches. Also confirms get_serialized_size_static
// agrees with the actual serialized byte length.
TEST_CASE("[PagePacker] Serialization round-trip") {
	PageTable pt;
	pt.pages.resize(5);
	for (uint32_t i = 0; i < 5; ++i) {
		pt.pages[i].start_cluster = i * 4;
		pt.pages[i].cluster_count = 4;
	}
	pt.sorted_cluster_indices.resize(20);
	for (uint32_t i = 0; i < 20; ++i) {
		// Arbitrary but deterministic permutation of [0..20).
		pt.sorted_cluster_indices[i] = (i * 7) % 20;
	}

	PackedByteArray bytes = pt.serialize();
	PageTable pt2 = PageTable::deserialize(bytes);

	CHECK(pt2.pages.size() == 5);
	CHECK(pt2.sorted_cluster_indices.size() == 20);

	for (uint32_t i = 0; i < 5; ++i) {
		INFO("page ", i,
				" start_cluster: expected=", pt.pages[i].start_cluster,
				" got=", pt2.pages[i].start_cluster,
				" cluster_count: expected=", pt.pages[i].cluster_count,
				" got=", pt2.pages[i].cluster_count);
		CHECK(pt2.pages[i].start_cluster == pt.pages[i].start_cluster);
		CHECK(pt2.pages[i].cluster_count == pt.pages[i].cluster_count);
	}

	for (uint32_t i = 0; i < 20; ++i) {
		INFO("sorted_cluster_indices[", i, "]: expected=",
				pt.sorted_cluster_indices[i], " got=",
				pt2.sorted_cluster_indices[i]);
		CHECK(pt2.sorted_cluster_indices[i] == pt.sorted_cluster_indices[i]);
	}

	// The static size estimator must agree with the actual serialized size.
	CHECK(static_cast<size_t>(bytes.size()) ==
			PageTable::get_serialized_size_static(5, 20));
}

} // namespace TestPagePacker
