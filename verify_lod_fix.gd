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

func decode_f32(data: PackedByteArray, off: int) -> float:
	var bytes := PackedByteArray()
	bytes.resize(4)
	bytes[0] = data[off + 0]
	bytes[1] = data[off + 1]
	bytes[2] = data[off + 2]
	bytes[3] = data[off + 3]
	return bytes.decode_float(0)

func _init() -> void:
	print("=== LOD Fix Verification ===")
	var res: NaniteMeshResource = load(TRES_PATH)
	if res == null:
		push_error("Failed to load .tres")
		quit(1)
		return

	var clusters_data: PackedByteArray = res.clusters_data
	var cluster_count: int = res.cluster_count
	var cluster_stride: int = 68

	print("Total clusters: ", cluster_count)
	print("cluster_stride: ", cluster_stride)
	print("clusters_data.size(): ", clusters_data.size())

	var lod_stats: Dictionary = {}
	var max_lod: int = -1

	for ci: int in range(cluster_count):
		var off: int = ci * cluster_stride
		var gid: int = decode_u32(clusters_data, off + 16)
		var tri_count: int = decode_u32(clusters_data, off + 12)

		if tri_count == 0:
			continue

		if gid > max_lod:
			max_lod = gid

		if not lod_stats.has(gid):
			lod_stats[gid] = {
				"clusters": 0,
				"triangles": 0,
				"min_x": INF,
				"min_y": INF,
				"min_z": INF,
				"max_x": -INF,
				"max_y": -INF,
				"max_z": -INF,
			}

		var s: Dictionary = lod_stats[gid]
		s["clusters"] += 1
		s["triangles"] += tri_count

		var min_x: float = decode_f32(clusters_data, off + 28)
		var min_y: float = decode_f32(clusters_data, off + 32)
		var min_z: float = decode_f32(clusters_data, off + 36)
		var ext_x: float = decode_f32(clusters_data, off + 40)
		var ext_y: float = decode_f32(clusters_data, off + 44)
		var ext_z: float = decode_f32(clusters_data, off + 48)
		var max_x: float = min_x + ext_x
		var max_y: float = min_y + ext_y
		var max_z: float = min_z + ext_z

		if min_x < s["min_x"]: s["min_x"] = min_x
		if min_y < s["min_y"]: s["min_y"] = min_y
		if min_z < s["min_z"]: s["min_z"] = min_z
		if max_x > s["max_x"]: s["max_x"] = max_x
		if max_y > s["max_y"]: s["max_y"] = max_y
		if max_z > s["max_z"]: s["max_z"] = max_z

	print("")
	print("=== Per-LOD Summary ===")
	print("LOD | Clusters | Triangles | X Range                     | Y Range                     | Z Range")

	var ref_x_range: float = 0.0
	var ref_y_range: float = 0.0
	var ref_z_range: float = 0.0

	for lod: int in range(max_lod + 1):
		if not lod_stats.has(lod):
			print("LOD %d: NO DATA!" % lod)
			continue

		var s: Dictionary = lod_stats[lod]
		var x_range: float = s["max_x"] - s["min_x"]
		var y_range: float = s["max_y"] - s["min_y"]
		var z_range: float = s["max_z"] - s["min_z"]

		if lod == 0:
			ref_x_range = x_range
			ref_y_range = y_range
			ref_z_range = z_range

		var x_pct: float = (x_range / ref_x_range * 100.0) if ref_x_range > 0 else 0.0
		var y_pct: float = (y_range / ref_y_range * 100.0) if ref_y_range > 0 else 0.0
		var z_pct: float = (z_range / ref_z_range * 100.0) if ref_z_range > 0 else 0.0

		print(" %2d  | %8d | %9d | [%7.3f, %7.3f] %.1f%% | [%7.3f, %7.3f] %.1f%% | [%7.3f, %7.3f] %.1f%%" % [
			lod, s["clusters"], s["triangles"],
			s["min_x"], s["max_x"], x_pct,
			s["min_y"], s["max_y"], y_pct,
			s["min_z"], s["max_z"], z_pct,
		])

		if x_pct < 90.0 or y_pct < 90.0 or z_pct < 90.0:
			push_warning("LOD %d coverage < 90%% on at least one axis! (X=%.1f%%, Y=%.1f%%, Z=%.1f%%)" % [lod, x_pct, y_pct, z_pct])

	print("")
	print("=== Verification Complete ===")
	quit(0)