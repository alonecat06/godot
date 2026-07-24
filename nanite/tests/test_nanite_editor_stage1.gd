#!/usr/bin/env godot
# nanite/tests/test_nanite_editor_stage1.gd
#
# Stage 1 editor integration reflection tests for Nanite (Task 1.14.3).
# Runs as a SceneTree script via:
#   godot --script res://nanite/tests/test_nanite_editor_stage1.gd
#
# Purpose: verify the Stage 1 editor-facing surface area by reflecting on
# ClassDB registration, inheritance chains, and exposed properties, without
# instantiating any UI nodes.
#
# Why reflection instead of instantiation:
#   Stage 1 adds NaniteMeshInstance3D (a Node3D subclass) and reworks
#   NaniteMeshEditor to embed it. UI nodes (SubViewportContainer and friends)
#   cannot be instantiated from a headless SceneTree script — there is no
#   parent viewport, so the editor would crash. Instead we verify the
#   ClassDB surface: class registration, parent-class relationships, and
#   the property list bound to NaniteMeshInstance3D. This is sufficient to
#   confirm the Inspector will auto-discover the exposed properties and that
#   NaniteMeshEditor can legally hold a NaniteMeshInstance3D member.
#
# This file is the Stage 1 companion to test_nanite_editor.gd (Stage 0).
# Style mirrors test_nanite_editor.gd / test_nanite_e2e_build.gd.

extends SceneTree

var _failures: Array = []

func _assert(condition: bool, msg: String) -> void:
	if not condition:
		_failures.append(msg)
		print("FAIL: ", msg)
	else:
		print("PASS: ", msg)

# Helper: returns true if `cls` exposes a property named `prop_name` via
# ClassDB. Property list entries are Dictionaries with a "name" key.
func _class_has_property(cls: String, prop_name: String) -> bool:
	var props: Array = ClassDB.class_get_property_list(cls)
	for p in props:
		if p is Dictionary and p.has("name") and p["name"] == prop_name:
			return true
	return false

# Spec scenario "NaniteMeshInstance3D shows in Inspector":
# NaniteMeshInstance3D must be registered to ClassDB and inherit from
# MeshInstance3D (so the Inspector auto-renders its property editors).
# Transitive inheritance to VisualInstance3D is also checked. The four
# Nanite-specific properties must be exposed so they appear in the Inspector.
func test_nanite_mesh_instance_in_inspector() -> void:
	print("\n=== test_nanite_mesh_instance_in_inspector ===")
	_assert(ClassDB.class_exists("NaniteMeshInstance3D"),
			"NaniteMeshInstance3D is registered to ClassDB")
	_assert(ClassDB.is_parent_class("NaniteMeshInstance3D", "MeshInstance3D"),
			"NaniteMeshInstance3D inherits MeshInstance3D (Inspector auto-discovery)")
	_assert(ClassDB.is_parent_class("NaniteMeshInstance3D", "VisualInstance3D"),
			"NaniteMeshInstance3D (transitively) inherits VisualInstance3D")
	_assert(_class_has_property("NaniteMeshInstance3D", "nanite_mesh"),
			"NaniteMeshInstance3D exposes property 'nanite_mesh'")
	_assert(_class_has_property("NaniteMeshInstance3D", "nanite_enabled"),
			"NaniteMeshInstance3D exposes property 'nanite_enabled'")
	_assert(_class_has_property("NaniteMeshInstance3D", "forced_lod"),
			"NaniteMeshInstance3D exposes property 'forced_lod'")
	_assert(_class_has_property("NaniteMeshInstance3D", "relative_screen_size"),
			"NaniteMeshInstance3D exposes property 'relative_screen_size'")

# Spec scenario "NaniteMeshEditor uses NaniteMeshInstance3D":
# NaniteMeshEditor must be registered to ClassDB and inherit from
# SubViewportContainer (same as Stage 0). Its internal mesh_instance member
# is a C++ typed pointer to NaniteMeshInstance3D; GDScript cannot reflect
# on raw C++ member types, so we verify the prerequisite — that the
# NaniteMeshInstance3D class itself is registered (otherwise NaniteMeshEditor
# could not reference it). Full UI behavior is a manual editor verification.
func test_nanite_mesh_editor_uses_nanite_instance() -> void:
	print("\n=== test_nanite_mesh_editor_uses_nanite_instance ===")
	_assert(ClassDB.class_exists("NaniteMeshEditor"),
			"NaniteMeshEditor is registered to ClassDB")
	_assert(ClassDB.is_parent_class("NaniteMeshEditor", "SubViewportContainer"),
			"NaniteMeshEditor inherits SubViewportContainer")
	_assert(ClassDB.class_exists("NaniteMeshInstance3D"),
			"NaniteMeshInstance3D is registered (prerequisite for NaniteMeshEditor.mesh_instance)")
	print("Note: NaniteMeshEditor's internal mesh_instance type is verified via code review, not runtime reflection (UI cannot be instantiated in headless mode).")

func _init():
	print("=== Nanite Stage 1 Editor Integration Tests ===")
	test_nanite_mesh_instance_in_inspector()
	test_nanite_mesh_editor_uses_nanite_instance()
	if _failures.is_empty():
		print("\n=== ALL STAGE 1 EDITOR TESTS PASSED ===")
	else:
		print("\n=== %d STAGE 1 EDITOR TEST FAILURES ===" % _failures.size())
		for f in _failures:
			print("  - ", f)
	quit()
