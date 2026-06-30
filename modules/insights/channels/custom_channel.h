/**************************************************************************/
/*  custom_channel.h                                                      */
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

#include "modules/insights/channels/insights_channel.h"
#include "core/templates/local_vector.h"

class CustomChannel : public InsightsChannel {
	GDCLASS(CustomChannel, InsightsChannel);

public:
	struct CustomZone {
		String name;
		String channel;
		String file;
		int line = 0;
		uint64_t start_ns = 0;
		uint64_t end_ns = 0;
	};

private:
	LocalVector<CustomZone> zones;
	uint32_t max_zones = 50000;

protected:
	static void _bind_methods();

public:
	void begin_zone(const String &p_name, const String &p_file = "", int p_line = 0);
	void end_zone();

	TypedArray<Dictionary> get_zones() const;

	void set_max_zones(uint32_t p_max);
	uint32_t get_max_zones() const;

	virtual void on_event(const Dictionary &p_event_data) override;
	virtual Dictionary serialize() override;

	CustomChannel();
	virtual ~CustomChannel();
};
