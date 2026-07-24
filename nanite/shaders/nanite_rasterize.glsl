#[compute]

#version 450

#VERSION_DEFINES

// Nanite Stage 1 rasterize shader: software-rasterize the visible clusters
// (listed in visible_clusters_buffer) into the visibility buffer.
//
// Each vis_buffer pixel stores (cluster_id << 8 | triangle_id); depth_buffer
// stores the per-pixel depth used to seed the HZB rebuild.
//
// Stage 1 simplified placeholder: each invocation claims one pixel and writes
// a sentinel coverage value. Real software rasterization (meshlet decode,
// triangle setup, barycentric depth test, visibility write) is stubbed behind
// a TODO so the pipeline structure is testable now.

layout(set = 0, binding = 0) readonly buffer VisibleClustersBuffer {
	uint visible_clusters[];
};
layout(set = 0, binding = 1) readonly buffer ClusterSSBO {
	vec4 cluster_data[]; // packed NaniteCluster array
};
layout(set = 0, binding = 2) readonly buffer VertexSSBO {
	vec4 vertex_data[]; // packed vertex array
};
layout(set = 0, binding = 3, r32ui) uniform writeonly uimage2D vis_buffer;
layout(set = 0, binding = 4, r32f) uniform writeonly image2D depth_buffer;

layout(push_constant, std430) uniform Params {
	ivec2 screen_size;
	uint visible_count;
	uint _pad;
} params;

layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

void main() {
	uint tid = gl_GlobalInvocationID.x;
	if (tid >= params.visible_count) {
		return;
	}

	uint cluster_idx = visible_clusters[tid];

	// TODO Stage 2+: decode the meshlet (vertex_offset/vertex_count,
	// triangle_offset/triangle_count from cluster_data), transform vertices to
	// clip space, rasterize each triangle into vis_buffer/depth_buffer with a
	// per-pixel depth test (keep nearest). For now write a sentinel coverage
	// value so the downstream HZB rebuild has non-empty depth to consume.
	uint encoded = (cluster_idx << 8) | 0u;

	// Claim one pixel per invocation as a placeholder write.
	ivec2 pos = ivec2(int(tid) % params.screen_size.x, int(tid) / params.screen_size.x);
	if (pos.x >= 0 && pos.x < params.screen_size.x && pos.y >= 0 && pos.y < params.screen_size.y) {
		imageStore(vis_buffer, pos, uvec4(encoded, 0u, 0u, 0u));
		imageStore(depth_buffer, pos, vec4(0.5, 0.0, 0.0, 1.0));
	}
}
