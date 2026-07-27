/**************************************************************************/
/*  nanite_gdext_bridge_manager.h                                         */
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

// Task 1.17 (S1-08 补完) — Independent singleton living in the gdext bridge
// layer. Owns the two NaniteGDExtBridge CompositorEffects (PRE_OPAQUE +
// POST_OPAQUE) plus a pre-built `default_compositor` Resource that bundles
// them. The Manager:
//
//   1. Creates the bridges + default_compositor in init().
//   2. Injects the PRE_OPAQUE bridge into NaniteServer via set_bridge()
//      so the core never sees the concrete bridge type.
//   3. Watches the SceneTree (`node_added` + 60-frame poll of `_viewports`)
//      and attaches `default_compositor` (or appends its effects) to the
//      World3D of every Viewport it discovers — covering game runtime,
//      Nanite preview windows, and the editor's 3D workspace.
//
// It is NOT a GDCLASS: lifetime is fully controlled by register_types.cpp
// (memnew at MODULE_INITIALIZATION_LEVEL_SERVERS, memdelete on teardown).

#include "core/object/object.h" // Object (callable_mp requires get_instance_id)
#include "scene/resources/compositor.h" // Compositor, CompositorEffect, Ref<>
#include "scene/resources/3d/world_3d.h" // World3D (for Ref<World3D>)

#include "nanite/bridge/nanite_gdext_bridge.h" // NaniteGDExtBridge

class NaniteServer;
class Node;
class Viewport;

// Inherits Object (no GDCLASS) so callable_mp / SceneTree::connect work —
// CallableCustomMethodPointer requires T::get_instance_id() to live in the
// ObjectDB so it can validate the receiver across deferred calls. Lifetime
// is still fully manual (memnew / memdelete in register_types).
class NaniteGDExtBridgeManager : public Object {
	static NaniteGDExtBridgeManager *singleton;

	// Two bridges: PRE_OPAQUE drives render_visibility; POST_OPAQUE drives
	// render_material_resolve. `pre_opaque_bridge` is the one handed to
	// NaniteServer as the active INaniteBridge handle.
	Ref<NaniteGDExtBridge> pre_opaque_bridge;
	Ref<NaniteGDExtBridge> post_opaque_bridge;

	// Pre-built Compositor Resource that bundles both bridges in its
	// effects list. Attached to Viewports via World3D::set_compositor().
	Ref<Compositor> default_compositor;

	bool auto_attach_enabled = true; // mirrors nanite/bridge/auto_attach_compositor
	int frame_counter = 0; // for the 60-frame poll cadence

	// SceneTree signal handlers.
	void _on_node_added(Node *p_node);
	void _on_process_frame();

	// Idempotent: ensures the viewport's find_world_3d() has the nanite
	// compositor attached (either directly or by appending effects).
	void _attach_viewport(Viewport *p_vp);
	void _attach_viewport_deferred(Viewport *p_vp);

public:
	static NaniteGDExtBridgeManager *get_singleton() { return singleton; }

	// Lifecycle. init() creates the bridges + default_compositor, injects
	// the bridge into NaniteServer, and wires the SceneTree signals. finish()
	// tears everything down. Called from register_types at SERVERS level.
	void init(NaniteServer *p_server);
	void finish();

	// Public API used by editor preview windows / user code to opt a
	// viewport into Nanite rendering.
	Ref<Compositor> get_default_compositor() const { return default_compositor; }
	void attach_to_viewport(Viewport *p_vp);
	void attach_to_compositor(const Ref<Compositor> &p_compositor);

	NaniteGDExtBridgeManager() = default;
	~NaniteGDExtBridgeManager() = default;
};
