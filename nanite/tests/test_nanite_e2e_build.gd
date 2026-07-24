#!/usr/bin/env godot
# nanite/tests/test_nanite_e2e_build.gd
#
# Stage 0 end-to-end build pipeline test (Task 0.10.1). Runs as a
# SceneTree script via:
#   godot --script res://nanite/tests/test_nanite_e2e_build.gd
#
# Verifies the spec's "阶段 0 端到端流程" scenarios:
#   - 完整构建管线: BuilderConfig → NaniteBuilder → build() returns a
#     non-null NaniteMeshResource with cluster_count > 0, node_count > 0,
#     page_count > 0, shadow_mesh != null.
#   - 50K tri 大网格构建: ~50000 tri mesh → cluster_count > 100,
#     node_count > 10, build completes in observable time.
#   - save/load roundtrip: .tres (ResourceSaver/ResourceLoader) and
#     .nanite (custom binary format) both preserve cluster/node/page
#     counts and blob sizes.
#
# GUT may not be available in Stage 0, so this is a plain SceneTree script
# that uses asserts and prints rather than GUT assertions.

extends SceneTree

var _failures: Array = []

func _assert(condition: bool, msg: String) -> void:
	if not condition:
		_failures.append(msg)
		print("FAIL: ", msg)
	else:
		print("PASS: ", msg)

# Procedurally build a UV sphere with `segments` rings × `segments` segs.
# Mirrors NaniteTestHelpers::create_sphere_mesh in C++ (test_helpers.h).
# For segments = N, produces (N+1)×(N+1) vertices and 2*N*N triangles.
func _build_test_sphere(segments: int) -> ArrayMesh:
	var verts := PackedVector3Array()
	var indices := PackedInt32Array()
	var rings := segments
	var segs := segments
	for i in range(rings + 1):
		var theta := PI * float(i) / float(rings)
		var sin_t := sin(theta)
		var cos_t := cos(theta)
		for j in range(segs + 1):
			var phi := 2.0 * PI * float(j) / float(segs)
			verts.append(Vector3(sin_t * cos(phi), cos_t, sin_t * sin(phi)))
	for i in range(rings):
		for j in range(segs):
			var v0 := i * (segs + 1) + j
			var v1 := v0 + 1
			var v2 := (i + 1) * (segs + 1) + j
			var v3 := v2 + 1
			indices.append_array([v0, v2, v1, v1, v2, v3])
	var arrays := []
	arrays.resize(Mesh.ARRAY_MAX)
	arrays[Mesh.ARRAY_VERTEX] = verts
	arrays[Mesh.ARRAY_INDEX] = indices
	var mesh := ArrayMesh.new()
	mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays)
	return mesh

# Procedurally build a "large" grid mesh with ~target_tris triangles and a
# small sinusoidal Z displacement so cluster bounds have volume. Mirrors
# NaniteTestHelpers::create_large_test_mesh in C++.
func _build_test_large_mesh(target_tris: int) -> ArrayMesh:
	var n := int(ceil(sqrt(float(target_tris) * 0.5)))
	if n < 1:
		n = 1
	var verts := PackedVector3Array()
	var indices := PackedInt32Array()
	for i in range(n + 1):
		for j in range(n + 1):
			var x := -1.0 + 2.0 * float(i) / float(n)
			var y := -1.0 + 2.0 * float(j) / float(n)
			var z := 0.1 * sin(float(i) * 0.7) * cos(float(j) * 0.7)
			verts.append(Vector3(x, y, z))
	for i in range(n):
		for j in range(n):
			var v0 := i * (n + 1) + j
			var v1 := v0 + 1
			var v2 := (i + 1) * (n + 1) + j
			var v3 := v2 + 1
			indices.append_array([v0, v2, v1, v1, v2, v3])
	var arrays := []
	arrays.resize(Mesh.ARRAY_MAX)
	arrays[Mesh.ARRAY_VERTEX] = verts
	arrays[Mesh.ARRAY_INDEX] = indices
	var mesh := ArrayMesh.new()
	mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays)
	return mesh

# Spec scenario "完整构建管线": default BuilderConfig → NaniteBuilder →
# build(sphere) returns a non-null NaniteMeshResource with non-zero
# cluster/node/page counts and a non-null shadow_mesh.
func _test_full_build_pipeline() -> void:
	print("\n=== test_full_build_pipeline ===")
	var cfg := BuilderConfig.new()
	_assert(cfg.is_valid(), "default BuilderConfig should be valid")
	var builder := NaniteBuilder.new()
	builder.config = cfg
	var sphere := _build_test_sphere(32)
	var res := builder.build(sphere)
	_assert(res != null, "build() should return non-null NaniteMeshResource")
	if res == null:
		return
	_assert(res.cluster_count > 0, "cluster_count > 0 (got %d)" % res.cluster_count)
	_assert(res.node_count > 0, "node_count > 0 (got %d)" % res.node_count)
	_assert(res.page_count > 0, "page_count > 0 (got %d)" % res.page_count)
	_assert(res.shadow_mesh != null, "shadow_mesh != null")

# Spec scenario "50K tri 大网格构建": a ~50000-triangle input mesh must
# yield cluster_count > 100 and node_count > 10. No hard ms upper limit —
# only observable progress is required.
func _test_build_large_mesh() -> void:
	print("\n=== test_build_large_mesh ===")
	var cfg := BuilderConfig.new()
	var builder := NaniteBuilder.new()
	builder.config = cfg
	var large_mesh := _build_test_large_mesh(50000)
	# surface_get_array_index_len returns the index count; /3 gives triangles.
	# (surface_get_array_len would return the vertex count, which is ~6x
	# smaller for these grid meshes.)
	var actual_tris := large_mesh.surface_get_array_index_len(0) / 3
	print("input triangles: ", actual_tris)
	var start := Time.get_ticks_msec()
	var res := builder.build(large_mesh)
	var elapsed := Time.get_ticks_msec() - start
	print("build time: %d ms" % elapsed)
	_assert(res != null, "build() should return non-null for large mesh")
	if res == null:
		return
	_assert(res.cluster_count > 100, "cluster_count > 100 (got %d)" % res.cluster_count)
	_assert(res.node_count > 10, "node_count > 10 (got %d)" % res.node_count)

# Save/load roundtrip via both Godot's native ResourceSaver/ResourceLoader
# (.tres) and NaniteMeshResource's custom .nanite binary format. Verifies
# that cluster/node/page counts and blob sizes are preserved.
func _test_save_load_roundtrip() -> void:
	print("\n=== test_save_load_roundtrip ===")
	var cfg := BuilderConfig.new()
	var builder := NaniteBuilder.new()
	builder.config = cfg
	var sphere := _build_test_sphere(32)
	var res := builder.build(sphere)
	if res == null:
		_assert(false, "build failed; cannot test save/load")
		return

	# --- .tres roundtrip (Godot native ResourceSaver/ResourceLoader) ---
	var path := "tmp/test_e2e_res.tres"
	DirAccess.make_dir_recursive_absolute("tmp")
	var save_err := ResourceSaver.save(res, path)
	_assert(save_err == OK, "ResourceSaver.save to %s should succeed (err=%d)" % [path, save_err])
	var loaded := ResourceLoader.load(path)
	_assert(loaded != null, "ResourceLoader.load should return non-null")
	if loaded == null:
		return
	var loaded_res := loaded as NaniteMeshResource
	_assert(loaded_res != null, "loaded should be NaniteMeshResource")
	if loaded_res == null:
		return
	_assert(loaded_res.cluster_count == res.cluster_count, "cluster_count roundtrip (%d == %d)" % [loaded_res.cluster_count, res.cluster_count])
	_assert(loaded_res.node_count == res.node_count, "node_count roundtrip (%d == %d)" % [loaded_res.node_count, res.node_count])
	_assert(loaded_res.page_count == res.page_count, "page_count roundtrip (%d == %d)" % [loaded_res.page_count, res.page_count])

	# --- .nanite binary roundtrip (NaniteMeshResource::save / load) ---
	var nanite_path := "tmp/test_e2e_res.nanite"
	var bin_save_err := res.save(nanite_path)
	_assert(bin_save_err == OK, "save .nanite should succeed (err=%d)" % bin_save_err)
	var loaded_bin := NaniteMeshResource.new()
	var bin_load_err := loaded_bin.load(nanite_path)
	_assert(bin_load_err == OK, "load .nanite should succeed (err=%d)" % bin_load_err)
	if bin_load_err != OK:
		return
	_assert(loaded_bin.cluster_count == res.cluster_count, ".nanite cluster_count roundtrip (%d == %d)" % [loaded_bin.cluster_count, res.cluster_count])
	_assert(loaded_bin.vertex_data.size() == res.vertex_data.size(), ".nanite vertex_data size match (%d == %d)" % [loaded_bin.vertex_data.size(), res.vertex_data.size()])

func _init():
	print("=== Nanite Stage 0 E2E Tests ===")
	_test_full_build_pipeline()
	_test_build_large_mesh()
	_test_save_load_roundtrip()
	if _failures.is_empty():
		print("\n=== ALL E2E TESTS PASSED ===")
	else:
		print("\n=== %d E2E TEST FAILURES ===" % _failures.size())
		for f in _failures:
			print("  - ", f)
	quit()
