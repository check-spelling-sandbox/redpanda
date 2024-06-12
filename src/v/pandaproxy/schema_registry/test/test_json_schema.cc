// Copyright 2021 Redpanda Data, Inc.
//
// Use of this software is governed by the Business Source License
// included in the file licenses/BSL.md
//
// As of the Change Date specified in that file, in accordance with
// the Business Source License, use of this software will be governed
// by the Apache License, Version 2.0

#include "pandaproxy/schema_registry/error.h"
#include "pandaproxy/schema_registry/errors.h"
#include "pandaproxy/schema_registry/exceptions.h"
#include "pandaproxy/schema_registry/json.h"
#include "pandaproxy/schema_registry/sharded_store.h"
#include "pandaproxy/schema_registry/types.h"

#include <seastar/core/sstring.hh>
#include <seastar/testing/thread_test_case.hh>
#include <seastar/util/defer.hh>

#include <boost/test/tools/context.hpp>
#include <fmt/core.h>

namespace pp = pandaproxy;
namespace pps = pp::schema_registry;

struct store_fixture {
    store_fixture() {
        store.start(pps::is_mutable::yes, ss::default_smp_service_group())
          .get();
    }
    ~store_fixture() { store.stop().get(); }

    pps::sharded_store store;
};

struct error_test_case {
    ss::sstring def;
    pps::error_info err;
    friend std::ostream&
    operator<<(std::ostream& os, error_test_case const& e) {
        fmt::print(
          os,
          "def: {}, error_code: {}, error_message: {}",
          e.def,
          e.err.code(),
          e.err.message());
        return os;
    }
};
static const auto error_test_cases = std::to_array({
  // Test invalid JSON
  error_test_case{
    R"({])",
    pps::error_info{
      pps::error_code::schema_invalid,
      "Invalid schema: Missing a name for object member. at offset 1"}},
});
SEASTAR_THREAD_TEST_CASE(test_make_invalid_json_schema) {
    for (const auto& data : error_test_cases) {
        store_fixture f;
        BOOST_TEST_CONTEXT(data) {
            BOOST_REQUIRE_EXCEPTION(
              pps::make_canonical_json_schema(
                f.store,
                {pps::subject{"test"}, {data.def, pps::schema_type::json}})
                .get(),
              pps::exception,
              [&data](pps::exception const& e) {
                  return e.code() == data.err.code()
                         && e.message() == data.err.message();
              });
        }
    };
}

static constexpr auto valid_test_cases = std::to_array<std::string_view>(
  {// primitives
   R"({"type": "number"})",
   R"({"type": "integer"})",
   R"({"type": "object"})",
   R"({"type": "array"})",
   R"({"type": "boolean"})",
   R"({"type": "null"})"});
SEASTAR_THREAD_TEST_CASE(test_make_valid_json_schema) {
    for (const auto& data : valid_test_cases) {
        store_fixture f;
        BOOST_TEST_CONTEXT(data) {
            auto s = pps::make_json_schema_definition(
              f.store,
              pps::make_canonical_json_schema(
                f.store, {pps::subject{"test"}, {data, pps::schema_type::json}})
                .get());
            BOOST_REQUIRE_NO_THROW(s.get());
        }
    };
}