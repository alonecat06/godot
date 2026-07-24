#[compute]

#version 450

#VERSION_DEFINES

// Nanite HZB downsample: takes a source depth texture and writes a half-res
// destination mip where each pixel = MAX of the 2x2 source region.
// Used to build the hierarchical-Z buffer for occlusion culling.
//
// src_depth is the previous (finer) mip of the HZB texture (R32_SFLOAT),
// dst_mip is the next (coarser) mip. Taking MAX of the 2x2 region means a
// coarse sample returns the farthest depth in that screen region, which is
// what the GPU cull shader needs for conservative occlusion testing.

layout(set = 0, binding = 0) uniform sampler2D src_depth;
layout(set = 0, binding = 1, r32f) uniform writeonly image2D dst_mip;

layout(push_constant, std430) uniform Params {
	ivec2 src_size; // size of src_depth (the previous mip)
	int mip_level; // destination mip level (for debugging)
	int _pad;
} params;

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

void main() {
	ivec2 dst_pos = ivec2(gl_GlobalInvocationID.xy);
	ivec2 src_pos = dst_pos * 2;

	if (src_pos.x >= params.src_size.x || src_pos.y >= params.src_size.y) {
		return;
	}

	// Sample 2x2 region from src depth, take MAX (farthest depth).
	float d0 = texelFetch(src_depth, src_pos, 0).r;
	float d1 = texelFetch(src_depth, src_pos + ivec2(1, 0), 0).r;
	float d2 = texelFetch(src_depth, src_pos + ivec2(0, 1), 0).r;
	float d3 = texelFetch(src_depth, src_pos + ivec2(1, 1), 0).r;

	float max_depth = max(max(d0, d1), max(d2, d3));
	imageStore(dst_mip, dst_pos, vec4(max_depth, 0.0, 0.0, 1.0));
}
