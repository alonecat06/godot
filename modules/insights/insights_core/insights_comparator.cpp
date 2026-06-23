/**************************************************************************/
/*  insights_comparator.cpp                                               */
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

#include "insights_comparator.h"

#include "core/math/math_funcs.h"

Dictionary InsightsComparator::compute_diff(const Ref<InsightsDatabase> &p_db_a, const Ref<InsightsDatabase> &p_db_b) const {
	InsightsDiff diff;

	if (p_db_a.is_null() || p_db_b.is_null()) {
		Dictionary empty;
		empty["regressions"] = Array();
		empty["improvements"] = Array();
		empty["new_zones"] = Array();
		empty["removed_zones"] = Array();
		empty["total_cpu_time_delta"] = (int64_t)0;
		empty["peak_memory_delta"] = (int64_t)0;
		return empty;
	}

	// Query all zones from both databases.
	Array zones_a = p_db_a->query_zones_in_range(0, UINT64_MAX);
	Array zones_b = p_db_b->query_zones_in_range(0, UINT64_MAX);

	// Aggregate total time per zone name for baseline (db_a).
	HashMap<String, int64_t> baseline_totals;
	int64_t total_cpu_a = 0;
	for (int i = 0; i < zones_a.size(); i++) {
		Dictionary zone = zones_a[i];
		String name = zone["name"];
		int64_t start_ns = (int64_t)zone["start_ns"];
		int64_t end_ns = (int64_t)zone["end_ns"];
		int64_t duration = end_ns - start_ns;
		if (!baseline_totals.has(name)) {
			baseline_totals[name] = 0;
		}
		baseline_totals[name] += duration;
		total_cpu_a += duration;
	}

	// Aggregate total time per zone name for current (db_b).
	HashMap<String, int64_t> current_totals;
	int64_t total_cpu_b = 0;
	for (int i = 0; i < zones_b.size(); i++) {
		Dictionary zone = zones_b[i];
		String name = zone["name"];
		int64_t start_ns = (int64_t)zone["start_ns"];
		int64_t end_ns = (int64_t)zone["end_ns"];
		int64_t duration = end_ns - start_ns;
		if (!current_totals.has(name)) {
			current_totals[name] = 0;
		}
		current_totals[name] += duration;
		total_cpu_b += duration;
	}

	diff.total_cpu_time_delta = total_cpu_b - total_cpu_a;

	// Compute peak memory delta from allocations.
	Array allocs_a = p_db_a->query_allocations_in_range(0, UINT64_MAX);
	Array allocs_b = p_db_b->query_allocations_in_range(0, UINT64_MAX);

	int64_t peak_mem_a = 0;
	HashMap<int64_t, int64_t> live_allocs_a;
	for (int i = 0; i < allocs_a.size(); i++) {
		Dictionary alloc = allocs_a[i];
		int64_t ptr = (int64_t)alloc["ptr"];
		int64_t size = (int64_t)alloc["size"];
		int64_t free_ns = (int64_t)alloc["free_ns"];
		if (free_ns == 0) {
			live_allocs_a[ptr] = size;
		}
	}
	for (const KeyValue<int64_t, int64_t> &E : live_allocs_a) {
		peak_mem_a += E.value;
	}

	int64_t peak_mem_b = 0;
	HashMap<int64_t, int64_t> live_allocs_b;
	for (int i = 0; i < allocs_b.size(); i++) {
		Dictionary alloc = allocs_b[i];
		int64_t ptr = (int64_t)alloc["ptr"];
		int64_t size = (int64_t)alloc["size"];
		int64_t free_ns = (int64_t)alloc["free_ns"];
		if (free_ns == 0) {
			live_allocs_b[ptr] = size;
		}
	}
	for (const KeyValue<int64_t, int64_t> &E : live_allocs_b) {
		peak_mem_b += E.value;
	}

	diff.peak_memory_delta = peak_mem_b - peak_mem_a;

	// Match zones by name and classify.
	for (const KeyValue<String, int64_t> &E : current_totals) {
		const String &name = E.key;
		int64_t current_time = E.value;

		if (baseline_totals.has(name)) {
			int64_t baseline_time = baseline_totals[name];
			int64_t time_increase = current_time - baseline_time;

			ZoneDiff zone_diff;
			zone_diff.name = name;
			zone_diff.time_increase_ns = time_increase;

			if (baseline_time > 0) {
				zone_diff.percentage_change = (double)time_increase / (double)baseline_time;
			} else if (current_time > 0) {
				zone_diff.percentage_change = 1.0;
			} else {
				zone_diff.percentage_change = 0.0;
			}

			Dictionary dict;
			dict["name"] = zone_diff.name;
			dict["time_increase_ns"] = zone_diff.time_increase_ns;
			dict["percentage_change"] = zone_diff.percentage_change;

			if (time_increase > 0) {
				diff.regressions.push_back(dict);
			} else if (time_increase < 0) {
				diff.improvements.push_back(dict);
			}
		} else {
			// Zone only in db_b (new zone).
			Dictionary dict;
			dict["name"] = name;
			dict["time_increase_ns"] = current_time;
			dict["percentage_change"] = 1.0;
			diff.new_zones.push_back(dict);
		}
	}

	// Find zones only in baseline (removed zones).
	for (const KeyValue<String, int64_t> &E : baseline_totals) {
		if (!current_totals.has(E.key)) {
			Dictionary dict;
			dict["name"] = E.key;
			dict["time_increase_ns"] = -E.value;
			dict["percentage_change"] = -1.0;
			diff.removed_zones.push_back(dict);
		}
	}

	// Convert InsightsDiff to Dictionary.
	Dictionary result;
	result["regressions"] = diff.regressions;
	result["improvements"] = diff.improvements;
	result["new_zones"] = diff.new_zones;
	result["removed_zones"] = diff.removed_zones;
	result["total_cpu_time_delta"] = diff.total_cpu_time_delta;
	result["peak_memory_delta"] = diff.peak_memory_delta;

	return result;
}

Array InsightsComparator::highlight_regressions(const Dictionary &p_diff, double p_threshold) const {
	Array result;
	Array regressions = p_diff["regressions"];
	for (int i = 0; i < regressions.size(); i++) {
		Dictionary reg = regressions[i];
		double pct = reg["percentage_change"];
		if (pct >= p_threshold) {
			result.push_back(reg);
		}
	}
	return result;
}

String InsightsComparator::generate_summary(const Dictionary &p_diff) const {
	String summary;

	summary += "=== Insights Diff Summary ===\n";

	// Total CPU time delta.
	int64_t cpu_delta = p_diff["total_cpu_time_delta"];
	if (cpu_delta > 0) {
		summary += vformat("Total CPU time: +%d ns (regression)\n", cpu_delta);
	} else if (cpu_delta < 0) {
		summary += vformat("Total CPU time: %d ns (improvement)\n", cpu_delta);
	} else {
		summary += "Total CPU time: no change\n";
	}

	// Peak memory delta.
	int64_t mem_delta = p_diff["peak_memory_delta"];
	if (mem_delta > 0) {
		summary += vformat("Peak memory: +%d bytes (regression)\n", mem_delta);
	} else if (mem_delta < 0) {
		summary += vformat("Peak memory: %d bytes (improvement)\n", mem_delta);
	} else {
		summary += "Peak memory: no change\n";
	}

	// Regressions.
	Array regressions = p_diff["regressions"];
	summary += vformat("Regressions: %d\n", regressions.size());
	for (int i = 0; i < regressions.size(); i++) {
		Dictionary reg = regressions[i];
		summary += vformat("  - %s: %+d ns (%.1f%%)\n",
				String(reg["name"]),
				(int64_t)reg["time_increase_ns"],
				(double)reg["percentage_change"] * 100.0);
	}

	// Improvements.
	Array improvements = p_diff["improvements"];
	summary += vformat("Improvements: %d\n", improvements.size());
	for (int i = 0; i < improvements.size(); i++) {
		Dictionary imp = improvements[i];
		summary += vformat("  - %s: %+d ns (%.1f%%)\n",
				String(imp["name"]),
				(int64_t)imp["time_increase_ns"],
				(double)imp["percentage_change"] * 100.0);
	}

	// New zones.
	Array new_zones = p_diff["new_zones"];
	summary += vformat("New zones: %d\n", new_zones.size());
	for (int i = 0; i < new_zones.size(); i++) {
		Dictionary nz = new_zones[i];
		summary += vformat("  - %s (%d ns)\n",
				String(nz["name"]),
				(int64_t)nz["time_increase_ns"]);
	}

	// Removed zones.
	Array removed_zones = p_diff["removed_zones"];
	summary += vformat("Removed zones: %d\n", removed_zones.size());
	for (int i = 0; i < removed_zones.size(); i++) {
		Dictionary rz = removed_zones[i];
		summary += vformat("  - %s\n", String(rz["name"]));
	}

	return summary;
}

void InsightsComparator::_bind_methods() {
	ClassDB::bind_method(D_METHOD("compute_diff", "db_a", "db_b"), &InsightsComparator::compute_diff);
	ClassDB::bind_method(D_METHOD("highlight_regressions", "diff", "threshold"), &InsightsComparator::highlight_regressions, DEFVAL(0.2));
	ClassDB::bind_method(D_METHOD("generate_summary", "diff"), &InsightsComparator::generate_summary);
}
