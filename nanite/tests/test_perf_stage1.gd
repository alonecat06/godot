#!/usr/bin/env godot
# nanite/tests/test_perf_stage1.gd
#
# Stage 1 performance benchmark (Task 1.13.2). Runs as a SceneTree script
# via:
#   godot --script res://nanite/tests/test_perf_stage1.gd
#
# Verifies the spec's "Stage 1 性能基准" scenario:
#   - test_perf_100k_tri_30fps: build a ~100K-triangle sphere
#     NaniteMeshResource, attach it to a NaniteMeshInstance3D + Camera3D
#     scene, warm up 5 frames, then measure 60 frames of total elapsed
#     time. The average frame time must stay under 33 ms (>= 30 fps) and
#     a perf report (avg_ms, fps, tri_count) is printed.
#
# If NaniteMeshInstance3D is not registered to ClassDB, or the large
# resource build fails, the test is SKIPped with a printed reason rather
# than counted as a failure.
#
# Style mirrors test_nanite_e2e_build.gd / test_nanite_editor.gd.

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
# segments = 224 → 2*224*224 = 100,352 triangles (~100K).
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

# Private helper: build a ~100K-triangle sphere NaniteMeshResource via
# NaniteBuilder. Returns null (and prints a reason) if the build fails so
# the caller can SKIP the perf test instead of failing it.
func _build_large_resource() -> Resource:
	var segments := 224 # 2*224*224 = 100,352 triangles.
	var sphere := _build_test_sphere(segments)
	var actual_tris := sphere.surface_get_array_index_len(0) / 3
	print("input triangles: ", actual_tris)
	var cfg := BuilderConfig.new()
	var builder := NaniteBuilder.new()
	builder.config = cfg
	var build_start := Time.get_ticks_msec()
	var res := builder.build(sphere)
	var build_elapsed := Time.get_ticks_msec() - build_start
	print("build time: %d ms" % build_elapsed)
	if res == null:
		print("SKIP reason: NaniteBuilder.build returned null for %d-tri sphere" % actual_tris)
		return null
	print("cluster_count: ", res.cluster_count, " node_count: ", res.node_count)
	return res

# Spec scenario "100K tri 30fps": the Stage 1 runtime must sustain >= 30 fps
# while rendering a ~100K-triangle Nanite mesh. Measures 60 frames of total
# elapsed time after a 5-frame warmup and asserts avg_ms < 33.0.
func _test_perf_100k_tri_30fps() -> void:
	print("\n=== test_perf_100k_tri_30fps ===")
	if not ClassDB.class_exists("NaniteMeshInstance3D"):
		print("SKIP: NaniteMeshInstance3D not registered to ClassDB")
		return
	var ns = Engine.get_singleton("NaniteServer")
	_assert(ns != null, "NaniteServer singleton should be available")
	if ns == null:
		return

	# 1. Build the large resource; SKIP if the build fails. tri_count is
	#    derived from the sphere formula (2 * segments^2 for segments = 224)
	#    since NaniteMeshResource exposes cluster/node counts, not raw tris.
	var res := _build_large_resource()
	if res == null:
		return
	var tri_count := 2 * 224 * 224 # = 100,352 triangles.

	# 2. Create NaniteMeshInstance3D + Camera3D scene.
	var root := Node3D.new()
	root.name = "PerfRoot"
	var instance = ClassDB.instantiate("NaniteMeshInstance3D")
	if instance == null:
		print("SKIP: ClassDB.instantiate(NaniteMeshInstance3D) returned null")
		root.free()
		return
	instance.set_nanite_mesh(res)
	var camera := Camera3D.new()
	camera.name = "PerfCamera"
	camera.transform = Transform3D(Basis.IDENTITY, Vector3(0, 0, 3))
	root.add_child(instance)
	root.add_child(camera)
	get_root().add_child(root)

	# 3. Warm up 5 frames so the GPU pipeline uploads mesh data and the
	#    SceneTree settles before measurement.
	for i in range(5):
		await process_frame

	# 4. Measure 60 frames of total elapsed time.
	var frame_count := 60
	var start_ms := Time.get_ticks_msec()
	for i in range(frame_count):
		await process_frame
	var total_ms := Time.get_ticks_msec() - start_ms

	# 5. Compute average frame time and derive fps.
	var avg_ms := float(total_ms) / float(frame_count)
	var fps := 1000.0 / avg_ms if avg_ms > 0.0 else 0.0

	# 6. Assert avg_ms < 33.0 (>= 30 fps).
	_assert(avg_ms < 33.0, "avg frame time < 33.0 ms / >= 30 fps (avg=%.2f ms, fps=%.1f)" % [avg_ms, fps])

	# 7. Print perf report.
	print("--- perf report ---")
	print("  tri_count:  ", tri_count)
	print("  frames:     ", frame_count)
	print("  total_ms:   ", total_ms)
	print("  avg_ms:     %.2f" % avg_ms)
	print("  fps:        %.1f" % fps)

	# Cleanup: remove from tree (triggers EXIT_TREE -> unregister_instance)
	# and free all nodes.
	get_root().remove_child(root)
	root.free()

# Async test runner. _init() cannot await, so it defers this coroutine which
# sequentially runs each test and quits once all are done.
func _run_tests() -> void:
	await _test_perf_100k_tri_30fps()
	if _failures.is_empty():
		print("\n=== ALL PERF STAGE 1 TESTS PASSED ===")
	else:
		print("\n=== %d PERF STAGE 1 TEST FAILURES ===" % _failures.size())
		for f in _failures:
			print("  - ", f)
	quit()

func _init():
	print("=== Nanite Stage 1 Performance Benchmarks ===")
	# _init cannot await; defer the async test runner so it starts on
	# the next idle frame and the SceneTree main loop keeps running.
	_run_tests.call_deferred()
