#[compute]

#version 450

#VERSION_DEFINES

// Nanite Stage 1 cull shader — per-cluster parallel culling (Task 1.16.7).
//
// Per spec decision 1 (Stage 1 simplified): each GPU thread processes one
// cluster, performing frustum + backface + HZB occlusion culling. No BVH
// stack-based traversal (deferred to Stage 2+). LOD selection is simplified:
// Stage 1 outputs every cluster that passes all three culling tests, since
// the cluster array has no parent_index field to substitute a coarser LOD.
// The error_threshold is currently ignored (kept in the push constant for
// future use).
//
// Cluster SSBO is treated as a raw uint32 array (17 uints per cluster, 68
// bytes — matches NaniteCluster::get_serialized_size()).
//
// Field layout (uint32 offsets within a cluster):
//    0: vertex_offset          6: error (float)
//    1: vertex_count          7,8,9:   bounds.position.xyz (float)
//    2: triangle_offset       10,11,12:bounds.size.xyz (float)
//    3: triangle_count        13,14,15:cone_axis.xyz (float)
//    4: group_id              16:      cone_cutoff (float)
//    5: material_index

layout(set = 0, binding = 0) readonly buffer ClusterSSBO {
	uint cluster_data[];
};
layout(set = 0, binding = 1) readonly buffer BVHSSBO {
	uint bvh_data[];
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
	mat4 model_matrix; // Task 1.16.6 — per-instance world transform
	ivec2 screen_size;
	float error_threshold;
	uint bvh_node_count;
	uint cluster_count;
	uint _pad;
} params;

layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

const uint CLUSTER_U32_COUNT = 17u; // 68 bytes / 4

// --- Cluster field accessors (raw uint reinterpret) ----------------------
uint cluster_vertex_offset(uint idx)  { return cluster_data[idx * CLUSTER_U32_COUNT + 0u]; }
uint cluster_vertex_count(uint idx)   { return cluster_data[idx * CLUSTER_U32_COUNT + 1u]; }
uint cluster_triangle_offset(uint idx){ return cluster_data[idx * CLUSTER_U32_COUNT + 2u]; }
uint cluster_triangle_count(uint idx) { return cluster_data[idx * CLUSTER_U32_COUNT + 3u]; }
uint cluster_group_id(uint idx)       { return cluster_data[idx * CLUSTER_U32_COUNT + 4u]; }
uint cluster_material_index(uint idx) { return cluster_data[idx * CLUSTER_U32_COUNT + 5u]; }
float cluster_error(uint idx)         { return uintBitsToFloat(cluster_data[idx * CLUSTER_U32_COUNT + 6u]); }
vec3 cluster_bounds_pos(uint idx) {
	return vec3(
		uintBitsToFloat(cluster_data[idx * CLUSTER_U32_COUNT + 7u]),
		uintBitsToFloat(cluster_data[idx * CLUSTER_U32_COUNT + 8u]),
		uintBitsToFloat(cluster_data[idx * CLUSTER_U32_COUNT + 9u])
	);
}
vec3 cluster_bounds_size(uint idx) {
	return vec3(
		uintBitsToFloat(cluster_data[idx * CLUSTER_U32_COUNT + 10u]),
		uintBitsToFloat(cluster_data[idx * CLUSTER_U32_COUNT + 11u]),
		uintBitsToFloat(cluster_data[idx * CLUSTER_U32_COUNT + 12u])
	);
}
vec3 cluster_cone_axis(uint idx) {
	return vec3(
		uintBitsToFloat(cluster_data[idx * CLUSTER_U32_COUNT + 13u]),
		uintBitsToFloat(cluster_data[idx * CLUSTER_U32_COUNT + 14u]),
		uintBitsToFloat(cluster_data[idx * CLUSTER_U32_COUNT + 15u])
	);
}
float cluster_cone_cutoff(uint idx) {
	return uintBitsToFloat(cluster_data[idx * CLUSTER_U32_COUNT + 16u]);
}

// --- Frustum cull: returns true if the AABB is fully outside the
//     camera frustum. Conservative: if any corner is behind the camera
//     (w <= 0), the AABB is kept (may still project to valid screen).
//     p_mvp = projection * view * model.
bool is_aabb_outside_frustum(vec3 aabb_min, vec3 aabb_max, mat4 p_mvp) {
	vec4 corners[8] = vec4[8](
		p_mvp * vec4(aabb_min.x, aabb_min.y, aabb_min.z, 1.0),
		p_mvp * vec4(aabb_max.x, aabb_min.y, aabb_min.z, 1.0),
		p_mvp * vec4(aabb_min.x, aabb_max.y, aabb_min.z, 1.0),
		p_mvp * vec4(aabb_max.x, aabb_max.y, aabb_min.z, 1.0),
		p_mvp * vec4(aabb_min.x, aabb_min.y, aabb_max.z, 1.0),
		p_mvp * vec4(aabb_max.x, aabb_min.y, aabb_max.z, 1.0),
		p_mvp * vec4(aabb_min.x, aabb_max.y, aabb_max.z, 1.0),
		p_mvp * vec4(aabb_max.x, aabb_max.y, aabb_max.z, 1.0)
	);

	bool all_left = true, all_right = true;
	bool all_bottom = true, all_top = true;
	bool all_near = true, all_far = true;

	for (int i = 0; i < 8; ++i) {
		vec4 c = corners[i];
		// Corner behind or at the camera plane — conservative: keep AABB.
		if (c.w <= 0.0) {
			return false;
		}
		// Vulkan NDC ranges: x,y in [-1, 1]; z in [0, 1].
		if (c.x >= -c.w) all_left = false;
		if (c.x <=  c.w) all_right = false;
		if (c.y >= -c.w) all_bottom = false;
		if (c.y <=  c.w) all_top = false;
		if (c.z >= 0.0)  all_near = false;
		if (c.z <=  c.w) all_far = false;
	}
	return all_left || all_right || all_bottom || all_top || all_near || all_far;
}

// --- Backface cull: returns true if the cluster's normal cone faces away
//     from the camera. p_view_dir = normalize(cluster_world - camera_world).
//     p_cone_axis_world = normal cone axis in world space.
//     p_cone_cutoff = cos(half-angle), [-1, 1]. Degenerate cones (cutoff
//     <= -1.0) signal "full sphere" — never cull.
bool is_cluster_backface(vec3 p_cone_axis_world, float p_cone_cutoff, vec3 p_view_dir) {
	if (p_cone_cutoff <= -0.999) {
		return false; // Degenerate cone — full sphere, never cull.
	}
	// If view_dir is inside the normal cone (dot > cutoff), the camera is
	// looking along the average normal direction = at the cluster's back.
	return dot(p_cone_axis_world, p_view_dir) > p_cone_cutoff;
}

// --- HZB occlusion cull: project the AABB to screen-space, sample the HZB
//     at the projected center, compare the cluster's nearest depth against
//     the HZB's stored max depth. Returns true if the cluster is fully
//     occluded. Conservative: returns false if any corner is behind the
//     camera (can't reliably project) or the projection is too small.
bool is_cluster_occluded(vec3 aabb_min, vec3 aabb_max, mat4 p_mvp) {
	vec4 corners[8] = vec4[8](
		p_mvp * vec4(aabb_min.x, aabb_min.y, aabb_min.z, 1.0),
		p_mvp * vec4(aabb_max.x, aabb_min.y, aabb_min.z, 1.0),
		p_mvp * vec4(aabb_min.x, aabb_max.y, aabb_min.z, 1.0),
		p_mvp * vec4(aabb_max.x, aabb_max.y, aabb_min.z, 1.0),
		p_mvp * vec4(aabb_min.x, aabb_min.y, aabb_max.z, 1.0),
		p_mvp * vec4(aabb_max.x, aabb_min.y, aabb_max.z, 1.0),
		p_mvp * vec4(aabb_min.x, aabb_max.y, aabb_max.z, 1.0),
		p_mvp * vec4(aabb_max.x, aabb_max.y, aabb_max.z, 1.0)
	);

	float nearest_depth = 1.0;
	vec2 screen_min = vec2(1.0);
	vec2 screen_max = vec2(0.0);

	for (int i = 0; i < 8; ++i) {
		vec4 c = corners[i];
		if (c.w <= 0.0) {
			return false; // Conservative: keep.
		}
		vec3 ndc = c.xyz / c.w;
		vec2 uv = ndc.xy * 0.5 + 0.5;
		screen_min = min(screen_min, uv);
		screen_max = max(screen_max, uv);
		nearest_depth = min(nearest_depth, ndc.z);
	}

	screen_min = clamp(screen_min, vec2(0.0), vec2(1.0));
	screen_max = clamp(screen_max, vec2(0.0), vec2(1.0));

	// Projection too small — can't reliably sample HZB, keep.
	if (screen_max.x - screen_min.x < 0.0001 || screen_max.y - screen_min.y < 0.0001) {
		return false;
	}

	// Stage 1 simplification: sample mip 0 (highest detail) at the projected
	// AABB center. A proper implementation would pick a mip level based on
	// the projected AABB size; Stage 1 accepts the false-positive rate.
	vec2 sample_uv = (screen_min + screen_max) * 0.5;
	float hzb_depth = textureLod(hzb_texture, sample_uv, 0.0).r;

	// HZB stores max depth (farthest geometry). If the cluster's nearest
	// depth is greater (farther) than the HZB depth, the cluster is occluded.
	return nearest_depth > hzb_depth;
}

void main() {
	uint tid = gl_GlobalInvocationID.x;
	if (tid >= params.cluster_count) {
		return;
	}

	vec3 bmin = cluster_bounds_pos(tid);
	vec3 bmax = bmin + cluster_bounds_size(tid);
	vec3 cone_axis = cluster_cone_axis(tid);
	float cone_cutoff = cluster_cone_cutoff(tid);

	// MVP = projection * view * model.
	mat4 mvp = camera.projection * camera.view_matrix * params.model_matrix;

	// --- 1) Frustum cull ---
	if (is_aabb_outside_frustum(bmin, bmax, mvp)) {
		return;
	}

	// --- 2) Backface cull ---
	// Transform cone_axis to world space (rigid transform: upper-left 3x3).
	// For non-uniform scaling this would need the inverse-transpose, but
	// Stage 1 assumes rigid/scale-uniform transforms.
	vec3 cone_axis_world = mat3(params.model_matrix) * cone_axis;

	// Camera world position: inverse(view_matrix) * origin.
	mat4 view_inv = inverse(camera.view_matrix);
	vec3 cam_world = (view_inv * vec4(0.0, 0.0, 0.0, 1.0)).xyz;

	// Cluster center in world space.
	vec3 cluster_center_world = (params.model_matrix * vec4((bmin + bmax) * 0.5, 1.0)).xyz;
	vec3 view_dir = normalize(cluster_center_world - cam_world);

	if (is_cluster_backface(cone_axis_world, cone_cutoff, view_dir)) {
		return;
	}

	// --- 3) HZB occlusion cull ---
	if (is_cluster_occluded(bmin, bmax, mvp)) {
		return;
	}

	// --- 4) LOD selection (Stage 1 simplified) ---
	// Stage 1 has no parent_index in the cluster struct, so we can't
	// substitute a coarser LOD. Every cluster that passes the culling tests
	// is emitted. The error_threshold is ignored for now (kept for Stage 2+).

	uint idx = atomicAdd(visible_count, 1);
	visible_clusters[idx] = tid;
}
