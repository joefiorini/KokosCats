#include "dheunit/test.h"
#include "fixtures/basic-fixture.h"
#include "helpers/assertions.h"

#include <functional>

using dhe::unit::Suite;
using dhe::unit::Tester;

namespace test {
namespace basic {

struct KokoSuite : public Suite {
  KokoSuite() : Suite{"koko::Basic"} {}

  void run(Tester &t) override {
    t.run("2+2", test([](Tester &t, Basic &basic) {
            assert_that(t, basic.add(2, 2), is_equal_to(4));
          }));
  }
};

static auto _ = KokoSuite{};
} // namespace basic
} // namespace test
