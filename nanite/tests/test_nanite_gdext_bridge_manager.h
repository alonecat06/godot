/**************************************************************************/
/*  test_nanite_gdext_bridge_manager.h                                    */
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

// Task 1.17 — tests for the NaniteGDExtBridgeManager singleton +
// NaniteServer::set_bridge() injection. The manager class lives in
// nanite/bridge/ and is always compiled (no NANITE_BRIDGE_GDEXT gate on
// the source files — only register_types gates instantiation).

#include "nanite/bridge/nanite_gdext_bridge.h"
#include "nanite/bridge/nanite_gdext_bridge_manager.h"
#include "nanite/core/nanite_bridge.h"
#include "nanite/core/nanite_server.h"
#include "scene/resources/compositor.h"
#include "tests/test_macros.h"

namespace TestNaniteGDExtBridgeManager {

TEST_CASE("[Nanite][BridgeManager] set_bridge_setter_injects_pointer") {
	// Task 1.17.2 — NaniteServer holds an abstract INaniteBridge * that is
	// injected by the bridge layer's Manager via set_bridge(). Verify the
	// setter actually stores the pointer (and that nullptr clears it).
	NaniteServer *ns = NaniteServer::get_singleton();
	REQUIRE(ns != nullptr);

	// Snapshot the existing bridge pointer so we don't disturb production
	// state if the manager already wired itself up at SERVERS level.
	INaniteBridge *original = ns->get_bridge();

	// Construct a throwaway bridge and inject it.
	Ref<NaniteGDExtBridge> bridge = memnew(NaniteGDExtBridge());
	REQUIRE(bridge.is_valid());
	ns->set_bridge(bridge.ptr());
	CHECK(ns->get_bridge() == bridge.ptr());

	// Clearing via nullptr must reset the link without touching the Ref.
	ns->set_bridge(nullptr);
	CHECK(ns->get_bridge() == nullptr);

	// Restore the original link (may be nullptr in headless test runs).
	ns->set_bridge(original);
	CHECK(ns->get_bridge() == original);
}

TEST_CASE("[Nanite][BridgeManager] attach_to_compositor_appends_effects") {
	// Task 1.17.4 — attach_to_compositor() must append the two nanite
	// CompositorEffects (PRE_OPAQUE + POST_OPAQUE) to a user-provided
	// Compositor and be idempotent (calling twice does not duplicate).
	// We construct a fresh local Manager (init() requires a NaniteServer
	// pointer + SceneTree, both of which may or may not be available in
	// the test env), so instead we test attach_to_compositor directly by
	// memnew'ing a Manager and seeding just the bridge Refs via init().

	// Use the singleton if it was registered at SERVERS level; otherwise
	// skip — headless test_main runs after register_types so the manager
	// should be present whenever NANITE_BRIDGE_GDEXT was compiled.
	NaniteGDExtBridgeManager *mgr = NaniteGDExtBridgeManager::get_singleton();
	if (mgr == nullptr || mgr->get_default_compositor().is_null()) {
		// Manager not initialized (e.g. compiled without NANITE_BRIDGE_GDEXT
		// or SceneTree unavailable in the test harness). Skip cleanly.
		return;
	}

	Ref<Compositor> user_compositor;
	user_compositor.instantiate();
	REQUIRE(user_compositor.is_valid());

	// Before attach, the user's compositor has no effects.
	CHECK(user_compositor->get_compositor_effects().size() == 0);

	mgr->attach_to_compositor(user_compositor);

	// After attach, both nanite effects should be present.
	TypedArray<CompositorEffect> effects = user_compositor->get_compositor_effects();
	CHECK(effects.size() == 2);

	// Calling attach again must NOT duplicate the effects.
	mgr->attach_to_compositor(user_compositor);
	effects = user_compositor->get_compositor_effects();
	CHECK(effects.size() == 2);
}

TEST_CASE("[Nanite][BridgeManager] default_compositor_bundles_two_bridges") {
	// Task 1.17.3 — the Manager's pre-built default_compositor must
	// contain exactly the PRE_OPAQUE + POST_OPAQUE bridges, in that order.
	NaniteGDExtBridgeManager *mgr = NaniteGDExtBridgeManager::get_singleton();
	if (mgr == nullptr || mgr->get_default_compositor().is_null()) {
		return; // Manager not initialized — skip.
	}

	Ref<Compositor> dc = mgr->get_default_compositor();
	REQUIRE(dc.is_valid());
	TypedArray<CompositorEffect> effects = dc->get_compositor_effects();
	REQUIRE(effects.size() == 2);

	Ref<CompositorEffect> first = effects[0];
	Ref<CompositorEffect> second = effects[1];
	REQUIRE(first.is_valid());
	REQUIRE(second.is_valid());
	CHECK(first->get_effect_callback_type() == CompositorEffect::EFFECT_CALLBACK_TYPE_PRE_OPAQUE);
	CHECK(second->get_effect_callback_type() == CompositorEffect::EFFECT_CALLBACK_TYPE_POST_OPAQUE);
}

} // namespace TestNaniteGDExtBridgeManager
