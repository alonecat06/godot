/**************************************************************************/
/*  nanite_resource_preview_gen.cpp                                       */
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

#ifdef TOOLS_ENABLED

#include "nanite_resource_preview_gen.h"

#include "nanite/core/nanite_resource.h"

#include "core/object/class_db.h"
#include "editor/inspector/editor_preview_plugins.h"
#include "scene/resources/mesh.h" // ArrayMesh full definition (nanite_resource.h only forward-declares it).

bool NaniteResourcePreviewGenerator::handles(const String &p_type) const {
	return p_type == "NaniteMeshResource";
}

Ref<Texture2D> NaniteResourcePreviewGenerator::generate(const Ref<Resource> &p_from, const Size2 &p_size, Dictionary &p_metadata) const {
	Ref<NaniteMeshResource> res = p_from;
	if (res.is_null()) {
		return Ref<Texture2D>();
	}

	Ref<ArrayMesh> shadow_mesh = res->get_shadow_mesh();
	if (shadow_mesh.is_null()) {
		// No shadow mesh available — no thumbnail can be rendered.
		return Ref<Texture2D>();
	}

	// Delegate to EditorMeshPreviewPlugin: it sets up its own scenario,
	// camera, lights, and a RenderingServer-side mesh instance, then renders
	// synchronously via DrawRequester::request_and_wait. This deliberately
	// does NOT start the Nanite GPU pipeline (Stage 0 has none) — it only
	// renders the coarse shadow ArrayMesh via the standard mesh preview path.
	Ref<EditorMeshPreviewPlugin> mesh_preview;
	mesh_preview.instantiate();
	return mesh_preview->generate(shadow_mesh, p_size, p_metadata);
}

#endif // TOOLS_ENABLED
