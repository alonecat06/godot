/**************************************************************************/
/*  tracy_converter.h                                                     */
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
/* TORT OR OTHERWISE, ARISING OUT OF OR IN CONNECTION WITH THE SOFTWARE   */
/* OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                          */
/**************************************************************************/

#pragma once

#include "core/object/ref_counted.h"

class TracyConverter : public RefCounted {
	GDCLASS(TracyConverter, RefCounted);

public:
	// Convert .gitracy → .tracy (native Tracy binary format).
	// This uses the Tracy native file protocol with LZ4/ZSTD compression
	// to produce a file that the Tracy profiler application can open directly.
	Error gitracy_to_tracy(const String &p_gitracy_path, const String &p_tracy_path) const;

	// Convert .tracy → .gitracy (Godot Insights internal format).
	Error tracy_to_gitracy(const String &p_tracy_path, const String &p_gitracy_path) const;

	// Convert .gitracy → Chrome Trace Event JSON (.json).
	// Tracy can import Chrome Trace JSON via File → Open.
	// This is the recommended way to view .gitracy data in Tracy
	// because the native .tracy binary protocol is extremely complex.
	Error gitracy_to_chrome_json(const String &p_gitracy_path, const String &p_json_path) const;

	// Check if a file is a valid Tracy capture file.
	bool is_tracy_file(const String &p_path) const;

protected:
	static void _bind_methods();
};
