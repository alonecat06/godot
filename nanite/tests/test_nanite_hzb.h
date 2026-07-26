/**************************************************************************/
/*  test_nanite_hzb.h                                                     */
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

#include "nanite/gpu/nanite_hzb.h"
#include "tests/test_macros.h"

#include "core/math/math_funcs.h"
#include "core/math/vector3.h"
#include "servers/rendering/rendering_device.h"

#include <cstring>

namespace TestNaniteHZB {

// NOTE: The compute_mip_count tests are pure-CPU and always run. The GPU
// tests below require a Vulkan backend (RenderingDevice::get_singleton() !=
// nullptr); on OpenGL/headless they SKIP rather than FAIL.

TEST_CASE("[Nanite][HZB] compute_mip_count_for_common_resolutions") {
	// mip 0 = full-res base; coarsest mip must reach 1x1.
	// count = floor(log2(max(w,h))) + 1, minimum 1.
	CHECK(NaniteHZB::compute_mip_count(1, 1) == 1);
	CHECK(NaniteHZB::compute_mip_count(2, 2) == 2); // 2x2, 1x1
	CHECK(NaniteHZB::compute_mip_count(4, 4) == 3); // 4, 2, 1
	CHECK(NaniteHZB::compute_mip_count(8, 8) == 4); // 8, 4, 2, 1
	CHECK(NaniteHZB::compute_mip_count(1024, 1024) == 11); // 1024..1

	// Non-square: driven by the larger dimension.
	CHECK(NaniteHZB::compute_mip_count(1920, 1080) == 11); // floor(log2(1920))=10, +1
	CHECK(NaniteHZB::compute_mip_count(1080, 1920) == 11);
	CHECK(NaniteHZB::compute_mip_count(2048, 1) == 12); // 2048..1

	// Degenerate / tiny inputs must not return 0.
	CHECK(NaniteHZB::compute_mip_count(1, 1) >= 1);
	CHECK(NaniteHZB::compute_mip_count(3, 3) == 2); // 3 -> 1 (floor(log2(3))=1, +1=2)
}

TEST_CASE("[Nanite][HZB] init_and_build_smoke_test") {
	RenderingDevice *rd = RenderingDevice::get_singleton();
	if (!rd) {
		// SKIP — no Vulkan backend available.
		return;
	}

	NaniteHZB hzb;
	hzb.init(rd);
	REQUIRE(hzb.is_initialized());

	// 4x4 R32F source depth, copy-compatible with the HZB texture.
	RD::TextureFormat src_tf;
	src_tf.format = RD::DATA_FORMAT_R32_SFLOAT;
	src_tf.width = 4;
	src_tf.height = 4;
	src_tf.usage_bits = RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_CAN_UPDATE_BIT | RD::TEXTURE_USAGE_CAN_COPY_FROM_BIT;
	RID src = rd->texture_create(src_tf, RD::TextureView());
	REQUIRE(src.is_valid());

	PackedByteArray data;
	data.resize(4 * 4 * sizeof(float));
	memset(data.ptrw(), 0, data.size());
	rd->texture_update(src, 0, data);

	hzb.build(rd, src);
	CHECK(hzb.get_hzb_texture().is_valid());
	CHECK(hzb.get_mip_count() == 3); // 4x4 -> mips 0,1,2

	rd->free_rid(src);
	hzb.cleanup(rd);
	CHECK_FALSE(hzb.is_initialized());
}

TEST_CASE("[Nanite][HZB] downsample_takes_max_of_2x2") {
	RenderingDevice *rd = RenderingDevice::get_singleton();
	if (!rd) {
		// SKIP — no Vulkan backend available.
		return;
	}

	// 4x4 source laid out row-major. Each 2x2 block's MAX must land in the
	// corresponding mip-1 texel:
	//   row 0: 0.1 0.2 | 0.5 0.6
	//   row 1: 0.3 0.4 | 0.7 0.8
	//   row 2: 0.9 1.0 | 0.3 0.4
	//   row 3: 0.1 0.2 | 0.5 0.6
	// mip 1 (2x2): [0.4, 0.8, 1.0, 0.6]
	const float src_values[16] = {
		0.1f, 0.2f, 0.5f, 0.6f,
		0.3f, 0.4f, 0.7f, 0.8f,
		0.9f, 1.0f, 0.3f, 0.4f,
		0.1f, 0.2f, 0.5f, 0.6f
	};
	const float expected_mip1[4] = { 0.4f, 0.8f, 1.0f, 0.6f };

	RD::TextureFormat src_tf;
	src_tf.format = RD::DATA_FORMAT_R32_SFLOAT;
	src_tf.width = 4;
	src_tf.height = 4;
	src_tf.usage_bits = RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_CAN_UPDATE_BIT | RD::TEXTURE_USAGE_CAN_COPY_FROM_BIT;
	RID src = rd->texture_create(src_tf, RD::TextureView());
	REQUIRE(src.is_valid());

	PackedByteArray data;
	data.resize(sizeof(src_values));
	memcpy(data.ptrw(), src_values, sizeof(src_values));
	rd->texture_update(src, 0, data);

	NaniteHZB hzb;
	hzb.init(rd);
	REQUIRE(hzb.is_initialized());

	hzb.build(rd, src);
	REQUIRE(hzb.get_hzb_texture().is_valid());
	REQUIRE(hzb.get_mip_count() >= 2);

	// Read mip 1 back via a 2x2 staging texture (texture_get_data reads a
	// flat layer, so copy the mip into its own texture first).
	RD::TextureFormat staging_tf;
	staging_tf.format = RD::DATA_FORMAT_R32_SFLOAT;
	staging_tf.width = 2;
	staging_tf.height = 2;
	staging_tf.usage_bits = RD::TEXTURE_USAGE_CAN_COPY_TO_BIT | RD::TEXTURE_USAGE_CAN_COPY_FROM_BIT;
	RID staging = rd->texture_create(staging_tf, RD::TextureView());
	REQUIRE(staging.is_valid());

	// Ensure the compute pass has finished writing mip 1 before copying.
	rd->barrier();
	Error err = rd->texture_copy(hzb.get_hzb_texture(), staging, Vector3(0, 0, 0), Vector3(0, 0, 0), Vector3(2, 2, 1), 1 /* src_mipmap */, 0 /* dst_mipmap */, 0, 0);
	CHECK(err == OK);

	PackedByteArray result = rd->texture_get_data(staging, 0);
	if (result.size() >= (int)(4 * sizeof(float))) {
		float vals[4];
		memcpy(vals, result.ptr(), sizeof(vals));
		// Tolerate fp rounding from the max() chain.
		for (int i = 0; i < 4; i++) {
			CHECK(Math::abs(vals[i] - expected_mip1[i]) < 0.0001f);
		}
	}

	rd->free_rid(staging);
	rd->free_rid(src);
	hzb.cleanup(rd);
}

} // namespace TestNaniteHZB
