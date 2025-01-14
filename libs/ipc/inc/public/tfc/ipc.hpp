#pragma once

#include <system_error>

#include <fmt/format.h>

#include <tfc/dbus/sdbusplus_fwd.hpp>
#include <tfc/ipc/details/dbus_client_iface.hpp>
#include <tfc/ipc/details/dbus_slot.hpp>
#include <tfc/ipc/details/impl.hpp>
#include <tfc/stx/concepts.hpp>

namespace tfc::ipc {

namespace asio = boost::asio;

namespace details {
auto constexpr register_cb(std::string const& ipc_name) {
  return [ipc_name](std::error_code error_code) {
    if (error_code == std::errc::host_unreachable) {
      throw std::runtime_error(fmt::format("Error registering: '{}', error: '{}'. Maybe forgot to run ipc-ruler", ipc_name,
                                           error_code.message()));
    }
    if (error_code) {
      throw std::runtime_error(fmt::format("Error registering: '{}', error: '{}'", ipc_name, error_code.message()));
    }
  };
}
}  // namespace details

/// \brief make manager client for the types(signal, slot) below
[[maybe_unused]] static auto make_manager_client(asio::io_context& ctx) {
  return tfc::ipc_ruler::ipc_manager_client{ ctx };
}

/**
 * @brief
 * This is the receiving end for tfc's ipc communications,
 * it listens to connection changes from ipc-ruler in
 * addition with implementing the ipc connection.
 * @tparam type_desc The type description for the slot.
 */
template <typename type_desc, typename manager_client_type = tfc::ipc_ruler::ipc_manager_client&>
class slot {
public:
  using value_t = typename details::slot_callback<type_desc>::value_t;
  static constexpr std::string_view type_name{ type_desc::type_name };
  static auto constexpr direction_v = details::slot_callback<type_desc>::direction_v;

  /**
   * C'tor for a tfc IPC slot.
   * @param ctx Execution context
   * @param client manager_client_type a reference to a manager client
   * @param name The slot name
   * @param callback Channel for value updates from the corresponding signal.
   */
  slot(asio::io_context& ctx,
       manager_client_type client,
       std::string_view name,
       std::string_view description,
       tfc::stx::invocable<value_t> auto&& callback)
    requires std::is_lvalue_reference_v<manager_client_type>
      : slot_{ details::slot_callback<type_desc>::create(
            ctx,
            name,
            [this, callb = std::forward<decltype(callback)>(callback)](value_t const& new_value) {
              callb(new_value);
              dbus_slot_.emit_value(new_value);
            }) },
        dbus_slot_{ client.connection(), [this] -> std::optional<value_t> const& { return this->value(); } },
        client_{ client } {
    client_init(description);
  }

  slot(asio::io_context& ctx,
       std::shared_ptr<sdbusplus::asio::connection> connection,
       std::string_view name,
       std::string_view description,
       tfc::stx::invocable<value_t> auto&& callback)
    requires(!std::is_lvalue_reference_v<manager_client_type>)
      : slot_{ details::slot_callback<type_desc>::create(
            ctx,
            name,
            [this, callb = std::forward<decltype(callback)>(callback)](value_t const& new_value) {
              callb(new_value);
              dbus_slot_.emit_value(new_value);
            }) },
        dbus_slot_{ connection, [this] -> std::optional<value_t> const& { return this->value(); } }, client_{ connection } {
    client_init(description);
  }

  slot(asio::io_context& ctx,
       manager_client_type client,
       std::string_view name,
       tfc::stx::invocable<value_t> auto&& callback)
      : slot(ctx, client, name, "", std::forward<decltype(callback)>(callback)) {}

  slot(slot&) = delete;

  slot(slot&&) noexcept = delete;

  auto operator=(slot&&) noexcept -> slot& = delete;

  auto operator=(slot const&) -> slot& = delete;

  [[nodiscard]] auto value() const noexcept -> std::optional<value_t> const& { return slot_->value(); }

  [[nodiscard]] auto name() const noexcept -> std::string_view { return slot_->name(); }

  [[nodiscard]] auto full_name() const noexcept -> std::string { return slot_->name_w_type(); }

private:
  void client_init(std::string_view description) {
    client_.register_connection_change_callback(slot_->name_w_type(), [this](std::string_view signal_name) {
      slot_->connect(signal_name);  //
    });
    client_.register_slot(slot_->name_w_type(), description, type_desc::value_e, details::register_cb(slot_->name_w_type()));

    dbus_slot_.initialize(full_name());
  }

  std::shared_ptr<details::slot_callback<type_desc>> slot_;
  details::dbus_slot<value_t> dbus_slot_;
  manager_client_type client_;
};

/**
 * an ipc component, registers its existence with an
 * ipc-ruler service.
 * @tparam type_desc The type of the signal
 */
template <typename type_desc, typename manager_client_type>
class signal {
public:
  using value_t = typename details::signal<type_desc>::value_t;
  static constexpr std::string_view type_name{ type_desc::type_name };
  static auto constexpr direction_v = details::signal<type_desc>::direction_v;

  /**
   * Signal c'tor
   * @param ctx Execution context
   * @param name Signals name
   */
  signal(asio::io_context& ctx, manager_client_type& client, std::string_view name, std::string_view description = "")
      : client_(client) {
    auto exp{ details::signal<type_desc>::create(ctx, name) };
    if (!exp.has_value()) {
      throw std::runtime_error{ fmt::format("Unable to bind to socket, reason: {}", exp.error().message()) };
    }
    signal_ = std::move(exp.value());
    client_.register_signal(signal_->name_w_type(), description, type_desc::value_e,
                            details::register_cb(signal_->name_w_type()));
  }

  auto send(value_t const& value) -> std::error_code { return signal_->send(value); }

  template <typename completion_token_t>
  auto async_send(value_t const& value, completion_token_t&& token) -> auto {
    return signal_->async_send(value, std::forward<completion_token_t>(token));
  }

  [[nodiscard]] auto name() const noexcept -> std::string_view { return signal_->name(); }

  [[nodiscard]] auto full_name() const noexcept -> std::string { return signal_->name_w_type(); }

private:
  manager_client_type& client_;
  std::shared_ptr<details::signal<type_desc>> signal_;
};

template <typename return_t, template <typename description_t, typename manager_client_t> typename ipc_base_t>
struct make_any;

using bool_slot = slot<details::type_bool>;
using int_slot = slot<details::type_int>;
using uint_slot = slot<details::type_uint>;
using double_slot = slot<details::type_double>;
using string_slot = slot<details::type_string>;
using json_slot = slot<details::type_json>;
using any_slot = std::variant<std::monostate,  //
                              bool_slot,       //
                              int_slot,        //
                              uint_slot,       //
                              double_slot,     //
                              string_slot,     //
                              json_slot>;
/// \brief any_slot foo = make_any_slot(type_e::bool, ctx, client, "name", "description", [](bool new_state){});
using make_any_slot = make_any<any_slot, slot>;

using bool_signal = signal<details::type_bool, ipc_ruler::ipc_manager_client>;
using int_signal = signal<details::type_int, ipc_ruler::ipc_manager_client>;
using uint_signal = signal<details::type_uint, ipc_ruler::ipc_manager_client>;
using double_signal = signal<details::type_double, ipc_ruler::ipc_manager_client>;
using string_signal = signal<details::type_string, ipc_ruler::ipc_manager_client>;
using json_signal = signal<details::type_json, ipc_ruler::ipc_manager_client>;
using any_signal = std::variant<std::monostate,  //
                                bool_signal,     //
                                int_signal,      //
                                uint_signal,     //
                                double_signal,   //
                                string_signal,   //
                                json_signal>;
/// \brief any_signal foo = make_any_signal::make(type_e::bool, ctx, client, "name", "description");
using make_any_signal = make_any<any_signal, signal>;

template <typename return_t, template <typename description_t, typename manager_client_t> typename ipc_base_t>
struct make_any {
  static auto make(details::type_e type, auto&&... args) -> return_t {
    switch (type) {
      using enum details::type_e;
      case _bool:
        return ipc_base_t<details::type_bool, ipc_ruler::ipc_manager_client>{ std::forward<decltype(args)>(args)... };
      case _int64_t:
        return ipc_base_t<details::type_int, ipc_ruler::ipc_manager_client>{ std::forward<decltype(args)>(args)... };
      case _uint64_t:
        return ipc_base_t<details::type_uint, ipc_ruler::ipc_manager_client>{ std::forward<decltype(args)>(args)... };
      case _double_t:
        return ipc_base_t<details::type_double, ipc_ruler::ipc_manager_client>{ std::forward<decltype(args)>(args)... };
      case _string:
        return ipc_base_t<details::type_string, ipc_ruler::ipc_manager_client>{ std::forward<decltype(args)>(args)... };
      case _json:
        return ipc_base_t<details::type_json, ipc_ruler::ipc_manager_client>{ std::forward<decltype(args)>(args)... };
      case unknown:
        return std::monostate{};
    }
    return {};
  }
};

}  // namespace tfc::ipc
