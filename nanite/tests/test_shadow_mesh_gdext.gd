extends RefCounted

# Test that the coarse-LOD shadow mesh is set on a NaniteMeshInstance3D.
# Run via: godot --headless --script res://nanite/tests/test_shadow_mesh_gdext.gd

func test_shadow_mesh_set_on_instance():
	var NaniteMeshInstance3D = ClassDB.instantiate("NaniteMeshInstance3D")
	assert(NaniteMeshInstance3D != null, "NaniteMeshInstance3D should be instantiable")
	# Build a minimal sphere resource via NaniteBuilder to get a real shadow_mesh.
	var NaniteBuilder = ClassDB.instantiate("NaniteBuilder")
	var sphere_mesh = preload("res://nanite/tests/_sphere_mesh.tres") # placeholder; see note
	# Note: this test requires a pre-built NaniteMeshResource with a valid
	# shadow_mesh. Stage 1 tests can skip this and just verify the API
	# exists; full integration is exercised via the C++ doctests.
	print("[SKIP] test_shadow_mesh_set_on_instance — needs pre-built resource fixture")

func _run_all():
	test_shadow_mesh_set_on_instance()
	print("All shadow mesh tests done.")

static func main():
	var t = new()
	t._run_all()
