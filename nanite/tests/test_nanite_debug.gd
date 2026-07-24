#!/usr/bin/env godot
# nanite/tests/test_nanite_debug.gd
#
# Tests for NaniteDebug runtime behavior exposed through NaniteServer
# (Task 1.9.5). Verifies the default debug mode is NONE and that
# set_debug_mode / get_debug_mode round-trip through the singleton.
#
# Run via: godot --headless --script res://nanite/tests/test_nanite_debug.gd

extends RefCounted

# Tests for NaniteDebug runtime behavior exposed through NaniteServer.
# Run via: godot --headless --script res://nanite/tests/test_nanite_debug.gd

const EXPECT_NONE = 0
const EXPECT_CLUSTER_SOLID = 1

func test_debug_mode_none_default():
	var srv = Engine.get_singleton("NaniteServer")
	assert(srv != null, "NaniteServer singleton should be available")
	var current = srv.get_debug_mode()
	assert(current == EXPECT_NONE, "Default debug mode should be NONE (0), got %d" % current)
	print("[PASS] test_debug_mode_none_default")

func test_debug_mode_can_change():
	var srv = Engine.get_singleton("NaniteServer")
	assert(srv != null, "NaniteServer singleton should be available")
	srv.set_debug_mode(EXPECT_CLUSTER_SOLID)
	var current = srv.get_debug_mode()
	assert(current == EXPECT_CLUSTER_SOLID, "Debug mode should be CLUSTER_SOLID_COLOR (1), got %d" % current)
	# Restore to NONE for isolation between tests.
	srv.set_debug_mode(EXPECT_NONE)
	print("[PASS] test_debug_mode_can_change")

func _run_all():
	test_debug_mode_none_default()
	test_debug_mode_can_change()
	print("All NaniteDebug tests passed.")

static func main():
	var t = new()
	t._run_all()
