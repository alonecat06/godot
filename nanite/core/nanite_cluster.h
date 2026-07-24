/**************************************************************************/
/*  nanite_cluster.h                                                      */
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
#include "core/math/vector3.h"
#include "core/variant/variant.h" // PackedByteArray

#include <cstdint>
#include <cstddef>

// NaniteCluster is the atomic meshlet unit produced by NaniteBuilder's
// leaf-layer pass (meshopt_buildMeshletsFlex + optimizeMeshletLevel +
// computeMeshletBounds). It is a plain-old-data struct: NOT registered to
// ClassDB, never exposed to GDScript directly. Lifetime is managed inside
// LocalVector<NaniteCluster> buffers owned by NaniteMeshResource.
//
// Serialization layout (little-endian, 4-byte aligned — see spec 0.6.2):
//   uint32 vertex_offset
//   uint32 vertex_count
//   uint32 triangle_offset
//   uint32 triangle_count
//   uint32 group_id
//   float  error
//   AABB   bounds        (6 × float: position.xyz, size.xyz)
//   Vec3   cone_axis     (3 × float)
//   float  cone_cutoff
// page_id is runtime-only and intentionally excluded from serialize().
struct NaniteCluster {
	uint32_t vertex_offset = 0;
	uint32_t vertex_count = 0;
	uint32_t triangle_offset = 0;
	uint32_t triangle_count = 0;
	uint32_t group_id = 0; // LOD level (L0 = 0)
	float error = 0.0f; // 0 for leaves
	AABB bounds = AABB(); // local-space
	Vector3 cone_axis = Vector3(0, 0, 0); // unit normal-cone axis
	float cone_cutoff = 0.0f; // cos(half-angle), range [-1, 1]

	// Runtime-only — assigned by PagePacker, never serialized.
	uint32_t page_id = 0;

	// Serializes all fields except page_id. Returns a freshly-allocated
	// PackedByteArray whose size() == get_serialized_size().
	PackedByteArray serialize() const;

	// Reads a NaniteCluster starting at p_offset bytes into p_data.
	// page_id in the returned struct is left at its default (0).
	static NaniteCluster deserialize(const PackedByteArray &p_data, uint32_t p_offset = 0);

	// Returns the fixed serialized size in bytes (always a multiple of 4).
	static size_t get_serialized_size();
};
