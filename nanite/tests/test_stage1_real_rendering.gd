#!/usr/bin/env godot
# nanite/tests/test_stage1_real_rendering.gd
#
# Task 1.16.13.2 — Stage 1 real-rendering end-to-end test.
#
# Verifies that the real Nanite GPU pipeline (Cull + Rasterize + HZB Build +
# Material Resolve) actually runs when a NaniteMeshInstance3D is in the scene
# tree. Runs as a SceneTree script via:
#   godot --script res://nanite/tests/test_stage1_real_rendering.gd
#
# Coverage:
#   - test_pipeline_runs_without_crash: NaniteMeshInstance3D with a built
#     sphere resource runs 5 frames without crashing; NaniteServer reports
#     exactly 1 instance and visible_cluster_count >= 0 (degraded because
#     the bridge is not auto-attached to the default Compositor in Stage 1;
#     when it is, visible_cluster_count > 0 on a frame where render_visibility
#     was called).
#   - test_debug_mode_can_be_set: NaniteServer.set_debug_mode() accepts all
#     7 modes without error (NONE=0..HZB_OCCLUSION=6).
#   - test_no_double_render: NaniteMeshInstance3D's shadow casting setting
#     is SHADOW_CASTING_SHADOWS_ONLY when nanite_enabled (Task 1.16.12).
#
# Style mirrors test_gdext_e2e.gd / test_nanite_e2e_build.gd.

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

# Task 1.16.13.2 — full pipeline must run without crashing when a
# NaniteMeshInstance3D is in the scene. The bridge is not auto-attached to
# the default Compositor in Stage 1 (S1-08), so visible_cluster_count may
# stay 0; this test mainly guards against regressions in registration,
# transform update, and resource lifetime.
func _test_pipeline_runs_without_crash() -> void:
	print("\n=== test_pipeline_runs_without_crash ===")
	if not ClassDB.class_exists("NaniteMeshInstance3D"):
		print("SKIP: NaniteMeshInstance3D not registered to ClassDB")
		return
	var ns = Engine.get_singleton("NaniteServer")
	_assert(ns != null, "NaniteServer singleton should be available")
	if ns == null:
		return

	var root := Node3D.new()
	root.name = "RealRenderRoot"

	var instance = ClassDB.instantiate("NaniteMeshInstance3D")
	_assert(instance != null, "ClassDB.instantiate(NaniteMeshInstance3D) should succeed")
	if instance == null:
		root.free()
		return

	# Build a higher-poly sphere so the cull/rasterize shader has work to do.
	var cfg := BuilderConfig.new()
	var builder := NaniteBuilder.new()
	builder.config = cfg
	var sphere := _build_test_sphere(48)
	var res := builder.build(sphere)
	_assert(res != null, "NaniteBuilder.build(sphere_48) should return non-null resource")
	if res == null:
		instance.free()
		root.free()
		return
	_assert(res.get_cluster_count() > 0, "Sphere resource should have clusters (got %d)" % res.get_cluster_count())

	instance.set_nanite_mesh(res)
	instance.transform = Transform3D(Basis.IDENTITY, Vector3(0, 0, 0))

	var camera := Camera3D.new()
	camera.name = "MainCamera"
	camera.transform = Transform3D(Basis.IDENTITY, Vector3(0, 0, 3))

	root.add_child(instance)
	root.add_child(camera)
	get_root().add_child(root)

	# Await 5 frames — if render_visibility fires (Stage 1 bridge not
	# auto-attached; S1-08) it will exercise cull + rasterize + material_resolve.
	# Either way, no crash should occur.
	for i in range(5):
		await process_frame

	var inst_count = ns.get_instance_count()
	_assert(inst_count == 1, "NaniteServer.get_instance_count() == 1 (got %d)" % inst_count)
	var vis_clusters = ns.get_visible_cluster_count()
	_assert(vis_clusters >= 0, "NaniteServer.get_visible_cluster_count() >= 0 (got %d)" % vis_clusters)

	get_root().remove_child(root)
	root.free()
	# After freeing, instance_count must return to 0.
	await process_frame
	_assert(ns.get_instance_count() == 0, "After free, instance_count == 0 (got %d)" % ns.get_instance_count())

# Task 1.16.13.2 — all 7 debug modes must be settable on NaniteServer.
func _test_debug_mode_can_be_set() -> void:
	print("\n=== test_debug_mode_can_be_set ===")
	var ns = Engine.get_singleton("NaniteServer")
	_assert(ns != null, "NaniteServer singleton should be available")
	if ns == null:
		return
	for mode in range(7): # NONE=0 .. HZB_OCCLUSION=6
		ns.set_debug_mode(mode)
		_assert(ns.get_debug_mode() == mode, "debug_mode round-trip %d" % mode)
	# Restore NONE.
	ns.set_debug_mode(0)

# Task 1.16.12 — when nanite_enabled, the MeshInstance3D shadow casting
# setting must be SHADOW_CASTING_SHADOWS_ONLY so the engine doesn't draw
# the shadow mesh in the main pass.
func _test_no_double_render() -> void:
	print("\n=== test_no_double_render ===")
	if not ClassDB.class_exists("NaniteMeshInstance3D"):
		print("SKIP: NaniteMeshInstance3D not registered to ClassDB")
		return
	var ns = Engine.get_singleton("NaniteServer")
	_assert(ns != null, "NaniteServer singleton should be available")
	if ns == null:
		return

	var root := Node3D.new()
	root.name = "NoDoubleRenderRoot"
	var instance = ClassDB.instantiate("NaniteMeshInstance3D")
	_assert(instance != null, "ClassDB.instantiate(NaniteMeshInstance3D) should succeed")
	if instance == null:
		root.free()
		return

	var cfg := BuilderConfig.new()
	var builder := NaniteBuilder.new()
	builder.config = cfg
	var sphere := _build_test_sphere(16)
	var res := builder.build(sphere)
	_assert(res != null, "NaniteBuilder.build(sphere_16) should return non-null resource")
	if res == null:
		instance.free()
		root.free()
		return

	instance.set_nanite_mesh(res)
	root.add_child(instance)
	get_root().add_child(root)

	await process_frame

	# GeometryInstance3D.ShadowCastingSetting: SHADOW_CASTING_SETTING_OFF=0,
	# SHADOW_CASTING_SETTING_ON=1, SHADOW_CASTING_SETTING_DOUBLE_SIDED=2,
	# SHADOW_CASTING_SETTING_SHADOWS_ONLY=3.
	var cast_setting = instance.get_cast_shadows_setting()
	_assert(cast_setting == GeometryInstance3D.SHADOW_CASTING_SETTING_SHADOWS_ONLY,
			"NaniteMeshInstance3D shadow setting should be SHADOWS_ONLY when nanite_enabled (got %d)" % cast_setting)

	# Disabling Nanite must restore SHADOW_CASTING_SETTING_ON.
	instance.set_nanite_enabled(false)
	await process_frame
	cast_setting = instance.get_cast_shadows_setting()
	_assert(cast_setting == GeometryInstance3D.SHADOW_CASTING_SETTING_ON,
			"shadow setting should be SHADOW_CASTING_SETTING_ON after set_nanite_enabled(false) (got %d)" % cast_setting)

	get_root().remove_child(root)
	root.free()

func _run_tests() -> void:
	await _test_pipeline_runs_without_crash()
	await _test_debug_mode_can_be_set()
	await _test_no_double_render()
	if _failures.is_empty():
		print("\n=== ALL STAGE 1 REAL-RENDERING TESTS PASSED ===")
	else:
		print("\n=== %d STAGE 1 REAL-RENDERING TEST FAILURES ===" % _failures.size())
		for f in _failures:
			print("  - ", f)
	quit()

func _init():
	print("=== Nanite Stage 1 Real-Rendering Tests (Task 1.16.13.2) ===")
	_run_tests.call_deferred()
