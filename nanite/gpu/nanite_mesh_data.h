/**************************************************************************/
/*  nanite_mesh_data.h                                                    */
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

#include "core/templates/rid.h"
#include "core/templates/local_vector.h"

class RenderingDevice;
class NaniteMeshResource;

// NaniteMeshData owns the GPU SSBOs that mirror a NaniteMeshResource's
// encoded blobs. Created by NaniteServer::register_mesh on first reference
// and released when the ref count drops to zero.
//
// Stage 1: all four blobs are uploaded as static storage buffers (no
// streaming). The material_rids array mirrors the resource's materials so
// the material-resolve shader can indirect-address them.
class NaniteMeshData {
	// Pure C++ class — no GDCLASS, no Object base.
public:
	RID cluster_ssbo;
	RID vertex_ssbo;
	RID bvh_ssbo;
	RID page_ssbo;

	LocalVector<RID> material_rids;

	int ref_count = 0;
	bool gpu_uploaded = false;

	// Uploads all 4 blobs + collects material RIDs.
	// No-op if already uploaded.
	void upload_to_gpu(RenderingDevice *p_rd, const NaniteMeshResource *p_resource);

	// Frees all SSBOs. No-op if not uploaded.
	void free_gpu_resources(RenderingDevice *p_rd);

	// Getters for pipeline binding.
	RID get_cluster_ssbo() const { return cluster_ssbo; }
	RID get_vertex_ssbo() const { return vertex_ssbo; }
	RID get_bvh_ssbo() const { return bvh_ssbo; }
	RID get_page_ssbo() const { return page_ssbo; }
	bool is_gpu_uploaded() const { return gpu_uploaded; }

	NaniteMeshData() = default;
	~NaniteMeshData() = default;
};
