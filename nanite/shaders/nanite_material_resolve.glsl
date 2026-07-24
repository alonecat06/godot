#[compute]

#version 450

#VERSION_DEFINES

// Nanite material-resolve shader (Task 1.11).
//
// Resolves the visibility buffer produced by the rasterize pass into a
// shaded color image by decoding (cluster_id << 8 | triangle_id) per pixel
// and producing a debug-mode-specific (Stage 1) or placeholder-white (default)
// color.
//
// Stage 1 limitation: no real material data is uploaded to the GPU yet
// (NaniteMeshResource::get_materials() is not exposed, and the cluster_ssbo
// material index field is not consumed). The default NONE branch writes a
// flat gray placeholder color. All 7 debug modes are wired up so the bridge
// / editor can flip modes at runtime even before real BRDF shading lands.

layout(set = 0, binding = 0, r32ui) uniform readonly uimage2D vis_buffer;
layout(set = 0, binding = 1, rgba8) uniform writeonly image2D color_buffer;
layout(set = 0, binding = 2) readonly buffer ClusterSSBO {
	vec4 cluster_data[]; // packed NaniteCluster array (material lookup not yet wired)
};

layout(push_constant, std430) uniform Params {
	ivec2 screen_size;
	uint debug_mode; // NaniteDebug::DebugMode (0 = NONE, 1 = CLUSTER_SOLID_COLOR, ...)
	uint _pad;
} params;

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

// Hash function for cluster id -> color (used by CLUSTER_SOLID_COLOR and the
// OVERDRAW_HEATMAP Stage 1 fallback). Distributes colors broadly so adjacent
// cluster ids are visually distinguishable.
vec3 hash_color(uint n) {
	float r = float((n * 1664525u) & 255u) / 255.0;
	float g = float((n * 22695477u) & 255u) / 255.0;
	float b = float((n * 34127u) & 255u) / 255.0;
	return vec3(r, g, b);
}

// Heat color for LOD_SOLID_COLOR (depth 0 = blue, depth 7 = red). The
// rasterize Stage 1 shader writes triangle_id == 0 into vis_buffer, so this
// branch mostly yields blue until real LOD depth is plumbed through.
vec3 heat_color(uint depth) {
	float t = float(depth & 7u) / 7.0;
	return mix(vec3(0.0, 0.0, 1.0), vec3(1.0, 0.0, 0.0), t);
}

void main() {
	ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
	if (pos.x >= params.screen_size.x || pos.y >= params.screen_size.y) {
		return;
	}

	uint encoded = imageLoad(vis_buffer, pos).r;
	if (encoded == 0u) {
		// Empty pixel (no Nanite geometry). Don't write to color_buffer
		// so we don't overwrite engine-rendered geometry behind us.
		return;
	}

	uint cluster_id = encoded >> 8;
	uint triangle_id = encoded & 0xFFu;

	vec3 out_color;
	switch (params.debug_mode) {
		case 0u: { // NONE: default shading (Stage 1: flat gray placeholder).
			out_color = vec3(0.8, 0.8, 0.8);
		} break;
		case 1u: { // CLUSTER_SOLID_COLOR: hash cluster_id to color.
			out_color = hash_color(cluster_id);
		} break;
		case 2u: { // LOD_SOLID_COLOR: heat color by triangle_id (proxy for depth).
			out_color = heat_color(triangle_id);
		} break;
		case 3u: { // OVERDRAW_HEATMAP: visualize cluster density.
			// Stage 1 simplification: same as CLUSTER_SOLID_COLOR (no
			// per-pixel overdraw counter exists yet).
			out_color = hash_color(cluster_id);
		} break;
		case 4u: { // PAGE_RESIDENCY: all resident in Stage 1, green.
			out_color = vec3(0.0, 1.0, 0.0);
		} break;
		case 5u: { // HZB_MIP_LEVELS: not implemented in Stage 1, magenta.
			out_color = vec3(1.0, 0.0, 1.0);
		} break;
		case 6u: { // HZB_OCCLUSION: not implemented in Stage 1, yellow.
			out_color = vec3(1.0, 1.0, 0.0);
		} break;
		default: {
			out_color = vec3(0.5);
		} break;
	}

	imageStore(color_buffer, pos, vec4(out_color, 1.0));
}
