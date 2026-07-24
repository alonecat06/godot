/**************************************************************************/
/*  nanite_bridge.h                                                       */
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

// INaniteBridge is the abstract interface implemented by each Stage's
// bridge (Stage 1: NaniteGDExtBridge, Stage 2: NaniteModuleBridge,
// Stage 3: NaniteDeepBridge). It lets NaniteServer invoke rendering
// callbacks without depending on the concrete bridge type.
//
// This header MUST stay free of heavy engine includes so it can be
// included from nanite_server.h without pulling in CompositorEffect etc.
// Use forward declarations for RenderData / NaniteServer / RID.

#include "core/string/string_name.h"
#include "core/templates/rid.h"

class NaniteServer;
class RenderData;
struct RID;

// INaniteBridge — abstract bridge interface.
// Not a Godot Object subclass (no GDCLASS) — it is owned by NaniteServer
// via a raw pointer and destroyed in finish().
class INaniteBridge {
public:
	enum ShadowMode {
		SHADOW_COARSE_LOD, // Stage 1: use shadow_mesh via mesh_set_shadow_mesh
		SHADOW_DYNAMIC_GPU, // Stage 2/3: per-light GPU shadow rasterization
	};

	virtual ~INaniteBridge() = default;

	// Called once after construction to install the bridge onto the server.
	// Implementations should set the shadow mode and register any callbacks.
	virtual void install(NaniteServer *p_server) = 0;

	// Returns the shadow strategy this bridge provides.
	virtual ShadowMode get_shadow_mode() const = 0;

	// Render callbacks invoked by the bridge's own hook (e.g.
	// CompositorEffect::_render_callback for Stage 1). NaniteServer
	// delegates the heavy lifting; the bridge only decides WHEN.
	virtual void on_pre_render(const RenderData *p_render_data) = 0;
	virtual void on_pre_opaque_pass(const RenderData *p_render_data) = 0;
	virtual void on_post_opaque_pass(const RenderData *p_render_data) = 0;
	virtual void on_shadow_pass(const RenderData *p_render_data, const RID &p_light, int p_pass) = 0;

	// Human-readable bridge identifier (e.g. "gdext" / "module" / "deep").
	virtual StringName get_bridge_name() const = 0;
};
