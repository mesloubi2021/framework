#pragma once

#include <functional>
#include <string_view>
#include <type_traits>

#include <glaze/glaze.hpp>

#include <tfc/confman/detail/change.hpp>
#include <tfc/confman/detail/config_dbus_client.hpp>
#include <tfc/confman/file_storage.hpp>
#include <tfc/dbus/sdbusplus_fwd.hpp>
#include <tfc/progbase.hpp>
#include <tfc/utils/json_schema.hpp>

namespace tfc::confman {

namespace asio = boost::asio;

/// \brief configuration storage which maintains and keeps a storage type up to date
/// \tparam config_storage_t equality comparable and default constructible type
/// \tparam file_storage_t injectable template to override default behaviour of file storage
/// \tparam config_dbus_client_t injectable template to override default behaviour of dbus client
template <typename config_storage_t,
          typename file_storage_t = file_storage<config_storage_t>,
          typename config_dbus_client_t = detail::config_dbus_client>
class config {
public:
  using type = config_storage_t;
  using storage_t = config_storage_t;

  /// \brief construct config and deliver it to config manager
  /// \param ctx context ref to which the config shall run in
  /// \param key identification of this config storage, requires to be unique
  config(asio::io_context& ctx, std::string_view key) : config{ ctx, key, config_storage_t{} } {}

  /// \brief construct config and deliver it to config manager
  /// \param conn valid dbus connection
  /// \param key identification of this config storage, requires to be unique
  config(std::shared_ptr<sdbusplus::asio::connection> conn, std::string_view key)
      : config{ conn, key, config_storage_t{} } {}

  /// \brief construct config and deliver it to config manager
  /// \param ctx context ref to which the config shall run in
  /// \param key identification of this config storage, requires to be unique
  /// \param def default values of given storage type
  template <typename storage_type>
    requires std::same_as<storage_t, std::remove_cvref_t<storage_type>>
  config(asio::io_context& ctx, std::string_view key, storage_type&& def)
      : client_{ ctx, key, std::bind_front(&config::string, this), std::bind_front(&config::schema, this),
                 std::bind_front(&config::from_string, this) },
        storage_{ client_.io_context(), tfc::base::make_config_file_name(key, "json"), std::forward<storage_type>(def) },
        logger_(fmt::format("config.{}", key)) {
    client_.initialize();
    storage_.on_change([]() {
      // todo this can lead too callback hell, set property calls dbus set prop and dbus set prop calls back
      //      client_.set(detail::config_property{ .value = string(), .schema = schema() });
    });
  }

  /// \brief construct config and deliver it to config manager
  /// \param conn valid dbus connection
  /// \param key identification of this config storage, requires to be unique
  /// \param def default values of given storage type
  template <typename storage_type>
    requires std::same_as<storage_t, std::remove_cvref_t<storage_type>>
  config(std::shared_ptr<sdbusplus::asio::connection> conn, std::string_view key, storage_type&& def)
      : client_{ conn, key, std::bind_front(&config::string, this), std::bind_front(&config::schema, this),
                 std::bind_front(&config::from_string, this) },
        storage_{ client_.io_context(), tfc::base::make_config_file_name(key, "json"), std::forward<storage_type>(def) },
        logger_(fmt::format("config.{}", key)) {
    client_.initialize();
    storage_.on_change([]() {
      // todo this can lead too callback hell, set property calls dbus set prop and dbus set prop calls back
      //      client_.set(detail::config_property{ .value = string(), .schema = schema() });
    });
  }

  /// \brief Advanced constructor providing file storage interface and dbus client
  /// \param key identification of this config storage, requires to be unique
  /// \param file_storage lvalue reference to file storage implementation
  /// \param dbus_client rvalue reference to constructed dbus client
  /// \note This constructor is good for testing! Since you can disable underlying functions by the substitutions.
  config(asio::io_context&, std::string_view key, file_storage_t file_storage, config_dbus_client_t dbus_client)
      : client_{ dbus_client }, storage_{ file_storage }, logger_{ fmt::format("config.{}", key) } {
    static_assert(std::is_lvalue_reference_v<file_storage_t>);
    static_assert(std::is_lvalue_reference_v<config_dbus_client_t>);
  }

  /// \brief get const access to storage
  /// \note can be used to assign observer to observable even though it is const
  [[nodiscard]] auto value() const noexcept -> storage_t const& { return storage_.value(); }
  /// \brief accessor to given storage
  auto operator->() const noexcept -> storage_t const* { return std::addressof(value()); }

  /// \return storage_t as json string
  [[nodiscard]] auto string() const -> std::string { return glz::write_json(storage_.value()); }

  /// TODO can we do this differently, jsonforms requires object as root element
  /// an example of failure would be confman<std::vector<int>> as the json schema root element would be array
  /// which is expected but jsonforms requires object. Hopefully we can remove this later.
  template <typename to_be_wrapped_t>
  struct object_wrapper {
    to_be_wrapped_t placeholder{};
    struct glaze {
      static constexpr auto value{ glz::object("config", &object_wrapper::placeholder) };
      static constexpr auto name{ glz::name_v<to_be_wrapped_t> };
    };
  };

  /// \return storage_t json schema
  [[nodiscard]] auto schema() const -> std::string {
    return tfc::json::write_json_schema<object_wrapper<config_storage_t>>();
  }

  auto set_changed() const noexcept -> std::error_code {
    client_.set(detail::config_property{ .value = string(), .schema = schema() });
    return storage_.set_changed();
  }

  /// \brief get config key used to index the given object of type storage_t
  [[nodiscard]] auto file() const noexcept -> std::filesystem::path const& { return storage_.file(); }

  using change = detail::change<config>;

  auto make_change() noexcept -> change { return change{ *this }; }

  auto from_string(std::string_view value) -> std::error_code {
    // this will call N nr of callbacks
    // for each confman::observer type
    auto const error{ glz::read_json<storage_t>(storage_.make_change().value(), value) };
    if (error) {
      return std::make_error_code(std::errc::io_error);  // todo make glz to std::error_code
    }
    return {};
  }

protected:
  friend struct detail::change<config>;

  // todo if this could be named `value` it would be neat
  // the change mechanism relies on this (the friend above)
  // todo const_cast is not nice, make different pattern
  [[nodiscard]] auto access() noexcept -> storage_t& { return const_cast<storage_t&>(storage_.value()); }

  config_dbus_client_t client_;
  file_storage_t storage_{};
  tfc::logger::logger logger_;
};

}  // namespace tfc::confman
