/**************************************************************************/
/*  nanite_serialization.h                                                */
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

#include "core/math/math_defs.h" // real_t
#include "core/variant/variant.h" // PackedByteArray (typedef of Vector<uint8_t>)

#include <cstring> // memcpy

// Shared little-endian byte helpers used by Nanite's POD structs
// (NaniteCluster / NaniteClusterNode) and, later, by PagePacker /
// NaniteMeshResource. The helpers operate on raw 4-byte aligned writes so
// the produced byte streams can be safely mmap'd on little-endian hosts
// (which is what Nanite targets in stage 0).
namespace nanite {

struct ByteWriter {
	PackedByteArray data;

	_FORCE_INLINE_ void write_uint32(uint32_t p_v) {
		data.append((p_v >> 0) & 0xFF);
		data.append((p_v >> 8) & 0xFF);
		data.append((p_v >> 16) & 0xFF);
		data.append((p_v >> 24) & 0xFF);
	}

	// Writes a real_t as a 32-bit IEEE-754 float (precision is intentionally
	// narrowed to float so the on-disk layout is stable regardless of
	// GODOT_REAL_TYPE_IS_DOUBLE).
	_FORCE_INLINE_ void write_real(real_t p_v) {
		float f = static_cast<float>(p_v);
		uint32_t bits;
		memcpy(&bits, &f, sizeof(float));
		write_uint32(bits);
	}

	_FORCE_INLINE_ void pad_to_4() {
		while (data.size() % 4 != 0) {
			data.append(0);
		}
	}
};

// Operates over a borrowed buffer; the caller must keep the underlying
// memory alive while the reader is in use.
struct ByteReader {
	const uint8_t *data = nullptr;
	uint32_t size = 0;
	uint32_t offset = 0;

	ByteReader() = default;
	ByteReader(const uint8_t *p_data, uint32_t p_size) :
			data(p_data), size(p_size) {}

	_FORCE_INLINE_ uint32_t read_uint32() {
		DEV_ASSERT(data && offset + 4 <= size);
		uint32_t v = 0;
		v |= uint32_t(data[offset + 0]) << 0;
		v |= uint32_t(data[offset + 1]) << 8;
		v |= uint32_t(data[offset + 2]) << 16;
		v |= uint32_t(data[offset + 3]) << 24;
		offset += 4;
		return v;
	}

	_FORCE_INLINE_ real_t read_real() {
		uint32_t bits = read_uint32();
		float v;
		memcpy(&v, &bits, sizeof(float));
		return static_cast<real_t>(v);
	}

	_FORCE_INLINE_ void pad_to_4() {
		while (offset % 4 != 0) {
			offset++;
		}
	}
};

} // namespace nanite
