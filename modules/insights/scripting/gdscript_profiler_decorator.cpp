/**************************************************************************/
/*  gdscript_profiler_decorator.cpp                                       */
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

#include "gdscript_profiler_decorator.h"

bool GDScriptProfilerDecorator::is_profiler_zone_decorator(const String &p_annotation) const {
	return p_annotation.find("@profiler_zone") >= 0;
}

String GDScriptProfilerDecorator::parse_decorator(const String &p_annotation) const {
	if (p_annotation == "@profiler_zone") {
		return String(); // Use function name as zone name.
	}

	// Match @profiler_zone("name") pattern.
	String pattern = "@profiler_zone(\"";
	int start = p_annotation.find(pattern);
	if (start < 0) {
		return String();
	}
	start += pattern.length();
	int end = p_annotation.find("\")", start);
	if (end < 0) {
		return String();
	}
	return p_annotation.substr(start, end - start);
}

String GDScriptProfilerDecorator::generate_zone_code(const String &p_function_name, const String &p_zone_name) const {
	String zone = p_zone_name.is_empty() ? p_function_name : p_zone_name;
	return vformat("GodotProfileZoneC(\"script\", \"%s\")", zone);
}

void GDScriptProfilerDecorator::_bind_methods() {
	ClassDB::bind_method(D_METHOD("parse_decorator", "annotation"), &GDScriptProfilerDecorator::parse_decorator);
	ClassDB::bind_method(D_METHOD("generate_zone_code", "function_name", "zone_name"), &GDScriptProfilerDecorator::generate_zone_code);
	ClassDB::bind_method(D_METHOD("is_profiler_zone_decorator", "annotation"), &GDScriptProfilerDecorator::is_profiler_zone_decorator);
}
