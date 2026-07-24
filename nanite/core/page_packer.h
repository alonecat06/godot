/**************************************************************************/
/*  page_packer.h                                                         */
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

#include "core/templates/local_vector.h"
#include "core/variant/variant.h" // PackedByteArray

#include "builder_config.h"
#include "nanite_cluster.h"
#include "nanite_serialization.h" // nanite::ByteWriter / ByteReader

#include <cstdint>
#include <cstddef>

// PageTable records the result of PagePacker::pack(): one PageRange per
// page (index into the sorted cluster array) plus the sort permutation
// that maps original cluster indices to their sorted positions.
//
// Like the other Nanite POD structs (NaniteCluster / NaniteClusterNode),
// PageTable is NOT registered to ClassDB; it lives as a value member
// inside NaniteMeshResource. The serialized form is little-endian and
// 4-byte aligned (see nanite_serialization.h).
//
// Serialization layout:
//   uint32 page_count                       // == pages.size()
//   uint32 cluster_count                    // == sorted_cluster_indices.size()
//   PageRange[page_count]                   // 2 × uint32 each
//   uint32[cluster_count]                   // sorted_cluster_indices
struct PageTable {
	struct PageRange {
		uint32_t start_cluster = 0; // index into the sorted cluster array
		uint32_t cluster_count = 0;
	};

	LocalVector<PageRange> pages; // index = page_id, value = cluster range
	LocalVector<uint32_t> sorted_cluster_indices; // permutation: original → sorted position

	_FORCE_INLINE_ size_t get_page_count() const { return pages.size(); }

	PackedByteArray serialize() const;
	static PageTable deserialize(const PackedByteArray &p_data, uint32_t p_offset = 0);
	static size_t get_serialized_size_static(size_t p_page_count, size_t p_cluster_count);
};

// PagePacker sorts clusters by (group_id, morton_code(bounds.center)) and
// then slices the sorted order into pages of approximately page_size_bytes.
// It writes the resulting page_id back into each NaniteCluster in place
// and returns the corresponding PageTable.
//
// The class is a pure static utility — NOT registered to ClassDB, never
// instantiated. Callers must keep the input LocalVector<NaniteCluster>
// alive for the lifetime of the returned PageTable's
// sorted_cluster_indices (they share no ownership, but the permutation
// references original cluster indices).
class PagePacker {
public:
	// Sort clusters by (group_id, morton_code(bounds.center)) then slice by
	// page_size_bytes. Modifies clusters[i].page_id in place. Returns the
	// PageTable describing the resulting page layout.
	//   - A single cluster larger than page_size_bytes is allowed to occupy
	//     its own page (does not fail).
	//   - Each page's accumulated cluster byte size is <= page_size_bytes,
	//     except for the single-cluster-overflow case which is bounded by
	//     page_size_bytes * 2 per the spec ("每个 page 实际字节数
	//     <= cfg.page_size_bytes * 2").
	static PageTable pack(LocalVector<NaniteCluster> &p_clusters, const BuilderConfig &p_cfg);

	// Spread a 10-bit integer's bits to every 3rd bit position
	// (e.g. 0b1101 -> 0b001000000001). Used by morton_encode_3d.
	static uint32_t split_by_3(uint32_t a);

	// 3D Morton code from three 10-bit integer axes. Returns a 30-bit
	// Z-order curve value, preserving spatial locality: clusters that are
	// close in (x, y, z) tend to be close in Morton order.
	static uint32_t morton_encode_3d(uint32_t x, uint32_t y, uint32_t z);
};
