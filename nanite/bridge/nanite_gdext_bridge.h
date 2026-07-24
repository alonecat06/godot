/**************************************************************************/
/*  nanite_gdext_bridge.h                                                 */
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

#include "core/nanite_bridge.h" // INaniteBridge
#include "scene/resources/compositor.h" // CompositorEffect (also pulls in RenderData)

class NaniteServer;

// NaniteGDExtBridge wraps a single CompositorEffect callback (PRE_OPAQUE or
// POST_OPAQUE). Two instances are created by NaniteServer::init() — one for
// each callback — to drive the Nanite GPU pipeline at the correct frame
// stages. The PRE_OPAQUE instance is also used as the "main" INaniteBridge
// pointer (install() is only called on it).
//
// IMPORTANT — how the callback is actually dispatched:
// CompositorEffect::set_effect_callback_type() registers
//   callable_mp(this, &CompositorEffect::_call_render_callback)
// with the RenderingServer. _call_render_callback is NON-virtual and only
// forwards to GDVIRTUAL2(_render_callback, ...), whose dispatch reaches
// *script* / *GDExtension* overrides — NOT in-engine C++ subclasses (there
// is no script instance and _get_extension() returns null for a ClassDB
// class). The GDVIRTUAL2 macro does not declare a virtual _render_callback
// method, so a plain `virtual ... override` would neither compile nor be
// called.
//
// To make this in-engine subclass receive the callback, the constructor
// re-binds the effect's callback slot to
//   callable_mp(this, &NaniteGDExtBridge::_render_callback)
// via RenderingServer::compositor_effect_set_callback(). The renderer then
// invokes our method directly (see RendererSceneRenderRD::_process_compositor_effects).
class NaniteGDExtBridge : public CompositorEffect, public INaniteBridge {
	GDCLASS(NaniteGDExtBridge, CompositorEffect);

private:
	NaniteServer *server = nullptr; // weak pointer; set in install()

protected:
	static void _bind_methods();

public:
	// Default constructor (used by ClassDB): defaults to PRE_OPAQUE.
	NaniteGDExtBridge();

	// Construct with a specific callback type (used by NaniteServer::init).
	explicit NaniteGDExtBridge(CompositorEffect::EffectCallbackType p_callback_type);

	// CompositorEffect callback entry point. NOT a C++ override of a base
	// virtual (GDVIRTUAL2 does not expose one); it is registered as the
	// effect's Callable in the constructor. Declared virtual so that a
	// future subclass of NaniteGDExtBridge may override it and the bound
	// Callable still dispatches correctly.
	virtual void _render_callback(int p_effect_callback_type, const RenderData *p_render_data);

	// INaniteBridge implementation.
	virtual void install(NaniteServer *p_server) override;
	virtual ShadowMode get_shadow_mode() const override { return SHADOW_COARSE_LOD; }
	virtual void on_pre_render(const RenderData *p_render_data) override {}
	virtual void on_pre_opaque_pass(const RenderData *p_render_data) override;
	virtual void on_post_opaque_pass(const RenderData *p_render_data) override;
	virtual void on_shadow_pass(const RenderData *p_render_data, const RID &p_light, int p_pass) override {}
	virtual StringName get_bridge_name() const override { return "gdext"; }

	~NaniteGDExtBridge();
};
