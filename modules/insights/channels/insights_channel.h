/**************************************************************************/
/*  insights_channel.h                                                    */
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

#include "core/math/color.h"
#include "core/object/object.h"
#include "core/string/string_name.h"
#include "core/string/ustring.h"

class InsightsChannel : public Object {
	GDCLASS(InsightsChannel, Object);

public:
	enum ChannelCategory {
		CHANNEL_CATEGORY_CPU,
		CHANNEL_CATEGORY_GPU,
		CHANNEL_CATEGORY_MEMORY,
		CHANNEL_CATEGORY_SCRIPT,
		CHANNEL_CATEGORY_LOADING,
		CHANNEL_CATEGORY_NETWORK,
		CHANNEL_CATEGORY_LOG,
		CHANNEL_CATEGORY_CUSTOM,
	};

private:
	StringName name;
	Color color;
	ChannelCategory category = CHANNEL_CATEGORY_CPU;
	bool is_enabled = true;

protected:
	static void _bind_methods();

public:
	void set_name(const StringName &p_name);
	StringName get_name() const;

	void set_color(const Color &p_color);
	Color get_color() const;

	void set_category(ChannelCategory p_category);
	ChannelCategory get_category() const;

	void set_is_enabled(bool p_enabled);
	bool get_is_enabled() const;

	virtual void on_event(const Dictionary &p_event_data);
	virtual Dictionary serialize();

	static bool validate_zone_name(const String &p_name);
	static ChannelCategory get_category_from_zone(const String &p_zone_name);
	static Color get_category_color(ChannelCategory p_category);

	InsightsChannel();
	virtual ~InsightsChannel();
};
