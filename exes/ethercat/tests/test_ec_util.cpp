#include <cstdint>
#include <type_traits>

#include <units/isq/si/si.h>
#include <units/quantity.h>
#include <boost/ut.hpp>
#include <tfc/ec/devices/schneider/atv320.hpp>
#include <tfc/ec/devices/util.hpp>

namespace ut = boost::ut;
using tfc::ec::util::setting;

namespace example {
enum struct enum_value_e : std::int8_t { on = 1, off = 2 };
using enum_setting = setting<ecx::index_t{ 0x42, 0x43 }, "name", "desc", enum_value_e, enum_value_e::on>;
using trivial_type_setting = setting<ecx::index_t{ 0x42, 0x43 }, "name", "desc", uint8_t, 13>;
static_assert(sizeof(enum_setting) == 1);
static_assert(sizeof(trivial_type_setting) == 1);

// using std chrono
using chrono_test = setting<ecx::index_t{ 0x42, 0x42 }, "name", "desc", std::chrono::milliseconds, 60>;
static_assert(sizeof(std::chrono::milliseconds) == 8);
static_assert(std::is_same_v<chrono_test::type::rep, std::chrono::milliseconds::rep>);
static_assert(std::is_same_v<chrono_test::type::period, std::milli>);
static_assert(sizeof(chrono_test) == 8);

// using mp units

using units::quantity;
using units::isq::si::dim_power;
using units::isq::si::watt;
using quantiy_test_t = quantity<dim_power, watt, uint32_t>;
static_assert(sizeof(quantiy_test_t) == 4);
using mp_test = setting<ecx::index_t{ 0x42, 0x42 }, "name", "desc", quantiy_test_t, uint32_t{ 60 }>;
static_assert(std::is_same_v<mp_test::type::rep, uint32_t>);
static_assert(std::is_same_v<mp_test::type::dimension, dim_power>);
static_assert(std::is_same_v<mp_test::type::unit, watt>);
static_assert(sizeof(mp_test) == 4);

}  // namespace example

auto main(int, char**) -> int {
  using ut::operator""_test;
  using ut::operator>>;
  using ut::expect;
  using ut::fatal;

  [[maybe_unused]] example::enum_setting const test1{};
  [[maybe_unused]] example::trivial_type_setting const test2{};
  [[maybe_unused]] example::chrono_test const test3{};
  [[maybe_unused]] example::mp_test const test4{};

  "mapping function"_test = []() {
    expect(tfc::ec::util::map(10, 0, 10, 0, 20) == 20);
    expect(tfc::ec::util::map(500, 0, 1000, 0, 20) == 10);
  };
  "setting to json"_test = []() {
    [[maybe_unused]] example::trivial_type_setting const test{};
    auto const json = glz::write_json(test);
    expect(json == "13") << "got: " << json;
    auto const exp = glz::read_json<example::trivial_type_setting>(json);
    expect(exp.has_value() >> fatal);
    expect(exp.value() == test);
  };

  "Test atv320 custom units"_test = []() {
    static_assert(std::chrono::seconds{ 1 } == tfc::ec::devices::schneider::deciseconds{ 10 });
    tfc::ec::devices::schneider::deciseconds an_hour = std::chrono::hours{ 1 };
    expect(an_hour == tfc::ec::devices::schneider::deciseconds{ 36000 });
  };
}
