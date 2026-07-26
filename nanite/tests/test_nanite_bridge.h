#pragma once

#include "nanite/core/nanite_bridge.h"
#include "tests/test_macros.h"

#include <type_traits>

namespace TestNaniteBridge {

TEST_CASE("[Nanite][Bridge] inanitebridge_is_abstract") {
	static_assert(!std::is_constructible<INaniteBridge>::value,
			"INaniteBridge must be abstract (has pure virtual methods)");
	CHECK(true); // static_assert above is the real test
}

TEST_CASE("[Nanite][Bridge] shadow_mode_enum_values") {
	CHECK((int)INaniteBridge::SHADOW_COARSE_LOD == 0);
	CHECK((int)INaniteBridge::SHADOW_DYNAMIC_GPU == 1);
}

} // namespace TestNaniteBridge
