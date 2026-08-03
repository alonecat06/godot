#!/usr/bin/env godot
extends SceneTree

const TRES_PATH := "res://mesh/sm_Fountain_01_10_Monument_01.nanite.tres"

func decode_u32(data: PackedByteArray, off: int) -> int:
	var v: int = 0
	v |= data[off + 0] as int
	v |= (data[off + 1] as int) << 8
	v |= (data[off + 2] as int) << 16
	v |= (data[off + 3] as int) << 24
	return v

func _init() -> void:
	print("=== LOD Vertex Bounds Analysis ===")
	var res: NaniteMeshResource = load(TRES_PATH)
	if res == null:
		push_error("Failed to load .tres")
		quit(1)
		return

	var clusters_data: PackedByteArray = res.clusters_data
	var vertex_data: PackedByteArray = res.vertex_data
	var mv_data: PackedByteArray = res.meshlet_vertices_data
	var mt_data: PackedByteArray = res.meshlet_triangles_data

	var cluster_count: int = res.cluster_count
	var cluster_stride: int = 68
	var vertex_stride: int = 32
	var total_vertex_count: int = vertex_data.size() / vertex_stride

	var mv: Array = []  # convert to array for easy access
	for i: int in range(0, mv_data.size(), 4):
		mv.append(decode_u32(mv_data, i))

	var vp_base: PackedFloat32Array = vertex_data.to_float32_array()

	print("Total clusters: ", cluster_count)
	print("Total vertices: ", total_vertex_count)

	# For each LOD, compute bounds from actual vertex positions
	var lod_bounds: Dictionary = {}
	var max_lod: int = -1

	for ci: int in range(cluster_count):
		var off: int = ci * cluster_stride
		var gid: int = decode_u32(clusters_data, off + 16)
		var tri_count: int = decode_u32(clusters_data, off + 12)
		var vert_count: int = decode_u32(clusters_data, off + 8)
		var vert_off: int = decode_u32(clusters_data, off + 0)
		var tri_off: int = decode_u32(clusters_data, off + 4)

		if tri_count == 0 or vert_count == 0:
			continue

		if gid > max_lod:
			max_lod = gid

		if not lod_bounds.has(gid):
			lod_bounds[gid] = {
				"min_x": INF, "min_y": INF, "min_z": INF,
				"max_x": -INF, "max_y": -INF, "max_z": -INF,
				"triangles": 0, "clusters": 0,
			}

		var s: Dictionary = lod_bounds[gid]
		s["clusters"] += 1
		s["triangles"] += tri_count

		# Compute bounds from actual vertex positions
		for vi: int in range(vert_count):
			var mv_idx: int = vert_off + vi
			if mv_idx >= mv.size():
				continue
			var global_v: int = mv[mv_idx]
			if global_v >= total_vertex_count:
				continue
			var base: int = global_v * (vertex_stride / 4)  # float offset
			var x: float = vp_base[base]
			var y: float = vp_base[base + 1]
			var z: float = vp_base[base + 2]
			if x < s["min_x"]: s["min_x"] = x
			if y < s["min_y"]: s["min_y"] = y
			if z < s["min_z"]: s["min_z"] = z
			if x > s["max_x"]: s["max_x"] = x
			if y > s["max_y"]: s["max_y"] = y
			if z > s["max_z"]: s["max_z"] = z

	print("")
	print("=== Vertex-based Bounds (from actual positions) ===")
	print("LOD | Clusters | Triangles | X Range                     | Y Range                     | Z Range")

	var ref_x: float = 0.0; var ref_y: float = 0.0; var ref_z: float = 0.0

	for lod: int in range(max_lod + 1):
		if not lod_bounds.has(lod):
			print("LOD %d: NO DATA!" % lod)
			continue

		var s: Dictionary = lod_bounds[lod]
		var xr: float = s["max_x"] - s["min_x"]
		var yr: float = s["max_y"] - s["min_y"]
		var zr: float = s["max_z"] - s["min_z"]

		if lod == 0:
			ref_x = xr; ref_y = yr; ref_z = zr

		var xp: float = (xr / ref_x * 100.0) if ref_x > 0 else 0.0
		var yp: float = (yr / ref_y * 100.0) if ref_y > 0 else 0.0
		var zp: float = (zr / ref_z * 100.0) if ref_z > 0 else 0.0

		print(" %2d  | %8d | %9d | [%7.3f, %7.3f] %.1f%% | [%7.3f, %7.3f] %.1f%% | [%7.3f, %7.3f] %.1f%%" % [
			lod, s["clusters"], s["triangles"],
			s["min_x"], s["max_x"], xp,
			s["min_y"], s["max_y"], yp,
			s["min_z"], s["max_z"], zp,
		])

	print("")
	print("=== Analysis Complete ===")
	quit(0)