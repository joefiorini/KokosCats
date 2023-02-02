#pragma once

#include "basic.h"

#include "dheunit/test.h"

#include <array>
#include <functional>

namespace test {
namespace basic {
using dhe::unit::Tester;

using TestFunc = std::function<void(Tester &)>;

using Basic = koko::Basic;

template <typename Run> static inline auto test(Run run) -> TestFunc {
  return [run](Tester &t) {
    Basic basic{};
    run(t, basic);
  };
}
} // namespace basic
} // namespace test
