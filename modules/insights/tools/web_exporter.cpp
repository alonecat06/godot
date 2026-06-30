/**************************************************************************/
/*  web_exporter.cpp                                                      */
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

#include "modules/insights/tools/web_exporter.h"

#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/object/class_db.h"
#include "modules/insights/insights_core/insights_database.h"

Error WebExporter::export_to_html(const String &p_gitracy_path, const String &p_html_path) const {
	Ref<InsightsDatabase> db;
	db.instantiate();
	Error err = db->load_from_file(p_gitracy_path);
	if (err != OK) {
		return err;
	}

	String zone_data;
	Array zones = db->query_zones_in_range(0, UINT64_MAX);
	zone_data = "[";
	for (int i = 0; i < zones.size(); i++) {
		if (i > 0) {
			zone_data += ",";
		}
		Dictionary z = zones[i];
		zone_data += "{";
		zone_data += "\"name\":" + String(JSON::stringify(z["name"])) + ",";
		zone_data += "\"start_ns\":" + itos((int64_t)z["start_ns"]) + ",";
		zone_data += "\"end_ns\":" + itos((int64_t)z["end_ns"]) + ",";
		zone_data += "\"depth\":" + itos((int)z["depth"]) + ",";
		zone_data += "\"thread_id\":" + itos((int64_t)z["thread_id"]);
		zone_data += "}";
	}
	zone_data += "]";

	String html = _get_html_template();
	html = html.replace("/*__ZONE_DATA__*/", zone_data);

	Ref<FileAccess> f = FileAccess::open(p_html_path, FileAccess::WRITE);
	if (f.is_null()) {
		return ERR_FILE_CANT_OPEN;
	}

	CharString utf8 = html.utf8();
	f->store_buffer((const uint8_t *)utf8.get_data(), utf8.length());
	f->close();

	return OK;
}

String WebExporter::_generate_timeline_html(const Ref<InsightsDatabase> &p_db) const {
	Array zones = p_db->query_zones_in_range(0, UINT64_MAX);
	if (zones.is_empty()) {
		return "";
	}

	uint64_t total_duration = p_db->get_total_duration_ns();
	if (total_duration == 0) {
		return "";
	}

	// Find the minimum start time for offset calculation.
	int64_t min_start = (int64_t)zones[0].get("start_ns");
	for (int i = 1; i < zones.size(); i++) {
		int64_t s = (int64_t)zones[i].get("start_ns");
		if (s < min_start) {
			min_start = s;
		}
	}

	String html;
	for (int i = 0; i < zones.size(); i++) {
		Dictionary z = zones[i];
		int64_t start_ns = (int64_t)z["start_ns"];
		int64_t end_ns = (int64_t)z["end_ns"];
		int depth = (int)z["depth"];
		String name = z["name"];

		double left_pct = (double)(start_ns - min_start) / (double)total_duration * 100.0;
		double width_pct = (double)(end_ns - start_ns) / (double)total_duration * 100.0;
		if (width_pct < 0.1) {
			width_pct = 0.1;
		}

		double top_px = depth * 22.0;

		html += "<div class=\"tl-bar\" style=\"left:" + String::num(left_pct, 2) + "%;width:" + String::num(width_pct, 2) + "%;top:" + String::num(top_px, 0) + "px\" title=\"" + name + " (" + itos(end_ns - start_ns) + " ns)\">" + name + "</div>\n";
	}

	return html;
}

String WebExporter::_generate_flamegraph_html(const Ref<InsightsDatabase> &p_db) const {
	Array zones = p_db->query_zones_in_range(0, UINT64_MAX);
	if (zones.is_empty()) {
		return "";
	}

	uint64_t total_duration = p_db->get_total_duration_ns();
	if (total_duration == 0) {
		return "";
	}

	// Find the minimum start time for offset calculation.
	int64_t min_start = (int64_t)zones[0].get("start_ns");
	int max_depth = 0;
	for (int i = 1; i < zones.size(); i++) {
		int64_t s = (int64_t)zones[i].get("start_ns");
		if (s < min_start) {
			min_start = s;
		}
		int d = (int)zones[i].get("depth");
		if (d > max_depth) {
			max_depth = d;
		}
	}

	String html;
	for (int d = max_depth; d >= 0; d--) {
		Array depth_zones = p_db->query_zones_by_depth(d);
		for (int i = 0; i < depth_zones.size(); i++) {
			Dictionary z = depth_zones[i];
			int64_t start_ns = (int64_t)z["start_ns"];
			int64_t end_ns = (int64_t)z["end_ns"];
			String name = z["name"];

			double left_pct = (double)(start_ns - min_start) / (double)total_duration * 100.0;
			double width_pct = (double)(end_ns - start_ns) / (double)total_duration * 100.0;
			if (width_pct < 0.1) {
				width_pct = 0.1;
			}

			// Flame graph: deeper zones are at the bottom.
			double bottom_px = (max_depth - d) * 22.0;

			html += "<div class=\"fg-bar\" style=\"left:" + String::num(left_pct, 2) + "%;width:" + String::num(width_pct, 2) + "%;bottom:" + String::num(bottom_px, 0) + "px\" title=\"" + name + " (" + itos(end_ns - start_ns) + " ns)\">" + name + "</div>\n";
		}
	}

	return html;
}

String WebExporter::_get_html_template() const {
	return R"rawliteral(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Godot Insights Profiler</title>
<style>
* { margin: 0; padding: 0; box-sizing: border-box; }
body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; background: #1a1a2e; color: #e0e0e0; }
h1 { padding: 16px 24px; background: #16213e; color: #e94560; font-size: 20px; border-bottom: 1px solid #0f3460; }
h2 { padding: 10px 24px; background: #0f3460; color: #e94560; font-size: 16px; }
.section { margin: 16px 24px; }
#timeline-container, #flamegraph-container {
  position: relative;
  width: 100%;
  overflow-x: auto;
  background: #16213e;
  border: 1px solid #0f3460;
  border-radius: 4px;
}
#timeline { position: relative; min-height: 200px; }
#flamegraph { position: relative; min-height: 200px; }
.tl-bar, .fg-bar {
  position: absolute;
  height: 20px;
  line-height: 20px;
  font-size: 11px;
  padding: 0 4px;
  overflow: hidden;
  white-space: nowrap;
  text-overflow: ellipsis;
  border-radius: 2px;
  cursor: pointer;
  color: #fff;
}
.tl-bar { background: #e94560; border: 1px solid #c73650; }
.fg-bar { background: #533483; border: 1px solid #3d2563; }
.tl-bar:hover, .fg-bar:hover { filter: brightness(1.3); }
#tooltip {
  display: none;
  position: fixed;
  background: #0f3460;
  color: #e0e0e0;
  padding: 8px 12px;
  border-radius: 4px;
  font-size: 12px;
  pointer-events: none;
  z-index: 1000;
  border: 1px solid #e94560;
  max-width: 400px;
}
</style>
</head>
<body>
<h1>Godot Insights Profiler</h1>

<div class="section">
<h2>Timeline</h2>
<div id="timeline-container"><div id="timeline"></div></div>
</div>

<div class="section">
<h2>Flame Graph</h2>
<div id="flamegraph-container"><div id="flamegraph"></div></div>
</div>

<div id="tooltip"></div>

<script>
const zones = /*__ZONE_DATA__*/;

function findMinMax(zones) {
  let minStart = Infinity, maxEnd = 0, maxDepth = 0;
  for (const z of zones) {
    if (z.start_ns < minStart) minStart = z.start_ns;
    if (z.end_ns > maxEnd) maxEnd = z.end_ns;
    if (z.depth > maxDepth) maxDepth = z.depth;
  }
  return { minStart, maxEnd, totalDuration: maxEnd - minStart, maxDepth };
}

const colors = [
  '#e94560','#533483','#0f3460','#e94560','#1a8a5c',
  '#d4a843','#4a90d9','#c05050','#6b5b95','#88b04b'
];

function renderTimeline() {
  const el = document.getElementById('timeline');
  if (!zones.length) { el.textContent = 'No zone data.'; return; }
  const { minStart, totalDuration, maxDepth } = findMinMax(zones);
  el.style.minHeight = ((maxDepth + 1) * 22 + 8) + 'px';
  for (const z of zones) {
    const left = ((z.start_ns - minStart) / totalDuration * 100).toFixed(2);
    const width = Math.max(((z.end_ns - z.start_ns) / totalDuration * 100), 0.1).toFixed(2);
    const top = z.depth * 22;
    const bar = document.createElement('div');
    bar.className = 'tl-bar';
    bar.style.left = left + '%';
    bar.style.width = width + '%';
    bar.style.top = top + 'px';
    bar.style.background = colors[z.depth % colors.length];
    bar.textContent = z.name;
    bar.title = z.name + ' (' + (z.end_ns - z.start_ns) + ' ns)';
    el.appendChild(bar);
  }
}

function renderFlamegraph() {
  const el = document.getElementById('flamegraph');
  if (!zones.length) { el.textContent = 'No zone data.'; return; }
  const { minStart, totalDuration, maxDepth } = findMinMax(zones);
  el.style.minHeight = ((maxDepth + 1) * 22 + 8) + 'px';
  for (const z of zones) {
    const left = ((z.start_ns - minStart) / totalDuration * 100).toFixed(2);
    const width = Math.max(((z.end_ns - z.start_ns) / totalDuration * 100), 0.1).toFixed(2);
    const bottom = (maxDepth - z.depth) * 22;
    const bar = document.createElement('div');
    bar.className = 'fg-bar';
    bar.style.left = left + '%';
    bar.style.width = width + '%';
    bar.style.bottom = bottom + 'px';
    bar.style.background = colors[z.depth % colors.length];
    bar.textContent = z.name;
    bar.title = z.name + ' (' + (z.end_ns - z.start_ns) + ' ns)';
    el.appendChild(bar);
  }
}

renderTimeline();
renderFlamegraph();
</script>
</body>
</html>)rawliteral";
}

void WebExporter::_bind_methods() {
	ClassDB::bind_method(D_METHOD("export_to_html", "gitracy_path", "html_path"), &WebExporter::export_to_html);
}
