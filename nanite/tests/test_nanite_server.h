#pragma once

#include "core/nanite_debug.h"
#include "core/nanite_resource.h"
#include "core/nanite_server.h"
#include "tests/test_macros.h"

namespace TestNaniteServer {

TEST_CASE("[Nanite][Server] singleton_creation_and_finish") {
	// Singleton should be created by register_types at SERVERS level.
	CHECK(NaniteServer::get_singleton() != nullptr);
}

TEST_CASE("[Nanite][Server] register_unregister_instance") {
	// Can't easily test without NaniteMeshInstance3D, but test count API.
	// The instance tests will be added in Task 1.8.
	NaniteServer *ns = NaniteServer::get_singleton();
	REQUIRE(ns != nullptr);
	// Just verify get_instance_count returns a non-negative value.
	CHECK(ns->get_instance_count() >= 0);
}

TEST_CASE("[Nanite][Server] register_mesh_returns_valid_rid_and_refcounts") {
	NaniteServer *ns = NaniteServer::get_singleton();
	REQUIRE(ns != nullptr);

	Ref<NaniteMeshResource> res;
	res.instantiate();
	REQUIRE(res.is_valid());

	// First registration must hand back a valid, non-null RID.
	RID rid_a = ns->register_mesh(res);
	CHECK(rid_a.is_valid());

	// Second registration of the same resource must return the SAME RID
	// (ref-counted, not a duplicate entry).
	RID rid_b = ns->register_mesh(res);
	CHECK(rid_b == rid_a);

	// Two unregister calls to balance the two register calls; the second
	// one should drop the ref count to zero and free the RID.
	ns->unregister_mesh(rid_a);
	ns->unregister_mesh(rid_b);

	// After full unregistration, re-registering must produce a NEW RID
	// (the old one was freed).
	RID rid_c = ns->register_mesh(res);
	CHECK(rid_c.is_valid());
	CHECK(rid_c != rid_a);
	ns->unregister_mesh(rid_c);
}

TEST_CASE("[Nanite][Server] debug_mode_roundtrip") {
	NaniteServer *ns = NaniteServer::get_singleton();
	REQUIRE(ns != nullptr);

	int original = ns->get_debug_mode();
	ns->set_debug_mode(NaniteDebug::CLUSTER_SOLID_COLOR);
	CHECK(ns->get_debug_mode() == (int)NaniteDebug::CLUSTER_SOLID_COLOR);
	// Restore so we don't leak state into other tests.
	ns->set_debug_mode(original);
}

} // namespace TestNaniteServer
