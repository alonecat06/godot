/**************************************************************************/
/*  builder_config.h                                                      */
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

#include "core/object/ref_counted.h"

// BuilderConfig is a RefCounted subclass that exposes the 13 tunable
// parameters driving the NaniteBuilder offline pipeline (meshlet sizing,
// partition target, simplification ratios, page budget, shadow LOD depth,
// etc.). It is registered to ClassDB so users can construct and edit it
// from the Inspector or GDScript.
//
// Defaults match the values mandated by the stage-0 spec ("BuilderConfig
// 参数定义与校验"), and `is_valid()` returns false if any field drops
// below its spec-mandated floor.
class BuilderConfig : public RefCounted {
	GDCLASS(BuilderConfig, RefCounted);

protected:
	static void _bind_methods();

public:
	// Meshlet sizing.
	int max_vertices = 64;
	int max_triangles = 128;
	int min_triangles = 32;

	// Partition / simplification.
	int partition_size = 4;
	double cone_weight = 0.5;
	double split_factor = 0.5;
	double simplification_ratio = 0.5;
	double target_error = 0.5;
	bool lock_partition_border = true;
	int meshlet_optimize_level = 3;

	// Hierarchy / paging / shadow.
	int max_lod_levels = 16;
	int page_size_bytes = 65536;
	int shadow_lod_depth = 3;

	void set_max_vertices(int p_value);
	int get_max_vertices() const;

	void set_max_triangles(int p_value);
	int get_max_triangles() const;

	void set_min_triangles(int p_value);
	int get_min_triangles() const;

	void set_partition_size(int p_value);
	int get_partition_size() const;

	void set_cone_weight(double p_value);
	double get_cone_weight() const;

	void set_split_factor(double p_value);
	double get_split_factor() const;

	void set_simplification_ratio(double p_value);
	double get_simplification_ratio() const;

	void set_target_error(double p_value);
	double get_target_error() const;

	void set_lock_partition_border(bool p_value);
	bool get_lock_partition_border() const;

	void set_meshlet_optimize_level(int p_value);
	int get_meshlet_optimize_level() const;

	void set_max_lod_levels(int p_value);
	int get_max_lod_levels() const;

	void set_page_size_bytes(int p_value);
	int get_page_size_bytes() const;

	void set_shadow_lod_depth(int p_value);
	int get_shadow_lod_depth() const;

	// Returns true when every field meets its spec-mandated floor:
	//   max_vertices       >= 32
	//   max_triangles      >= 32
	//   partition_size     >= 2
	//   page_size_bytes    >= 4096
	//   shadow_lod_depth   >= 1
	bool is_valid() const;
};
