/**************************************************************************/
/*  resource_load_tracker.h                                               */
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
#include "core/string/ustring.h"
#include "core/templates/list.h"
#include "core/templates/hash_map.h"
#include "core/variant/variant.h"

class ResourceLoadTracker : public Object {
	GDCLASS(ResourceLoadTracker, Object);

public:
	struct LoadEvent {
		String path;
		String loader;
		uint64_t start_ns = 0;
		uint64_t end_ns = 0;
		uint64_t memory_size = 0;
		Vector<String> parent_stack;
		uint64_t thread_id = 0;
		bool failed = false;
		String error;
	};

private:
	static ResourceLoadTracker *singleton;
	List<LoadEvent> events;
	HashMap<String, List<LoadEvent>::Element *> active_loads;

protected:
	static void _bind_methods();

public:
	static ResourceLoadTracker *get_singleton() { return singleton; }

	void on_load_begin(const String &p_path, const String &p_loader);
	void on_load_end(const String &p_path, uint64_t p_size);
	void on_load_fail(const String &p_path, const String &p_error);

	TypedArray<Dictionary> get_events_in_range(uint64_t p_start_ns, uint64_t p_end_ns) const;
	TypedArray<Dictionary> get_active_loads() const;

	void clear();

	ResourceLoadTracker();
	~ResourceLoadTracker();
};
