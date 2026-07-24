/**************************************************************************/
/*  nanite_gdext_bridge.cpp                                               */
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

#include "bridge/nanite_gdext_bridge.h"

#include "core/nanite_server.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "servers/rendering/rendering_server.h"

void NaniteGDExtBridge::_bind_methods() {
}

NaniteGDExtBridge::NaniteGDExtBridge() :
		NaniteGDExtBridge(CompositorEffect::EFFECT_CALLBACK_TYPE_PRE_OPAQUE) {
	// Delegating constructor; the body runs after the target constructor.
}

NaniteGDExtBridge::NaniteGDExtBridge(CompositorEffect::EffectCallbackType p_callback_type) {
	// set_effect_callback_type stores the type on this Resource and, when a
	// RenderingServer is available, registers
	//   callable_mp(this, &CompositorEffect::_call_render_callback)
	// as the effect's Callable. That base method is non-virtual and only
	// forwards to GDVIRTUAL2(_render_callback, ...), which does NOT reach
	// in-engine C++ subclasses (no script instance, no GDExtension binding).
	// We therefore re-bind the slot to our own _render_callback so the
	// renderer (RendererSceneRenderRD::_process_compositor_effects) invokes
	// it directly. In headless/test builds RenderingServer is null and both
	// calls are no-ops; the object is still a valid CompositorEffect.
	set_effect_callback_type(p_callback_type);

	RenderingServer *rs = RenderingServer::get_singleton();
	if (rs != nullptr && get_rid().is_valid()) {
		rs->compositor_effect_set_callback(
				get_rid(),
				RSE::CompositorEffectCallbackType(p_callback_type),
				callable_mp(this, &NaniteGDExtBridge::_render_callback));
	}
}

void NaniteGDExtBridge::_render_callback(int p_effect_callback_type, const RenderData *p_render_data) {
	(void)p_effect_callback_type; // unused; we dispatch on get_effect_callback_type()
	NaniteServer *ns = NaniteServer::get_singleton();
	if (ns == nullptr) {
		return;
	}
	CompositorEffect::EffectCallbackType cb = get_effect_callback_type();
	if (cb == CompositorEffect::EFFECT_CALLBACK_TYPE_PRE_OPAQUE) {
		ns->render_visibility(p_render_data);
	} else if (cb == CompositorEffect::EFFECT_CALLBACK_TYPE_POST_OPAQUE) {
		ns->render_material_resolve(p_render_data);
	}
	// Other callback types are ignored: Nanite only runs at PRE_OPAQUE /
	// POST_OPAQUE.
}

void NaniteGDExtBridge::install(NaniteServer *p_server) {
	server = p_server;
	if (server != nullptr) {
		server->set_shadow_mode(SHADOW_COARSE_LOD);
	}
}

void NaniteGDExtBridge::on_pre_opaque_pass(const RenderData *p_render_data) {
	NaniteServer *ns = NaniteServer::get_singleton();
	if (ns) {
		ns->render_visibility(p_render_data);
	}
}

void NaniteGDExtBridge::on_post_opaque_pass(const RenderData *p_render_data) {
	NaniteServer *ns = NaniteServer::get_singleton();
	if (ns) {
		ns->render_material_resolve(p_render_data);
	}
}

NaniteGDExtBridge::~NaniteGDExtBridge() {
}
