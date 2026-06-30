/**************************************************************************/
/*  test_insights_phase5.h                                               */
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

#include "tests/test_macros.h"

#include "modules/insights/channels/script_channel.h"
#include "modules/insights/scripting/mono_profiler_bridge.h"
#include "modules/insights/scripting/gdscript_profiler_decorator.h"
#include "modules/insights/tools/tracy_converter.h"
#include "modules/insights/tools/insights_launcher.h"
#include "modules/insights/tools/insights_cli.h"

namespace TestInsightsPhase5 {

// 1. C# Mono 方法调用性能分析 — 验证 ScriptChannel 记录 C# 函数调用
TEST_CASE("[Insights] C# Mono method call profiling") {
	ScriptChannel *channel = memnew(ScriptChannel);

	channel->enter_function("Player.Update", "Player.cs", 42, ScriptChannel::LANGUAGE_C_SHARP);
	channel->enter_function("Player.Move", "Player.cs", 58, ScriptChannel::LANGUAGE_C_SHARP);
	channel->leave_function();
	channel->leave_function();

	TypedArray<Dictionary> records = channel->get_call_records();
	CHECK(records.size() == 2);

	Dictionary first = records[0];
	CHECK(String(first["function_name"]) == "Player.Update");
	CHECK(int(first["language"]) == ScriptChannel::LANGUAGE_C_SHARP);

	memdelete(channel);
}

// 2. C# async/coroutine zone 生命周期 — 验证挂起和恢复记录
TEST_CASE("[Insights] C# async/coroutine zone lifecycle") {
	ScriptChannel *channel = memnew(ScriptChannel);

	channel->enter_function("LoadAsync", "Loader.cs", 10, ScriptChannel::LANGUAGE_C_SHARP);
	channel->suspend_function("LoadAsync");
	channel->resume_function("LoadAsync");
	channel->leave_function();

	TypedArray<Dictionary> records = channel->get_call_records();
	CHECK(records.size() == 1);

	Dictionary rec = records[0];
	CHECK(bool(rec["was_suspended"]) == true);
	CHECK(int64_t(rec["resume_ns"]) > 0);

	memdelete(channel);
}

// 3. GDScript @profiler_zone 装饰器 — 验证装饰器解析和代码生成
TEST_CASE("[Insights] GDScript @profiler_zone decorator") {
	Ref<GDScriptProfilerDecorator> decorator;
	decorator.instantiate();
	CHECK(decorator.is_valid());

	// parse_decorator: without custom name returns empty string (use function name).
	CHECK(decorator->parse_decorator("@profiler_zone") == "");

	// parse_decorator: with custom name returns the name.
	CHECK(decorator->parse_decorator("@profiler_zone(\"custom_name\")") == "custom_name");

	// is_profiler_zone_decorator: positive case.
	CHECK(decorator->is_profiler_zone_decorator("@profiler_zone") == true);

	// is_profiler_zone_decorator: negative case.
	CHECK(decorator->is_profiler_zone_decorator("@export") == false);

	// generate_zone_code: empty zone name uses function name.
	String code_fn = decorator->generate_zone_code("_ready", "");
	CHECK(code_fn.contains("_ready"));

	// generate_zone_code: custom zone name is used.
	String code_custom = decorator->generate_zone_code("func", "custom_name");
	CHECK(code_custom.contains("custom_name"));
}

// 4. .gitracy ↔ .tracy 转换器 — 验证 TracyConverter 实例化和文件检测
TEST_CASE("[Insights] .gitracy ↔ .tracy converter") {
	Ref<TracyConverter> converter;
	converter.instantiate();
	CHECK(converter.is_valid());

	// is_tracy_file on a non-existent file returns false.
	CHECK(converter->is_tracy_file("res://nonexistent_file.tracy") == false);
}

// 5. 启动 Insights 工具 — 验证 InsightsLauncher 初始状态
TEST_CASE("[Insights] Launch with insights tool") {
	Ref<InsightsLauncher> launcher;
	launcher.instantiate();
	CHECK(launcher.is_valid());

	CHECK(launcher->is_running() == false);
	CHECK(launcher->get_port() == 8086);
}

// 6. CI 命令行工具 — 验证 InsightsCLI 初始状态
TEST_CASE("[Insights] CI command line tool") {
	Ref<InsightsCLI> cli;
	cli.instantiate();
	CHECK(cli.is_valid());

	CHECK(cli->get_last_report() == "");
}

} // namespace TestInsightsPhase5
