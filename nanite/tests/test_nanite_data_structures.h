/**************************************************************************/
/*  test_nanite_data_structures.h                                         */
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

#include "nanite/core/nanite_bvh.h"
#include "nanite/core/nanite_cluster.h"

#include "core/math/math_funcs.h"

namespace TestNaniteDataStructures {

TEST_CASE("[NaniteCluster] Default initialization") {
	NaniteCluster c;
	CHECK(c.vertex_offset == 0);
	CHECK(c.vertex_count == 0);
	CHECK(c.triangle_offset == 0);
	CHECK(c.triangle_count == 0);
	CHECK(c.group_id == 0);
	CHECK(c.error == 0.0f);
	CHECK(c.bounds == AABB());
	CHECK(c.cone_axis == Vector3(0, 0, 0));
	CHECK(c.cone_cutoff == 0.0f);
	CHECK(c.page_id == 0);
}

TEST_CASE("[NaniteClusterNode] Leaf sentinel") {
	NaniteClusterNode n;
	CHECK(n.left_child == UINT32_MAX);
	CHECK(n.right_child == UINT32_MAX);
	CHECK(n.cluster_count == 0);
	CHECK(n.is_leaf() == true);
}

TEST_CASE("[NaniteCluster] Serialization round-trip") {
	NaniteCluster c;
	c.vertex_offset = 100;
	c.vertex_count = 64;
	c.triangle_offset = 200;
	c.triangle_count = 128;
	c.group_id = 3;
	c.error = 1.5f;
	c.bounds = AABB(Vector3(1, 2, 3), Vector3(4, 5, 6));
	c.cone_axis = Vector3(0.5f, 0.6f, 0.7f);
	c.cone_cutoff = 0.8f;
	c.page_id = 42; // Must NOT survive serialization.

	PackedByteArray bytes = c.serialize();
	NaniteCluster d = NaniteCluster::deserialize(bytes);

	CHECK(d.vertex_offset == c.vertex_offset);
	CHECK(d.vertex_count == c.vertex_count);
	CHECK(d.triangle_offset == c.triangle_offset);
	CHECK(d.triangle_count == c.triangle_count);
	CHECK(d.group_id == c.group_id);
	CHECK(Math::is_equal_approx(d.error, c.error));
	CHECK(d.bounds.position == c.bounds.position);
	CHECK(d.bounds.size == c.bounds.size);
	CHECK(d.cone_axis == c.cone_axis);
	CHECK(Math::is_equal_approx(d.cone_cutoff, c.cone_cutoff));
	// page_id is runtime-only — must come back as default 0.
	CHECK(d.page_id == 0);
}

TEST_CASE("[NaniteClusterNode] Serialization round-trip") {
	NaniteClusterNode n;
	n.bounds = AABB(Vector3(-1, -2, -3), Vector3(2, 4, 6));
	n.error = 2.5f;
	n.left_child = 5;
	n.right_child = 7;
	n.first_cluster = 10;
	n.cluster_count = 4;
	n.depth = 2;

	PackedByteArray bytes = n.serialize();
	NaniteClusterNode m = NaniteClusterNode::deserialize(bytes);

	CHECK(m.bounds.position == n.bounds.position);
	CHECK(m.bounds.size == n.bounds.size);
	CHECK(Math::is_equal_approx(m.error, n.error));
	CHECK(m.left_child == n.left_child);
	CHECK(m.right_child == n.right_child);
	CHECK(m.first_cluster == n.first_cluster);
	CHECK(m.cluster_count == n.cluster_count);
	CHECK(m.depth == n.depth);
	CHECK(m.is_leaf() == false);
}

TEST_CASE("[NaniteCluster] Byte alignment") {
	NaniteCluster c;
	PackedByteArray bytes = c.serialize();
	CHECK(bytes.size() % 4 == 0);
	CHECK(bytes.size() == NaniteCluster::get_serialized_size());
}

TEST_CASE("[NaniteCluster] page_id not serialized") {
	NaniteCluster c;
	c.page_id = 999;
	PackedByteArray bytes = c.serialize();
	NaniteCluster d = NaniteCluster::deserialize(bytes);

	// page_id should not appear in the byte stream — comes back as default 0.
	CHECK(d.page_id == 0);
	// Size must remain at the fixed serialized size (no page_id field).
	CHECK(bytes.size() == NaniteCluster::get_serialized_size());
}

} // namespace TestNaniteDataStructures
