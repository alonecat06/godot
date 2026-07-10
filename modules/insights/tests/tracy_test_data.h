/**************************************************************************/
/*  tracy_test_data.h                                                     */
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

#ifdef TRACY_SERVER_ENABLED

#include "modules/insights/tracy_server/TracyWorker.hpp"
#include "modules/insights/tracy_server/TracyFileWrite.hpp"

#include "core/error/error_list.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"

#include <algorithm>
#include <memory>
#include <vector>

namespace InsightsTest {

// Generate a .tracy file with test data containing CPU zones, messages, and plots.
// Returns OK on success, or an error code on failure.
Error generate_test_tracy_file(const String &p_output_path) {
	// 1. Create ImportEventTimeline entries for CPU zones.
	std::vector<tracy::Worker::ImportEventTimeline> timeline;

	// Thread 1 (Main): "godot:physics/step" zone from 1000000 to 2000000 ns.
	timeline.push_back({ /*tid=*/1, /*timestamp=*/1000000, /*name=*/"godot:physics/step", /*text=*/"", /*isEnd=*/false, /*locFile=*/"physics_step.cpp", /*locLine=*/42 });
	timeline.push_back({ /*tid=*/1, /*timestamp=*/2000000, /*name=*/"", /*text=*/"", /*isEnd=*/true, /*locFile=*/"", /*locLine=*/0 });

	// Thread 1 (Main): "godot:render/forward" zone from 2100000 to 4000000 ns.
	timeline.push_back({ /*tid=*/1, /*timestamp=*/2100000, /*name=*/"godot:render/forward", /*text=*/"", /*isEnd=*/false, /*locFile=*/"render_forward.cpp", /*locLine=*/87 });
	timeline.push_back({ /*tid=*/1, /*timestamp=*/4000000, /*name=*/"", /*text=*/"", /*isEnd=*/true, /*locFile=*/"", /*locLine=*/0 });

	// Thread 2 (Worker): "godot:loading/resource" zone from 500000 to 3500000 ns.
	timeline.push_back({ /*tid=*/2, /*timestamp=*/500000, /*name=*/"godot:loading/resource", /*text=*/"", /*isEnd=*/false, /*locFile=*/"resource_loader.cpp", /*locLine=*/156 });
	timeline.push_back({ /*tid=*/2, /*timestamp=*/3500000, /*name=*/"", /*text=*/"", /*isEnd=*/true, /*locFile=*/"", /*locLine=*/0 });

	// 2. Create ImportEventMessages entries.
	std::vector<tracy::Worker::ImportEventMessages> messages;

	// Frame markers on Thread 1.
	messages.push_back({ /*tid=*/1, /*timestamp=*/100000, /*message=*/"frame" });
	messages.push_back({ /*tid=*/1, /*timestamp=*/17000000, /*message=*/"frame" });

	// Test message on Thread 1.
	messages.push_back({ /*tid=*/1, /*timestamp=*/1000000, /*message=*/"Test message from Godot" });

	// 3. Create ImportEventPlots entries.
	std::vector<tracy::Worker::ImportEventPlots> plots;

	// Plot "FPS" with 10 data points from 0ms to 166ms, values around 60.
	{
		tracy::Worker::ImportEventPlots fps_plot;
		fps_plot.name = "FPS";
		fps_plot.format = tracy::PlotValueFormatting::Number;
		for (int i = 0; i < 10; i++) {
			int64_t ts = static_cast<int64_t>(i) * 16666666; // ~16.6ms apart (60 FPS)
			double val = 60.0 + (i % 3 - 1) * 0.5; // Values around 60
			fps_plot.data.emplace_back(ts, val);
		}
		plots.push_back(std::move(fps_plot));
	}

	// Plot "Memory" with 5 data points showing increasing memory.
	{
		tracy::Worker::ImportEventPlots mem_plot;
		mem_plot.name = "Memory";
		mem_plot.format = tracy::PlotValueFormatting::Memory;
		for (int i = 0; i < 5; i++) {
			int64_t ts = static_cast<int64_t>(i) * 4000000000LL; // 4 seconds apart
			double val = 256.0 * 1024 * 1024 + static_cast<double>(i) * 50.0 * 1024 * 1024; // 256MB increasing by 50MB
			mem_plot.data.emplace_back(ts, val);
		}
		plots.push_back(std::move(mem_plot));
	}

	// 4. Create thread names map.
	std::unordered_map<uint64_t, std::string> threadNames;
	threadNames[1] = "Main";
	threadNames[2] = "Worker";

	// 5. Sort timeline and messages by timestamp (stable_sort).
	std::stable_sort(timeline.begin(), timeline.end(),
			[](const tracy::Worker::ImportEventTimeline &a, const tracy::Worker::ImportEventTimeline &b) {
				return a.timestamp < b.timestamp;
			});

	std::stable_sort(messages.begin(), messages.end(),
			[](const tracy::Worker::ImportEventMessages &a, const tracy::Worker::ImportEventMessages &b) {
				return a.timestamp < b.timestamp;
			});

	// 6. Baseline time to 0 (subtract min timestamp from all timestamps).
	uint64_t min_ts = UINT64_MAX;
	for (const auto &ev : timeline) {
		min_ts = std::min(min_ts, ev.timestamp);
	}
	for (const auto &ev : messages) {
		min_ts = std::min(min_ts, ev.timestamp);
	}
	for (const auto &plot : plots) {
		for (const auto &dp : plot.data) {
			min_ts = std::min(min_ts, static_cast<uint64_t>(dp.first));
		}
	}

	if (min_ts > 0 && min_ts != UINT64_MAX) {
		for (auto &ev : timeline) {
			ev.timestamp -= min_ts;
		}
		for (auto &ev : messages) {
			ev.timestamp -= min_ts;
		}
		for (auto &plot : plots) {
			for (auto &dp : plot.data) {
				dp.first -= static_cast<int64_t>(min_ts);
			}
		}
	}

	// 7. Construct tracy::Worker using Import API constructor.
	tracy::Worker worker("Godot", "Godot Engine", timeline, messages, plots, threadNames);

	// 8. Write to file.
	auto w = std::unique_ptr<tracy::FileWrite>(tracy::FileWrite::Open(p_output_path.utf8().get_data(), tracy::FileCompression::Fast));
	if (!w) {
		return ERR_FILE_CANT_WRITE;
	}
	worker.Write(*w, false);

	// 9. Return OK on success.
	return OK;
}

} // namespace InsightsTest

#endif // TRACY_SERVER_ENABLED
