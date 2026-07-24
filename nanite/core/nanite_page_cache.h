/**************************************************************************/
/*  nanite_page_cache.h                                                   */
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

#include "core/object/object.h"

// NanitePageCache owns the streaming page residency state for the
// runtime. In Stage 1 there is no streaming — every NaniteMeshResource
// is uploaded up-front and all of its pages are permanently resident,
// so request_page() always succeeds and evict_lru() is a no-op.
//
// TODO Stage 2+: implement a real LRU-based page cache backed by a
// GPU buffer ring with async upload fences and per-page ref counts.
class NanitePageCache : public Object {
	GDCLASS(NanitePageCache, Object);

protected:
	static void _bind_methods();

public:
	// Stage 1: always returns true (all pages resident).
	bool request_page(uint64_t p_page_id);

	// Stage 1: no-op (no eviction).
	void evict_lru();

	// Stage 1: returns sentinel value (no real tracking).
	int get_resident_count() const;

	NanitePageCache() = default;
	~NanitePageCache() = default;
};
