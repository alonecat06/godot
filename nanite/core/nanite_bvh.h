/**************************************************************************/
/*  nanite_bvh.h                                                          */
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

#include "core/math/aabb.h"
#include "core/variant/variant.h" // PackedByteArray

#include <cstdint>
#include <cstddef>

// NaniteClusterNode is one node of the linearized BVH built by
// NaniteBuilder::build_bvh(). Internal nodes reference their children by
// index into the same `LocalVector<NaniteClusterNode>` array; leaf nodes
// carry a sentinel (UINT32_MAX) in left_child / right_child and instead
// point at a contiguous range of NaniteClusters via first_cluster +
// cluster_count.
//
// Like NaniteCluster, this is a POD struct: NOT registered to ClassDB.
//
// Serialization layout (little-endian, 4-byte aligned — see spec 0.6.2):
//   AABB   bounds          (6 × float)
//   float  error
//   uint32 left_child
//   uint32 right_child
//   uint32 first_cluster
//   uint32 cluster_count
//   uint32 depth
struct NaniteClusterNode {
	AABB bounds = AABB();
	float error = 0.0f;
	uint32_t left_child = UINT32_MAX; // UINT32_MAX for leaf
	uint32_t right_child = UINT32_MAX; // UINT32_MAX for leaf
	uint32_t first_cluster = 0; // index into clusters array (leaves only)
	uint32_t cluster_count = 0; // number of clusters under this node (leaves only)
	uint32_t depth = 0; // root = 0

	_FORCE_INLINE_ bool is_leaf() const {
		return left_child == UINT32_MAX && right_child == UINT32_MAX;
	}

	PackedByteArray serialize() const;
	static NaniteClusterNode deserialize(const PackedByteArray &p_data, uint32_t p_offset = 0);

	// Returns the fixed serialized size in bytes (always a multiple of 4).
	static size_t get_serialized_size();
};
