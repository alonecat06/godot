/**************************************************************************/
/*  test_builder_config.h                                                 */
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

#include "tests/test_macros.h"

#include "../core/builder_config.h"

namespace TestBuilderConfig {

TEST_CASE("[BuilderConfig] Default is valid") {
	BuilderConfig cfg;
	CHECK(cfg.is_valid());
}

TEST_CASE("[BuilderConfig] max_triangles=0 rejected") {
	BuilderConfig cfg;
	cfg.set_max_triangles(0);
	CHECK_FALSE(cfg.is_valid());
}

TEST_CASE("[BuilderConfig] max_vertices=0 rejected") {
	BuilderConfig cfg;
	cfg.set_max_vertices(0);
	CHECK_FALSE(cfg.is_valid());
}

TEST_CASE("[BuilderConfig] partition_size=1 rejected") {
	BuilderConfig cfg;
	cfg.set_partition_size(1);
	CHECK_FALSE(cfg.is_valid());
}

TEST_CASE("[BuilderConfig] All getters/setters round-trip") {
	BuilderConfig cfg;

	cfg.set_max_vertices(128);
	cfg.set_max_triangles(256);
	cfg.set_min_triangles(64);
	cfg.set_partition_size(8);
	cfg.set_cone_weight(0.25);
	cfg.set_split_factor(0.75);
	cfg.set_simplification_ratio(0.33);
	cfg.set_target_error(0.05);
	cfg.set_lock_partition_border(false);
	cfg.set_meshlet_optimize_level(2);
	cfg.set_max_lod_levels(8);
	cfg.set_page_size_bytes(131072);
	cfg.set_shadow_lod_depth(5);

	CHECK_EQ(cfg.get_max_vertices(), 128);
	CHECK_EQ(cfg.get_max_triangles(), 256);
	CHECK_EQ(cfg.get_min_triangles(), 64);
	CHECK_EQ(cfg.get_partition_size(), 8);
	CHECK_EQ(cfg.get_cone_weight(), doctest::Approx(0.25));
	CHECK_EQ(cfg.get_split_factor(), doctest::Approx(0.75));
	CHECK_EQ(cfg.get_simplification_ratio(), doctest::Approx(0.33));
	CHECK_EQ(cfg.get_target_error(), doctest::Approx(0.05));
	CHECK_FALSE(cfg.get_lock_partition_border());
	CHECK_EQ(cfg.get_meshlet_optimize_level(), 2);
	CHECK_EQ(cfg.get_max_lod_levels(), 8);
	CHECK_EQ(cfg.get_page_size_bytes(), 131072);
	CHECK_EQ(cfg.get_shadow_lod_depth(), 5);
}

TEST_CASE("[BuilderConfig] page_size_bytes=4095 rejected") {
	BuilderConfig cfg;
	cfg.set_page_size_bytes(4095);
	CHECK_FALSE(cfg.is_valid());
}

TEST_CASE("[BuilderConfig] shadow_lod_depth=0 rejected") {
	BuilderConfig cfg;
	cfg.set_shadow_lod_depth(0);
	CHECK_FALSE(cfg.is_valid());
}

} // namespace TestBuilderConfig
