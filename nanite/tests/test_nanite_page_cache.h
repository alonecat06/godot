#pragma once

#include "nanite/core/nanite_page_cache.h"
#include "tests/test_macros.h"

#include <climits>

namespace TestNanitePageCache {

// Stage 1: NanitePageCache is always-resident (no streaming).
// Verify the placeholder behavior holds so callers can rely on it.

TEST_CASE("[Nanite][PageCache] stage1_always_resident") {
	NanitePageCache cache;

	// Every page ID should be reported as resident.
	CHECK(cache.request_page(0) == true);
	CHECK(cache.request_page(1) == true);
	CHECK(cache.request_page(42) == true);
	CHECK(cache.request_page(UINT64_MAX) == true);

	// No eviction in Stage 1; evict_lru() is a no-op and shouldn't
	// change the (sentinel) resident count.
	cache.evict_lru();
	CHECK(cache.get_resident_count() == INT_MAX);
}

} // namespace TestNanitePageCache
