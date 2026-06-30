/**************************************************************************/
/*  ai_analyzer.cpp                                                       */
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

#include "modules/insights/insights_core/ai_analyzer.h"

#include "core/io/json.h"
#include "core/math/math_funcs.h"
#include "core/object/class_db.h"
#include "core/variant/binder_common.h"
#include "core/variant/variant.h"
#include "modules/insights/insights_core/insights_database.h"

String AIAnalyzer::build_analysis_prompt(const Ref<InsightsDatabase> &p_database) const {
	String prompt = "Analyze the following performance trace data and identify bottlenecks:\n";

	if (p_database.is_null()) {
		return prompt;
	}

	Array zones = p_database->query_zones_in_range(0, UINT64_MAX);

	// Collect zone names and durations into a sortable structure.
	struct ZoneDuration {
		String name;
		double duration_ms;
	};

	LocalVector<ZoneDuration> zone_durations;
	for (int i = 0; i < zones.size(); i++) {
		Dictionary zone = zones[i];
		String name = zone["name"];
		uint64_t start_ns = (uint64_t)(int64_t)zone["start_ns"];
		uint64_t end_ns = (uint64_t)(int64_t)zone["end_ns"];
		uint64_t duration_ns = end_ns - start_ns;
		double duration_ms = (double)duration_ns / 1000000.0;

		ZoneDuration zd;
		zd.name = name;
		zd.duration_ms = duration_ms;
		zone_durations.push_back(zd);
	}

	// Sort by duration descending (longest first) using simple selection sort.
	for (uint32_t i = 0; i < zone_durations.size(); i++) {
		uint32_t max_idx = i;
		for (uint32_t j = i + 1; j < zone_durations.size(); j++) {
			if (zone_durations[j].duration_ms > zone_durations[max_idx].duration_ms) {
				max_idx = j;
			}
		}
		if (max_idx != i) {
			SWAP(zone_durations[i], zone_durations[max_idx]);
		}
	}

	// Format the prompt.
	for (uint32_t i = 0; i < zone_durations.size(); i++) {
		prompt += vformat("  - %s: %.4fms\n", zone_durations[i].name, zone_durations[i].duration_ms);
	}

	return prompt;
}

Dictionary AIAnalyzer::parse_response(const String &p_json_response) {
	Dictionary result_dict;

	Variant parsed = JSON::parse_string(p_json_response);
	if (parsed.get_type() != Variant::DICTIONARY) {
		result_dict["bottleneck"] = String();
		result_dict["suggestions"] = Array();
		result_dict["severity"] = (int)SEVERITY_LOW;
		return result_dict;
	}

	Dictionary data = parsed;

	// Extract bottleneck.
	String bottleneck = data.get("bottleneck", String());
	last_result.bottleneck = bottleneck;

	// Extract suggestions.
	Array suggestions_array = data.get("suggestions", Array());
	last_result.suggestions.clear();
	for (int i = 0; i < suggestions_array.size(); i++) {
		last_result.suggestions.push_back(suggestions_array[i]);
	}

	// Extract severity.
	String severity_str = data.get("severity", "low");
	severity_str = severity_str.to_lower();
	if (severity_str == "critical") {
		last_result.severity = SEVERITY_CRITICAL;
	} else if (severity_str == "high") {
		last_result.severity = SEVERITY_HIGH;
	} else if (severity_str == "medium") {
		last_result.severity = SEVERITY_MEDIUM;
	} else {
		last_result.severity = SEVERITY_LOW;
	}

	result_dict["bottleneck"] = last_result.bottleneck;
	Array result_suggestions;
	for (uint32_t i = 0; i < last_result.suggestions.size(); i++) {
		result_suggestions.push_back(last_result.suggestions[i]);
	}
	result_dict["suggestions"] = result_suggestions;
	result_dict["severity"] = (int)last_result.severity;

	return result_dict;
}

Dictionary AIAnalyzer::get_last_result() const {
	Dictionary result;
	result["bottleneck"] = last_result.bottleneck;
	Array suggestions;
	for (uint32_t i = 0; i < last_result.suggestions.size(); i++) {
		suggestions.push_back(last_result.suggestions[i]);
	}
	result["suggestions"] = suggestions;
	result["severity"] = (int)last_result.severity;
	return result;
}

void AIAnalyzer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("build_analysis_prompt", "database"), &AIAnalyzer::build_analysis_prompt);
	ClassDB::bind_method(D_METHOD("parse_response", "json_response"), &AIAnalyzer::parse_response);
	ClassDB::bind_method(D_METHOD("get_last_result"), &AIAnalyzer::get_last_result);

	BIND_ENUM_CONSTANT(SEVERITY_LOW);
	BIND_ENUM_CONSTANT(SEVERITY_MEDIUM);
	BIND_ENUM_CONSTANT(SEVERITY_HIGH);
	BIND_ENUM_CONSTANT(SEVERITY_CRITICAL);
}

AIAnalyzer::AIAnalyzer() {
}

AIAnalyzer::~AIAnalyzer() {
}

VARIANT_ENUM_CAST(AIAnalyzer::Severity);
