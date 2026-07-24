/**************************************************************************/
/*  page_packer.cpp                                                       */
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

#include "page_packer.h"

#include "core/math/aabb.h"
#include "core/math/vector3.h"

#include <algorithm>

uint32_t PagePacker::split_by_3(uint32_t a) {
	uint32_t x = a & 0x000003FF;
	x = (x | x << 16) & 0x030000FF;
	x = (x | x << 8) & 0x030F00FF;
	x = (x | x << 4) & 0x030C30C3;
	x = (x | x << 2) & 0x09249249;
	return x;
}

uint32_t PagePacker::morton_encode_3d(uint32_t x, uint32_t y, uint32_t z) {
	return split_by_3(x) | (split_by_3(y) << 1) | (split_by_3(z) << 2);
}

PageTable PagePacker::pack(LocalVector<NaniteCluster> &p_clusters, const BuilderConfig &p_cfg) {
	PageTable table;
	const uint32_t cluster_count = static_cast<uint32_t>(p_clusters.size());

	if (cluster_count == 0) {
		return table;
	}

	// Step 1: Compute the global AABB spanning every cluster's bounds.
	// Used to normalize cluster centers into [0, 1023]^3 for Morton encoding.
	AABB global_aabb = p_clusters[0].bounds;
	for (uint32_t i = 1; i < cluster_count; ++i) {
		global_aabb.merge_with(p_clusters[i].bounds);
	}

	// Guard against degenerate AABBs (any axis with zero extent would cause
	// division by zero during normalization). Expand to a thin slab.
	const float min_extent = 1e-6f;
	if (global_aabb.size.x < min_extent) {
		global_aabb.size.x = min_extent;
	}
	if (global_aabb.size.y < min_extent) {
		global_aabb.size.y = min_extent;
	}
	if (global_aabb.size.z < min_extent) {
		global_aabb.size.z = min_extent;
	}

	const Vector3 aabb_min = global_aabb.position;
	const Vector3 aabb_extent = global_aabb.size;

	// Steps 2 + 3: For each cluster, compute its normalized center in
	// [0, 1023]^3 (10-bit per axis) and the corresponding Morton code.
	LocalVector<uint32_t> morton_codes;
	morton_codes.resize(cluster_count);
	for (uint32_t i = 0; i < cluster_count; ++i) {
		Vector3 center = p_clusters[i].bounds.get_center();
		float nx = (center.x - aabb_min.x) / aabb_extent.x;
		float ny = (center.y - aabb_min.y) / aabb_extent.y;
		float nz = (center.z - aabb_min.z) / aabb_extent.z;
		// Clamp to [0, 1] — float rounding can push the upper bound past 1.0.
		nx = CLAMP(nx, 0.0f, 1.0f);
		ny = CLAMP(ny, 0.0f, 1.0f);
		nz = CLAMP(nz, 0.0f, 1.0f);
		uint32_t ix = static_cast<uint32_t>(nx * 1023.0f);
		uint32_t iy = static_cast<uint32_t>(ny * 1023.0f);
		uint32_t iz = static_cast<uint32_t>(nz * 1023.0f);
		morton_codes[i] = morton_encode_3d(ix, iy, iz);
	}

	// Step 4: Build a sort permutation [0..cluster_count) and sort it by
	// (group_id, morton_code). After this, sort_permutation[i] is the
	// original cluster index that should appear at sorted position i.
	LocalVector<uint32_t> sort_permutation;
	sort_permutation.resize(cluster_count);
	for (uint32_t i = 0; i < cluster_count; ++i) {
		sort_permutation[i] = i;
	}
	std::sort(sort_permutation.ptr(), sort_permutation.ptr() + cluster_count,
			[&](uint32_t a, uint32_t b) {
				if (p_clusters[a].group_id != p_clusters[b].group_id) {
					return p_clusters[a].group_id < p_clusters[b].group_id;
				}
				return morton_codes[a] < morton_codes[b];
			});

	// Step 5: Greedy bin-pack. Walk the sorted order, accumulating clusters
	// into the current page. When adding the next cluster would exceed
	// page_size_bytes (and the page is non-empty), close the page and start
	// a new one. A single cluster that by itself exceeds page_size_bytes is
	// allowed to occupy its own page (it's the only cluster in that page).
	const size_t page_size_bytes = static_cast<size_t>(p_cfg.page_size_bytes);
	const size_t cluster_byte_size = NaniteCluster::get_serialized_size();

	uint32_t current_page_start = 0; // index into sort_permutation
	size_t current_page_bytes = 0;
	uint32_t current_page_count = 0;

	for (uint32_t sorted_idx = 0; sorted_idx < cluster_count; ++sorted_idx) {
		// If the current page is non-empty AND adding this cluster would
		// push it past the budget, close it and start a new one.
		if (current_page_count > 0 && current_page_bytes + cluster_byte_size > page_size_bytes) {
			PageTable::PageRange range;
			range.start_cluster = current_page_start;
			range.cluster_count = sorted_idx - current_page_start;
			table.pages.push_back(range);

			current_page_start = sorted_idx;
			current_page_bytes = 0;
			current_page_count = 0;
		}

		current_page_bytes += cluster_byte_size;
		current_page_count += 1;

		// Write page_id back to the original cluster (via permutation).
		// table.pages.size() here is the index of the page we're currently
		// filling — if we just closed one above, it's the new (empty) page.
		const uint32_t original_idx = sort_permutation[sorted_idx];
		p_clusters[original_idx].page_id = static_cast<uint32_t>(table.pages.size());
	}

	// Close the final page if it has any clusters.
	if (current_page_count > 0) {
		PageTable::PageRange range;
		range.start_cluster = current_page_start;
		range.cluster_count = cluster_count - current_page_start;
		table.pages.push_back(range);
	}

	// Step 9: Save the sort permutation so callers can map between original
	// and sorted cluster order.
	table.sorted_cluster_indices = sort_permutation;

	return table;
}

PackedByteArray PageTable::serialize() const {
	nanite::ByteWriter w;

	w.write_uint32(static_cast<uint32_t>(pages.size()));
	w.write_uint32(static_cast<uint32_t>(sorted_cluster_indices.size()));

	for (size_t i = 0; i < pages.size(); ++i) {
		w.write_uint32(pages[i].start_cluster);
		w.write_uint32(pages[i].cluster_count);
	}
	for (size_t i = 0; i < sorted_cluster_indices.size(); ++i) {
		w.write_uint32(sorted_cluster_indices[i]);
	}

	w.pad_to_4();
	return w.data;
}

PageTable PageTable::deserialize(const PackedByteArray &p_data, uint32_t p_offset) {
	PageTable table;
	nanite::ByteReader r(p_data.ptr(), p_data.size());
	r.offset = p_offset;

	const uint32_t page_count = r.read_uint32();
	const uint32_t cluster_count = r.read_uint32();

	table.pages.resize(page_count);
	for (uint32_t i = 0; i < page_count; ++i) {
		table.pages[i].start_cluster = r.read_uint32();
		table.pages[i].cluster_count = r.read_uint32();
	}

	table.sorted_cluster_indices.resize(cluster_count);
	for (uint32_t i = 0; i < cluster_count; ++i) {
		table.sorted_cluster_indices[i] = r.read_uint32();
	}

	r.pad_to_4();
	return table;
}

size_t PageTable::get_serialized_size_static(size_t p_page_count, size_t p_cluster_count) {
	// 2 × uint32 (counts) + 2 × uint32 per page + 1 × uint32 per cluster.
	// All fields are 4-byte aligned so no padding is needed.
	return 8 + p_page_count * 8 + p_cluster_count * 4;
}
