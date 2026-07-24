#[compute]

#version 450

#VERSION_DEFINES

// Nanite Stage 1 cull shader: BVH traversal + frustum/backface/HZB occlusion
// culling + LOD selection. Outputs the list of visible cluster indices to
// visible_clusters_buffer, with visible_count holding the running total.
//
// Stage 1 simplified pass-through: every cluster is appended to the visible
// list unconditionally. The full traversal/cull logic is stubbed behind TODOs
// so the pipeline structure can be exercised end-to-end while the real cull
// is filled in incrementally.
//
// NOTE on push constants: view_matrix + projection (128 bytes for two mat4s)
// would exceed RenderingDevice::MAX_PUSH_CONSTANT_SIZE (128 bytes) once the
// scalar params are added. They are therefore passed via a uniform buffer
// (binding 5, CameraUniform) instead of the push constant block. The push
// constant only carries the small per-dispatch scalars.
//
// SSBO layout (must match the serialized blobs uploaded by NaniteMeshData):
//
//   NaniteCluster (serialized, 4-byte aligned — see core/nanite_cluster.h):
//     uint   vertex_offset;
//     uint   vertex_count;
//     uint   triangle_offset;
//     uint   triangle_count;
//     uint   group_id;            // LOD level (L0 = 0)
//     float  error;               // 0 for leaves
//     AABB   bounds;              // position.xyz + size.xyz (6 x float)
//     vec3   cone_axis;           // unit normal-cone axis (3 x float)
//     float  cone_cutoff;         // cos(half-angle), [-1, 1]
//     (page_id is runtime-only and NOT serialized)
//
//   NaniteClusterNode (serialized, 4-byte aligned — see core/nanite_bvh.h):
//     AABB   bounds;              // 6 x float
//     float  error;
//     uint   left_child;          // UINT32_MAX for leaf
//     uint   right_child;         // UINT32_MAX for leaf
//     uint   first_cluster;       // index into clusters array (leaves only)
//     uint   cluster_count;       // clusters under this node (leaves only)
//     uint   depth;               // root = 0
//
// For the simplified pass-through the blobs are treated as raw vec4 arrays;
// the struct offsets above are only needed once real culling is added.

layout(set = 0, binding = 0) readonly buffer ClusterSSBO {
	vec4 cluster_data[]; // packed NaniteCluster array
};
layout(set = 0, binding = 1) readonly buffer BVHSSBO {
	vec4 bvh_data[]; // packed NaniteClusterNode array
};
layout(set = 0, binding = 2) uniform sampler2D hzb_texture;
layout(set = 0, binding = 3) writeonly buffer VisibleClustersBuffer {
	uint visible_clusters[];
};
layout(set = 0, binding = 4) buffer VisibleCountBuffer {
	uint visible_count;
};
layout(set = 0, binding = 5) uniform CameraUniform {
	mat4 view_matrix;
	mat4 projection;
} camera;

layout(push_constant, std430) uniform Params {
	ivec2 screen_size;
	float error_threshold;
	uint bvh_node_count;
	uint cluster_count;
	uint _pad;
} params;

layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

// TODO Stage 2+: implement BVH traversal starting from the root node
// (index 0). For each visited node:
//   1. Transform bounds to clip space with camera.projection * camera.view_matrix.
//   2. Frustum-cull: skip the node if fully outside the view frustum.
//   3. Backface-cull: skip if the normal cone (cone_axis/cone_cutoff) faces away.
//   4. HZB occlusion-cull: project bounds to screen, sample hzb_texture at the
//      coarsest covered mip; skip if the node is behind the stored max depth.
//   5. LOD: if node.error <= params.error_threshold, descend into leaves and
//      emit each first_cluster..first_cluster+cluster_count cluster;
//      otherwise descend into children (left_child / right_child).
// The simplified version below skips all of the above and just emits every
// cluster, so the rest of the pipeline (rasterize -> HZB rebuild) is testable.

void main() {
	uint tid = gl_GlobalInvocationID.x;
	if (tid >= params.cluster_count) {
		return;
	}

	// Simplified Stage 1: mark every cluster as visible (no culling yet).
	uint idx = atomicAdd(visible_count, 1);
	visible_clusters[idx] = tid;
}
