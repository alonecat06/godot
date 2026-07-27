/**************************************************************************/
/*  nanite_gdext_bridge_manager.cpp                                       */
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

#include "nanite/bridge/nanite_gdext_bridge_manager.h"

#include "nanite/core/nanite_server.h"

#include "core/config/project_settings.h"
#include "core/object/callable_mp.h"
#include "core/object/object_id.h" // ObjectID (deferred-call parameter)
#include "core/variant/variant.h"
#include "scene/main/scene_tree.h"
#include "scene/main/viewport.h"

// Static singleton pointer; set in init(), cleared in finish().
NaniteGDExtBridgeManager *NaniteGDExtBridgeManager::singleton = nullptr;

void NaniteGDExtBridgeManager::init(NaniteServer *p_server) {
	singleton = this;

	// Register the auto-attach toggle and read its current value in one
	// call. GLOBAL_DEF is idempotent and returns the registered value
	// (existing or default); using its return value avoids a separate
	// GLOBAL_GET, which can trip on ProjectSettings / OS initialization
	// ordering at SERVERS level (get_setting_with_override() calls
	// OS::get_singleton()->has_feature() during feature-override lookup).
	auto_attach_enabled = GLOBAL_DEF(PropertyInfo(Variant::BOOL, "nanite/bridge/auto_attach_compositor"), true);

	// 1) Create two CompositorEffect instances (one per callback type).
	//    The PRE_OPAQUE instance is also the "main" INaniteBridge handle
	//    handed to NaniteServer.
	pre_opaque_bridge = Ref<NaniteGDExtBridge>(memnew(NaniteGDExtBridge(CompositorEffect::EFFECT_CALLBACK_TYPE_PRE_OPAQUE)));
	post_opaque_bridge = Ref<NaniteGDExtBridge>(memnew(NaniteGDExtBridge(CompositorEffect::EFFECT_CALLBACK_TYPE_POST_OPAQUE)));

	// 2) Build a pre-baked Compositor Resource bundling both bridges.
	default_compositor.instantiate();
	{
		TypedArray<CompositorEffect> effects;
		effects.push_back(pre_opaque_bridge);
		effects.push_back(post_opaque_bridge);
		default_compositor->set_compositor_effects(effects);
	}

	// 3) Install the bridge onto the server + inject via setter so the
	//    core never imports the concrete bridge type.
	if (p_server != nullptr) {
		pre_opaque_bridge->install(p_server);
		p_server->set_bridge(pre_opaque_bridge.ptr());
	}

	// 4) Hook SceneTree signals for automatic viewport attachment.
	//    node_added fires for every Node inserted; process_frame fires once
	//    per main-loop tick (used as a 60-frame poll cadence for the
	//    `_viewports` group fallback path).
	SceneTree *st = SceneTree::get_singleton();
	if (st != nullptr) {
		st->connect("node_added", callable_mp(this, &NaniteGDExtBridgeManager::_on_node_added));
		st->connect("process_frame", callable_mp(this, &NaniteGDExtBridgeManager::_on_process_frame));
	}
}

void NaniteGDExtBridgeManager::finish() {
	// Disconnect SceneTree signals first so no late callback touches
	// releasing Refs.
	SceneTree *st = SceneTree::get_singleton();
	if (st != nullptr) {
		if (st->is_connected("node_added", callable_mp(this, &NaniteGDExtBridgeManager::_on_node_added))) {
			st->disconnect("node_added", callable_mp(this, &NaniteGDExtBridgeManager::_on_node_added));
		}
		if (st->is_connected("process_frame", callable_mp(this, &NaniteGDExtBridgeManager::_on_process_frame))) {
			st->disconnect("process_frame", callable_mp(this, &NaniteGDExtBridgeManager::_on_process_frame));
		}
	}

	// Drop the core's bridge pointer — the Refs are about to be released.
	if (NaniteServer::get_singleton() != nullptr) {
		NaniteServer::get_singleton()->set_bridge(nullptr);
	}

	default_compositor.unref();
	post_opaque_bridge.unref();
	pre_opaque_bridge.unref();

	if (singleton == this) {
		singleton = nullptr;
	}
}

void NaniteGDExtBridgeManager::attach_to_viewport(Viewport *p_vp) {
	// Public entry point used by editor preview windows and user code.
	// Defers the actual attachment so callers can invoke during
	// construction (before the viewport is inside the tree) — the deferred
	// handler bails out if the viewport still isn't in the tree by the
	// time it runs.
	if (p_vp == nullptr) {
		return;
	}
	// call_deferred bound to an ObjectID arg. Using ObjectID (rather than
	// Viewport*) avoids a dangling pointer if the Viewport is freed before
	// the deferred call fires — see _attach_viewport_deferred for details.
	callable_mp(this, &NaniteGDExtBridgeManager::_attach_viewport_deferred).call_deferred(p_vp->get_instance_id());
}

void NaniteGDExtBridgeManager::attach_to_compositor(const Ref<Compositor> &p_compositor) {
	// Append the nanite effects to a user-provided Compositor. Idempotent:
	// effects already present are not re-added.
	if (p_compositor.is_null()) {
		return;
	}
	if (pre_opaque_bridge.is_null() || post_opaque_bridge.is_null()) {
		return;
	}

	TypedArray<CompositorEffect> effects = p_compositor->get_compositor_effects();
	bool has_pre = false;
	bool has_post = false;
	for (int i = 0; i < effects.size(); ++i) {
		Ref<CompositorEffect> e = effects[i];
		if (e == pre_opaque_bridge) {
			has_pre = true;
		} else if (e == post_opaque_bridge) {
			has_post = true;
		}
	}
	if (!has_pre) {
		effects.push_back(pre_opaque_bridge);
	}
	if (!has_post) {
		effects.push_back(post_opaque_bridge);
	}
	p_compositor->set_compositor_effects(effects);
}

void NaniteGDExtBridgeManager::_attach_viewport_deferred(ObjectID p_vp_id) {
	// Resolve the ObjectID back to an Object* at the moment the deferred
	// call fires. If the Viewport was freed between queueing and now,
	// ObjectDB::get_instance() returns nullptr and we bail out cleanly —
	// no dangling pointer dereference.
	Object *obj = ObjectDB::get_instance(p_vp_id);
	if (obj == nullptr) {
		return;
	}
	Viewport *vp = Object::cast_to<Viewport>(obj);
	if (vp == nullptr) {
		return;
	}
	if (!vp->is_inside_tree()) {
		// Viewport isn't in the tree yet — wait for the next poll.
		return;
	}
	_attach_viewport(vp);
}

void NaniteGDExtBridgeManager::_attach_viewport(Viewport *p_vp) {
	if (p_vp == nullptr) {
		return;
	}
	if (default_compositor.is_null()) {
		return;
	}

	Ref<World3D> w = p_vp->find_world_3d();
	if (w.is_null()) {
		return;
	}

	Ref<Compositor> existing = w->get_compositor();
	if (existing.ptr() == default_compositor.ptr()) {
		return; // Already ours — nothing to do.
	}
	if (existing.is_valid()) {
		// User already has a Compositor — append rather than replace.
		attach_to_compositor(existing);
		return;
	}
	w->set_compositor(default_compositor);
}

void NaniteGDExtBridgeManager::_on_node_added(Node *p_node) {
	if (!auto_attach_enabled) {
		return;
	}
	Viewport *vp = Object::cast_to<Viewport>(p_node);
	if (vp == nullptr) {
		return;
	}
	// Defer: when node_added fires, the viewport may not yet have its
	// world_3d assigned (e.g. SubViewport children added via add_child
	// before set_world_3d). Deferring lets the next message-queue flush
	// pick up the final world_3d.
	// Pass ObjectID (not Viewport*) so a Viewport freed before the
	// deferred call fires doesn't crash the message-queue dispatcher.
	callable_mp(this, &NaniteGDExtBridgeManager::_attach_viewport_deferred).call_deferred(vp->get_instance_id());
}

void NaniteGDExtBridgeManager::_on_process_frame() {
	if (!auto_attach_enabled) {
		return;
	}
	frame_counter++;
	if (frame_counter % 60 != 0) {
		return;
	}

	SceneTree *st = SceneTree::get_singleton();
	if (st == nullptr) {
		return;
	}

	// Fallback: scan every viewport in the `_viewports` group. This
	// catches viewports that already existed before the Manager connected
	// to node_added (e.g. editor SubViewports constructed during engine
	// boot, before the nanite module was initialized).
	Vector<Node *> viewports = st->get_nodes_in_group("_viewports");
	for (Node *n : viewports) {
		Viewport *vp = Object::cast_to<Viewport>(n);
		if (vp != nullptr) {
			_attach_viewport(vp);
		}
	}
}
