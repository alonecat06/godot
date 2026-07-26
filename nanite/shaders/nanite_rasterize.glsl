#[compute]

#version 450

#VERSION_DEFINES

// Nanite Stage 1 rasterize shader — software rasterization (Task 1.16.8).
//
// Each GPU thread processes one visible cluster (indexed by
// visible_clusters[tid]). For every triangle in the cluster the shader:
//   1. Reads 3 local vertex indices from meshlet_triangles_data (byte offset
//      = cluster.triangle_offset + tri * 3).
//   2. Converts local indices to global vertex indices via
//      meshlet_vertices_data (cluster.vertex_offset + local_idx).
//   3. Reads 3 vertices from vertex_data (raw stride 32 B: pos.xyz +
//      normal.xyz + uv.xy).
//   4. Transforms positions: clip = projection * view * model * pos;
//      ndc = clip.xyz / clip.w; screen = (ndc.xy * 0.5 + 0.5) * screen_size.
//   5. Computes the triangle's screen-space AABB.
//   6. Iterates pixels inside the AABB; for each pixel inside the triangle
//      (barycentric test), interpolates depth and writes the nearest via a
//      non-atomic compare-then-store (Stage 1 simplification — races are
//      acceptable; last writer wins per pixel).
//
// vis_buffer pixel = (cluster_id << 8) | triangle_id (8 bits each, 256
// triangles per cluster max). depth_buffer stores per-pixel NDC z (0..1).
//
// Stage 1 limitation: no atomic depth test (imageAtomicCompSwap is not
// available on R32_SFLOAT). Multiple threads writing the same pixel race;
// the final state is one of the valid triangles (never garbage).

// --- Bindings ---------------------------------------------------------
// binding 0: visible cluster indices (uint32[], written by cull pass)
// binding 1: cluster metadata (uint32[], 17 uints per cluster = 68 B)
// binding 2: vertex pool (raw stride 32 B per vertex: pos + normal + uv)
// binding 3: vis_buffer (r32ui, writeonly) — encoded (cluster<<8 | tri)
// binding 4: depth_buffer (r32f, writeonly) — per-pixel NDC z
// binding 5: camera UBO (view_matrix + projection, mat4 each)
// binding 6: meshlet_vertices (uint32[], global vertex indices)
// binding 7: meshlet_triangles (uint8[] packed into uint32[], local indices)
layout(set = 0, binding = 0) readonly buffer VisibleClustersBuffer {
	uint visible_clusters[];
};
layout(set = 0, binding = 1) readonly buffer ClusterSSBO {
	uint cluster_data[];
};
layout(set = 0, binding = 2) readonly buffer VertexSSBO {
	uint vertex_data_u32[]; // raw bytes reinterpreted as uint32[]
};
layout(set = 0, binding = 3, r32ui) uniform writeonly uimage2D vis_buffer;
layout(set = 0, binding = 4, r32f) uniform image2D depth_buffer; // read+write for compare-then-store
layout(set = 0, binding = 5) uniform CameraUniform {
	mat4 view_matrix;
	mat4 projection;
} camera;
layout(set = 0, binding = 6) readonly buffer MeshletVerticesSSBO {
	uint meshlet_vertices[];
};
layout(set = 0, binding = 7) readonly buffer MeshletTrianglesSSBO {
	uint meshlet_triangles_u32[]; // packed uint8[] as uint32[]
};

layout(push_constant, std430) uniform Params {
	mat4 model_matrix;
	ivec2 screen_size;
	uint visible_count;
	uint _pad;
} params;

layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

const uint CLUSTER_U32_COUNT = 17u;

// --- Cluster field accessors (raw uint reinterpret) -------------------
uint cluster_vertex_offset(uint idx)  { return cluster_data[idx * CLUSTER_U32_COUNT + 0u]; }
uint cluster_vertex_count(uint idx)   { return cluster_data[idx * CLUSTER_U32_COUNT + 1u]; }
uint cluster_triangle_offset(uint idx){ return cluster_data[idx * CLUSTER_U32_COUNT + 2u]; }
uint cluster_triangle_count(uint idx) { return cluster_data[idx * CLUSTER_U32_COUNT + 3u]; }

// --- Vertex decode (raw stride 32 B) ---------------------------------
// vertex_data is a raw byte array with stride 32 B per vertex:
//   float[3] position (12 B) + float[3] normal (12 B) + float[2] uv (8 B)
// We reinterpret it as uint32[] and read individual floats via uintBitsToFloat.
vec3 decode_position(uint vertex_index) {
	uint base = vertex_index * 8u; // 8 uint32s per vertex
	return vec3(
		uintBitsToFloat(vertex_data_u32[base + 0u]),
		uintBitsToFloat(vertex_data_u32[base + 1u]),
		uintBitsToFloat(vertex_data_u32[base + 2u])
	);
}

// --- Meshlet triangle index decode (packed uint8[]) ------------------
// meshlet_triangles_data is a byte array; each triangle is 3 bytes (local
// vertex indices 0..255). We read a single byte from the packed uint32[]
// storage. Little-endian byte order is guaranteed by Vulkan.
uint read_meshlet_triangle_index(uint byte_offset) {
	uint word_idx = byte_offset >> 2u;       // byte_offset / 4
	uint byte_in_word = byte_offset & 3u;     // byte_offset % 4
	uint word = meshlet_triangles_u32[word_idx];
	return (word >> (byte_in_word * 8u)) & 0xFFu;
}

// --- Barycentric coordinates (2D, screen space) ----------------------
// Returns vec3(b0, b1, b2) where b0+b1+b2 = 1. The point is inside the
// triangle iff all three components are >= 0.
vec3 barycentric(vec2 p, vec2 a, vec2 b, vec2 c) {
	vec2 v0 = b - a;
	vec2 v1 = c - a;
	vec2 v2 = p - a;
	float d00 = dot(v0, v0);
	float d01 = dot(v0, v1);
	float d11 = dot(v1, v1);
	float d20 = dot(v2, v0);
	float d21 = dot(v2, v1);
	float denom = d00 * d11 - d01 * d01;
	if (abs(denom) < 1e-8) {
		return vec3(-1.0); // Degenerate triangle.
	}
	float inv = 1.0 / denom;
	float v = (d11 * d20 - d01 * d21) * inv;
	float w = (d00 * d21 - d01 * d20) * inv;
	float u = 1.0 - v - w;
	return vec3(u, v, w);
}

void main() {
	uint tid = gl_GlobalInvocationID.x;
	if (tid >= params.visible_count) {
		return;
	}

	uint cluster_id = visible_clusters[tid];

	uint v_offset = cluster_vertex_offset(cluster_id);
	uint v_count = cluster_vertex_count(cluster_id);
	uint t_offset = cluster_triangle_offset(cluster_id);
	uint t_count = cluster_triangle_count(cluster_id);

	if (v_count == 0u || t_count == 0u) {
		return;
	}

	mat4 mvp = camera.projection * camera.view_matrix * params.model_matrix;

	uint encoded_vis = (cluster_id << 8u);

	for (uint tri = 0u; tri < t_count; ++tri) {
		// Read 3 local vertex indices (0..255) from meshlet_triangles.
		uint li0 = read_meshlet_triangle_index(t_offset + tri * 3u + 0u);
		uint li1 = read_meshlet_triangle_index(t_offset + tri * 3u + 1u);
		uint li2 = read_meshlet_triangle_index(t_offset + tri * 3u + 2u);

		// Bounds-check local indices against cluster vertex count.
		if (li0 >= v_count || li1 >= v_count || li2 >= v_count) {
			continue;
		}

		// Convert local → global vertex index via meshlet_vertices.
		uint gi0 = meshlet_vertices[v_offset + li0];
		uint gi1 = meshlet_vertices[v_offset + li1];
		uint gi2 = meshlet_vertices[v_offset + li2];

		// Decode positions.
		vec3 p0 = decode_position(gi0);
		vec3 p1 = decode_position(gi1);
		vec3 p2 = decode_position(gi2);

		// Transform to clip space.
		vec4 c0 = mvp * vec4(p0, 1.0);
		vec4 c1 = mvp * vec4(p1, 1.0);
		vec4 c2 = mvp * vec4(p2, 1.0);

		// Skip triangles behind the camera (any w <= 0). Conservative:
		// a proper clip-plane split is deferred to Stage 2+.
		if (c0.w <= 0.0 || c1.w <= 0.0 || c2.w <= 0.0) {
			continue;
		}

		// Perspective divide → NDC.
		vec3 ndc0 = c0.xyz / c0.w;
		vec3 ndc1 = c1.xyz / c1.w;
		vec3 ndc2 = c2.xyz / c2.w;

		// NDC → screen (Vulkan: x,y in [-1,1], z in [0,1]).
		vec2 s0 = ndc0.xy * 0.5 + 0.5;
		vec2 s1 = ndc1.xy * 0.5 + 0.5;
		vec2 s2 = ndc2.xy * 0.5 + 0.5;

		vec2 screen0 = s0 * vec2(params.screen_size);
		vec2 screen1 = s1 * vec2(params.screen_size);
		vec2 screen2 = s2 * vec2(params.screen_size);

		// Screen-space AABB.
		vec2 aabb_min = min(screen0, min(screen1, screen2));
		vec2 aabb_max = max(screen0, max(screen1, screen2));

		ivec2 imin = ivec2(floor(aabb_min));
		ivec2 imax = ivec2(ceil(aabb_max));

		// Clamp to screen.
		imin.x = max(imin.x, 0);
		imin.y = max(imin.y, 0);
		imax.x = min(imax.x, params.screen_size.x - 1);
		imax.y = min(imax.y, params.screen_size.y - 1);

		uint tri_vis = encoded_vis | tri;

		for (int py = imin.y; py <= imax.y; ++py) {
			for (int px = imin.x; px <= imax.x; ++px) {
				vec2 pixel = vec2(float(px) + 0.5, float(py) + 0.5);

				vec3 bary = barycentric(pixel, screen0, screen1, screen2);
				if (bary.x < 0.0 || bary.y < 0.0 || bary.z < 0.0) {
					continue;
				}

				// Interpolate NDC z.
				float depth = bary.x * ndc0.z + bary.y * ndc1.z + bary.z * ndc2.z;
				// Clamp to valid depth range.
				depth = clamp(depth, 0.0, 1.0);

				// Stage 1 simplification: non-atomic compare-then-store.
				// Multiple threads may race on the same pixel, but the
				// final state is always one of the valid triangles.
				float old_depth = imageLoad(depth_buffer, ivec2(px, py)).r;
				if (depth < old_depth) {
					imageStore(depth_buffer, ivec2(px, py), vec4(depth, 0.0, 0.0, 1.0));
					imageStore(vis_buffer, ivec2(px, py), uvec4(tri_vis, 0u, 0u, 0u));
				}
			}
		}
	}
}
