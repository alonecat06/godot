/**************************************************************************/
/*  nanite_hzb.h                                                          */
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

#include "core/templates/rid.h"
#include "core/templates/local_vector.h"

class RenderingDevice;

// NaniteHZB manages the hierarchical-Z mip pyramid used by the GPU cull
// shader for occlusion culling. Each mip level stores the MAX of its
// 2x2 children, so sampling a coarser mip gives the farthest depth in
// that screen region.
//
// Stage 1: the HZB is rebuilt every frame from the Nanite rasterize
// pass depth output (mip 0) via a chain of 8x8 compute dispatches.
//
// mip 0 holds a copy of the input depth texture (full resolution); each
// subsequent mip i is half the resolution of mip i-1 and stores the per-texel
// maximum of the 2x2 region it covers.
class NaniteHZB {
	// Pure C++ class — no GDCLASS, no Object base.
public:
	// Computes the number of mip levels needed for a texture of the given
	// resolution so the coarsest mip is 1x1. mip 0 is the full-resolution
	// base level, so the count is floor(log2(max(w,h))) + 1 (minimum 1).
	static int compute_mip_count(int p_width, int p_height) {
		int max_dim = MAX(p_width, p_height);
		if (max_dim <= 1) {
			return 1;
		}
		int count = 1;
		while (max_dim > 1) {
			max_dim >>= 1;
			count++;
		}
		return count;
	}

private:
	RID hzb_texture;
	LocalVector<RID> hzb_mip_views; // per-level views for binding
	RID downsample_shader;
	RID downsample_pipeline;
	RID sampler; // nearest, clamp-to-edge sampler for texelFetch

	int mip_count = 0;
	int current_width = 0;
	int current_height = 0;
	bool needs_rebuild = true;
	bool initialized = false;

	// Uniform set cache: one per mip level (reused across frames if size unchanged).
	LocalVector<RID> downsample_uniform_sets;

	// Frees the HZB texture + per-mip views (keeps shader/pipeline/sampler).
	void _free_texture(RenderingDevice *p_rd);

public:
	void init(RenderingDevice *p_rd);
	void cleanup(RenderingDevice *p_rd);
	void resize(RenderingDevice *p_rd, int p_width, int p_height);
	void build(RenderingDevice *p_rd, const RID &p_depth_texture);

	RID get_hzb_texture() const { return hzb_texture; }
	int get_mip_count() const { return mip_count; }
	bool is_initialized() const { return initialized; }
};
