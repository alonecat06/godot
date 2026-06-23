/**************************************************************************/
/*  insights_comparator.h                                                 */
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
#include "core/string/ustring.h"
#include "core/variant/dictionary.h"

#include "modules/insights/insights_core/insights_database.h"

class InsightsComparator : public RefCounted {
	GDCLASS(InsightsComparator, RefCounted);

public:
	struct ZoneDiff {
		String name;
		int64_t time_increase_ns = 0;
		double percentage_change = 0.0;
	};

	struct InsightsDiff {
		Array regressions;
		Array improvements;
		Array new_zones;
		Array removed_zones;
		int64_t total_cpu_time_delta = 0;
		int64_t peak_memory_delta = 0;
	};

	Dictionary compute_diff(const Ref<InsightsDatabase> &p_db_a, const Ref<InsightsDatabase> &p_db_b) const;
	Array highlight_regressions(const Dictionary &p_diff, double p_threshold = 0.2) const;
	String generate_summary(const Dictionary &p_diff) const;

protected:
	static void _bind_methods();
};
