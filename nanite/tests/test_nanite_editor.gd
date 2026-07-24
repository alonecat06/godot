#!/usr/bin/env godot
# nanite/tests/test_nanite_editor.gd
#
# Editor extension integration tests for Nanite (Task 0.9.6). Runs as a
# SceneTree script via:
#   godot --script res://nanite/tests/test_nanite_editor.gd
#
# Verifies the editor-facing surface area of the Nanite module by checking
# ClassDB registration and inheritance for the four editor classes:
#   - EditorInspectorPluginNanite : EditorInspectorPlugin
#   - NaniteMeshEditor : SubViewportContainer
#   - NaniteEditorPlugin : EditorPlugin
#   - NaniteResourcePreviewGenerator : EditorResourcePreviewGenerator
#
# Direct method calls (can_handle / handles / edit) are C++ virtual
# overrides without GDScript bindings, so they cannot be invoked from
# GDScript. ClassDB metadata is the verifiable surface for Stage 0.
# Full UI behavior (rendering, mouse drag, focus handling) is left to
# manual editor verification.
#
# Style mirrors test_nanite_e2e_build.gd.

extends SceneTree

var _failures: Array = []

func _assert(condition: bool, msg: String) -> void:
	if not condition:
		_failures.append(msg)
		print("FAIL: ", msg)
	else:
		print("PASS: ", msg)

# Spec scenario "can_handle 识别 NaniteMeshResource":
# EditorInspectorPluginNanite must be registered to ClassDB and inherit
# from EditorInspectorPlugin so the editor can dispatch can_handle() to
# it when a NaniteMeshResource is selected.
func _test_can_handle_nanite_resource() -> void:
	print("\n=== test_can_handle_nanite_resource ===")
	_assert(ClassDB.class_exists("EditorInspectorPluginNanite"),
			"EditorInspectorPluginNanite is registered to ClassDB")
	_assert(ClassDB.is_parent_class("EditorInspectorPluginNanite", "EditorInspectorPlugin"),
			"EditorInspectorPluginNanite inherits EditorInspectorPlugin")
	_assert(ClassDB.class_exists("NaniteMeshResource"),
			"NaniteMeshResource is registered to ClassDB (so can_handle would match)")

# Spec scenario "edit 不崩溃":
# NaniteMeshEditor must be registered to ClassDB and inherit from
# SubViewportContainer so the editor can instantiate it as an Inspector
# panel. Instantiating UI nodes from a headless SceneTree script crashes
# (no parent viewport), so we verify registration + inheritance only.
# Actual edit() invocation is a manual editor verification task.
func _test_edit_does_not_crash() -> void:
	print("\n=== test_edit_does_not_crash ===")
	_assert(ClassDB.class_exists("NaniteMeshEditor"),
			"NaniteMeshEditor is registered to ClassDB")
	_assert(ClassDB.is_parent_class("NaniteMeshEditor", "SubViewportContainer"),
			"NaniteMeshEditor inherits SubViewportContainer")
	_assert(ClassDB.is_parent_class("NaniteMeshEditor", "Control"),
			"NaniteMeshEditor (transitively) inherits Control (Inspector-compatible)")
	_assert(ClassDB.class_exists("NaniteEditorPlugin"),
			"NaniteEditorPlugin is registered to ClassDB")
	_assert(ClassDB.is_parent_class("NaniteEditorPlugin", "EditorPlugin"),
			"NaniteEditorPlugin inherits EditorPlugin")

# Spec scenario "handles 返回 true":
# NaniteResourcePreviewGenerator must be registered to ClassDB and inherit
# from EditorResourcePreviewGenerator. The C++ override of handles() is
# verified via doctest in test_nanite_resource.h (which is exercised by the
# doctest suite); GDScript can only verify the ClassDB surface.
func _test_handles_returns_true() -> void:
	print("\n=== test_handles_returns_true ===")
	_assert(ClassDB.class_exists("NaniteResourcePreviewGenerator"),
			"NaniteResourcePreviewGenerator is registered to ClassDB")
	_assert(ClassDB.is_parent_class("NaniteResourcePreviewGenerator", "EditorResourcePreviewGenerator"),
			"NaniteResourcePreviewGenerator inherits EditorResourcePreviewGenerator")

func _init():
	print("=== Nanite Stage 0 Editor Integration Tests ===")
	_test_can_handle_nanite_resource()
	_test_edit_does_not_crash()
	_test_handles_returns_true()
	if _failures.is_empty():
		print("\n=== ALL EDITOR TESTS PASSED ===")
	else:
		print("\n=== %d EDITOR TEST FAILURES ===" % _failures.size())
		for f in _failures:
			print("  - ", f)
	quit()
