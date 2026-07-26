/**************************************************************************/
/*  nanite_cluster.cpp                                                    */
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

#include "nanite_cluster.h"

#include "nanite_serialization.h"

PackedByteArray NaniteCluster::serialize() const {
	nanite::ByteWriter w;

	w.write_uint32(vertex_offset);
	w.write_uint32(vertex_count);
	w.write_uint32(triangle_offset);
	w.write_uint32(triangle_count);
	w.write_uint32(group_id);
	w.write_uint32(material_index); // Task 1.16.1
	w.write_real(error);

	// AABB: position.xyz then size.xyz
	w.write_real(bounds.position.x);
	w.write_real(bounds.position.y);
	w.write_real(bounds.position.z);
	w.write_real(bounds.size.x);
	w.write_real(bounds.size.y);
	w.write_real(bounds.size.z);

	// Normal cone
	w.write_real(cone_axis.x);
	w.write_real(cone_axis.y);
	w.write_real(cone_axis.z);
	w.write_real(cone_cutoff);

	// page_id is intentionally NOT written (per spec 0.6.2).
	w.pad_to_4();
	return w.data;
}

NaniteCluster NaniteCluster::deserialize(const PackedByteArray &p_data, uint32_t p_offset) {
	NaniteCluster c;
	nanite::ByteReader r(p_data.ptr(), p_data.size());
	r.offset = p_offset;

	c.vertex_offset = r.read_uint32();
	c.vertex_count = r.read_uint32();
	c.triangle_offset = r.read_uint32();
	c.triangle_count = r.read_uint32();
	c.group_id = r.read_uint32();
	c.material_index = r.read_uint32(); // Task 1.16.1
	c.error = r.read_real();

	c.bounds.position.x = r.read_real();
	c.bounds.position.y = r.read_real();
	c.bounds.position.z = r.read_real();
	c.bounds.size.x = r.read_real();
	c.bounds.size.y = r.read_real();
	c.bounds.size.z = r.read_real();

	c.cone_axis.x = r.read_real();
	c.cone_axis.y = r.read_real();
	c.cone_axis.z = r.read_real();
	c.cone_cutoff = r.read_real();

	r.pad_to_4();
	// page_id left at default 0 — runtime-only.
	return c;
}

size_t NaniteCluster::get_serialized_size() {
	// 6 × uint32 (vertex_offset, vertex_count, triangle_offset, triangle_count,
	//             group_id, material_index)
	// + 1 × float (error) + 6 × float (AABB) + 3 × float (cone_axis)
	// + 1 × float (cone_cutoff)
	// = 24 + 4 + 24 + 12 + 4 = 68 bytes (Task 1.16.1 bumped from 64 → 68).
	return 68;
}
