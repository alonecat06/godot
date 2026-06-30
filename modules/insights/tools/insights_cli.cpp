/**************************************************************************/
/*  insights_cli.cpp                                                      */
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

#include "insights_cli.h"

#include "modules/insights/insights_core/insights_comparator.h"
#include "modules/insights/tools/tracy_converter.h"

String InsightsCLI::last_report;

int InsightsCLI::compare_command(const String &p_baseline_path, const String &p_pr_path, double p_threshold) {
	Ref<InsightsDatabase> db_baseline;
	db_baseline.instantiate();
	Error err = db_baseline->load_from_file(p_baseline_path);
	if (err != OK) {
		last_report = vformat("Failed to open baseline database: %s", p_baseline_path);
		return 0;
	}

	Ref<InsightsDatabase> db_pr;
	db_pr.instantiate();
	err = db_pr->load_from_file(p_pr_path);
	if (err != OK) {
		last_report = vformat("Failed to open PR database: %s", p_pr_path);
		return 0;
	}

	Ref<InsightsComparator> comparator;
	comparator.instantiate();

	Dictionary diff = comparator->compute_diff(db_baseline, db_pr);
	Array regressions = comparator->highlight_regressions(diff, p_threshold);

	last_report = comparator->generate_summary(diff);

	if (regressions.size() > 0) {
		last_report += "\n=== Regressions exceeding threshold ===\n";
		for (int i = 0; i < regressions.size(); i++) {
			Dictionary reg = regressions[i];
			last_report += vformat("  - %s: %+d ns (%.1f%%)\n",
					String(reg["name"]),
					(int64_t)reg["time_increase_ns"],
					(double)reg["percentage_change"] * 100.0);
		}
		return 1;
	}

	return 0;
}

Error InsightsCLI::export_command(const String &p_gitracy_path, const String &p_format) {
	if (p_format == "tracy") {
		String tracy_path = p_gitracy_path.get_basename() + ".tracy";
		Ref<TracyConverter> converter;
		converter.instantiate();
		return converter->gitracy_to_tracy(p_gitracy_path, tracy_path);
	}

	return ERR_INVALID_PARAMETER;
}

String InsightsCLI::get_last_report() const {
	return last_report;
}

void InsightsCLI::_bind_methods() {
	ClassDB::bind_method(D_METHOD("compare_command", "baseline_path", "pr_path", "threshold"), &InsightsCLI::compare_command, DEFVAL(0.1));
	ClassDB::bind_method(D_METHOD("export_command", "gitracy_path", "format"), &InsightsCLI::export_command, DEFVAL("tracy"));
	ClassDB::bind_method(D_METHOD("get_last_report"), &InsightsCLI::get_last_report);
}
