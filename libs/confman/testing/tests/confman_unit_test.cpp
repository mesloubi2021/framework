#include <gmock/gmock.h>
#include <boost/asio.hpp>
#include <boost/ut.hpp>

#include <tfc/confman.hpp>
#include <tfc/confman/observable.hpp>
#include <tfc/mocks/confman/detail/config_dbus_client.hpp>
#include <tfc/mocks/confman/file_storage.hpp>
#include <tfc/progbase.hpp>

namespace ut = boost::ut;
namespace asio = boost::asio;

using ut::operator""_test;
using tfc::confman::observable;

struct storage {
  observable<int> a{ 33 };
  observable<int> b{ 44 };
  observable<std::string> c{ "c" };
};

template <>
struct glz::meta<storage> {
  static constexpr auto value{ glz::object("a", &storage::a, "b", &storage::b, "c", &storage::c) };
};

struct config_test {
  asio::io_context ctx{};
  using mock_file_storage_t = testing::NiceMock<tfc::confman::mock_file_storage<storage>>;
  mock_file_storage_t mock_file_storage{ ctx, "bar" };
  using mock_dbus_config_client_t = testing::NiceMock<tfc::confman::detail::config_dbus_client>;
  mock_dbus_config_client_t mock_dbus_client{ ctx };
  tfc::confman::config<storage, mock_file_storage_t&, mock_dbus_config_client_t&> const config{ ctx, "foo",
                                                                                                mock_file_storage,
                                                                                                mock_dbus_client };
};

auto main(int argc, char** argv) -> int {
  tfc::base::init(argc, argv);

  "set changed"_test = [] {
    config_test const test{};
    auto presumed_err{ std::make_error_code(std::errc::bad_message) };
    ON_CALL(test.mock_file_storage, set_changed()).WillByDefault(testing::Return(presumed_err));
    EXPECT_CALL(test.mock_file_storage, set_changed()).Times(1);
    auto returned_err{ test.config.set_changed() };
    ut::expect(returned_err == presumed_err);
  };

  return static_cast<int>(boost::ut::cfg<>.run({ .report_errors = true }));
}
