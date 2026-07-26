/**************************************************************************/
/*  test_nanite_gdext_bridge.h                                            */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#pragma once

// NOTE: this header is globbed into modules_tests.gen.h (see modules/SCsub)
// and compiled as part of tests/test_main.cpp under the *main* env, which
// does NOT carry NANITE_BRIDGE_GDEXT (that define lives only on env_nanite).
// We therefore DO NOT #ifdef the body here — the NaniteGDExtBridge class is
// always compiled (nanite/SCsub globs bridge/*.cpp unconditionally), and the
// tests use memnew / is_class / install, none of which require ClassDB
// registration (which IS gated on NANITE_BRIDGE_GDEXT in register_types.cpp).

#include "nanite/bridge/nanite_gdext_bridge.h"
#include "nanite/core/nanite_bridge.h"
#include "nanite/core/nanite_server.h"
#include "scene/resources/compositor.h"
#include "tests/test_macros.h"

namespace TestNaniteGDExtBridge {

TEST_CASE("[Nanite][Bridge] gdext_bridge_registers_as_compositor_effect") {
	// NaniteGDExtBridge is a CompositorEffect (Resource -> RefCounted).
	// Use a Ref<> for lifecycle; memnew on a RefCounted yields refcount 1
	// and Ref<> takes ownership. is_class() walks the GDCLASS static
	// hierarchy, so it works regardless of ClassDB registration timing.
	Ref<NaniteGDExtBridge> bridge = memnew(NaniteGDExtBridge());
	REQUIRE(bridge.is_valid());
	CHECK(bridge->is_class("NaniteGDExtBridge"));
	CHECK(bridge->is_class("CompositorEffect"));
	CHECK(bridge->is_class("Resource"));
}

TEST_CASE("[Nanite][Bridge] install_sets_coarse_lod_shadow_mode") {
	// Construct a throwaway NaniteServer (no init() — we only exercise
	// the shadow-mode setter, not the singleton / GPU pipeline). The
	// destructor is safe: it only clears `singleton` if singleton == this,
	// which init() would have set and we did not call.
	NaniteServer *server = memnew(NaniteServer);

	// Seed with the non-target value so the assertion can actually fail
	// if install() forgets to set the mode (the default is already
	// SHADOW_COARSE_LOD, so without this the test would be vacuous).
	server->set_shadow_mode(INaniteBridge::SHADOW_DYNAMIC_GPU);

	Ref<NaniteGDExtBridge> bridge = memnew(NaniteGDExtBridge());
	REQUIRE(bridge.is_valid());
	bridge->install(server);

	CHECK(server->get_shadow_mode() == INaniteBridge::SHADOW_COARSE_LOD);

	memdelete(server);
}

} // namespace TestNaniteGDExtBridge
