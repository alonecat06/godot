/**************************************************************************/
/*  nanite_bvh.cpp                                                        */
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

#include "nanite_bvh.h"

#include "nanite_serialization.h"

PackedByteArray NaniteClusterNode::serialize() const {
	nanite::ByteWriter w;

	// AABB: position.xyz then size.xyz
	w.write_real(bounds.position.x);
	w.write_real(bounds.position.y);
	w.write_real(bounds.position.z);
	w.write_real(bounds.size.x);
	w.write_real(bounds.size.y);
	w.write_real(bounds.size.z);

	w.write_real(error);
	w.write_uint32(left_child);
	w.write_uint32(right_child);
	w.write_uint32(first_cluster);
	w.write_uint32(cluster_count);
	w.write_uint32(depth);

	w.pad_to_4();
	return w.data;
}

NaniteClusterNode NaniteClusterNode::deserialize(const PackedByteArray &p_data, uint32_t p_offset) {
	NaniteClusterNode n;
	nanite::ByteReader r(p_data.ptr(), p_data.size());
	r.offset = p_offset;

	n.bounds.position.x = r.read_real();
	n.bounds.position.y = r.read_real();
	n.bounds.position.z = r.read_real();
	n.bounds.size.x = r.read_real();
	n.bounds.size.y = r.read_real();
	n.bounds.size.z = r.read_real();

	n.error = r.read_real();
	n.left_child = r.read_uint32();
	n.right_child = r.read_uint32();
	n.first_cluster = r.read_uint32();
	n.cluster_count = r.read_uint32();
	n.depth = r.read_uint32();

	r.pad_to_4();
	return n;
}

size_t NaniteClusterNode::get_serialized_size() {
	// 6 × float (AABB) + 1 × float (error) + 5 × uint32 = 24 + 4 + 20 = 48 bytes.
	return 48;
}
