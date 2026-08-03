#!/usr/bin/env godot
extends SceneTree

const TRES_PATH := "E:/Code/01_Study/GodotTest/nanite-test/mesh/sm_Fountain_01_10_Monument_01.nanite.tres"

func _init() -> void:
	print("=== Cluster Bounds Analysis ===")
	var res: NaniteMeshResource = load(TRES_PATH)
	if res == null:
		push_error("Failed to load .tres")
		quit(1)
		return

	var clusters_data: PackedByteArray = res.get_clusters_data()
	var cluster_count: int = res.get_cluster_count()
	var cluster_stride: int = 68

	var lod_global_aabb: Dictionary = {}
	for ci: int in range(cluster_count):
		var off: int = ci * cluster_stride
		var gid: int = decode_u32(clusters_data, off + 16)
		var tri_count: int = decode_u32(clusters_data, off + 12)
		if tri_count == 0:
			continue

		var min_x: float = decode_f32(clusters_data, off + 28)
		var min_y: float = decode_f32(clusters_data, off + 32)
		var min_z: float = decode_f32(clusters_data, off + 36)
		var max_x: float = min_x + decode_f32(clusters_data, off + 40)
		var max_y: float = min_y + decode_f32(clusters_data, off + 44)
		var max_z: float = min_z + decode_f32(clusters_data, off + 48)

		if not lod_global_aabb.has(gid):
			var arr: Array = []
			arr.resize(2)
			arr[0] = Vector3(min_x, min_y, min_z)
			arr[1] = Vector3(max_x, max_y, max_z)
			lod_global_aabb[gid] = arr
		else:
			var cur: Array = lod_global_aabb[gid]
			var v0: Vector3 = cur[0]
			var v1: Vector3 = cur[1]
			cur[0] = Vector3(min(v0.x, min_x), min(v0.y, min_y), min(v0.z, min_z))
			cur[1] = Vector3(max(v1.x, max_x), max(v1.y, max_y), max(v1.z, max_z))

	print("LOD  | BBoxMin                          | BBoxMax                          | Extent")
	print("-----|----------------------------------|----------------------------------|--------")
	var max_lod: int = -1
	for k in lod_global_aabb:
		if int(k) > max_lod:
			max_lod = int(k)
	for lod: int in range(max_lod + 1):
		if lod_global_aabb.has(lod):
			var bb: Array = lod_global_aabb[lod]
			var v0: Vector3 = bb[0]
			var v1: Vector3 = bb[1]
			var extent: Vector3 = v1 - v0
			print("%-4d | (%6.2f, %6.2f, %6.2f) | (%6.2f, %6.2f, %6.2f) | (%.2f, %.2f, %.2f)" % [
				lod, v0.x, v0.y, v0.z, v1.x, v1.y, v1.z,
				extent.x, extent.y, extent.z
			])

	if lod_global_aabb.has(0) and lod_global_aabb.has(1):
		var bb0: Array = lod_global_aabb[0]
		var bb1: Array = lod_global_aabb[1]
		var v0_0: Vector3 = bb0[0]
		var v0_1: Vector3 = bb0[1]
		var v1_0: Vector3 = bb1[0]
		var v1_1: Vector3 = bb1[1]
		print("")
		print("LOD 0 extent: ", v0_1 - v0_0)
		print("LOD 1 extent: ", v1_1 - v1_0)
		var extent_ratio: Vector3 = (v1_1 - v1_0) / (v0_1 - v0_0)
		print("LOD 1 / LOD 0 extent ratio: (%.2f, %.2f, %.2f)" % [extent_ratio.x, extent_ratio.y, extent_ratio.z])

	quit(0)

func decode_u32(data: PackedByteArray, offset: int) -> int:
	return (data[offset] |
		(data[offset + 1] << 8) |
		(data[offset + 2] << 16) |
		(data[offset + 3] << 24))

func decode_f32(data: PackedByteArray, offset: int) -> float:
	var bits: int = (data[offset] |
		(data[offset + 1] << 8) |
		(data[offset + 2] << 16) |
		(data[offset + 3] << 24))
	return interpret_float(bits)

func interpret_float(bits: int) -> float:
	if bits == 0:
		return 0.0
	var sign: float = -1.0 if (bits & 0x80000000) != 0 else 1.0
	var exp: int = ((bits >> 23) & 0xFF) - 127
	var mantissa: float = 1.0
	for i: int in range(23):
		if (bits >> (22 - i)) & 1:
			mantissa += 1.0 / pow(2.0, i + 1)
	return sign * mantissa * pow(2.0, exp)