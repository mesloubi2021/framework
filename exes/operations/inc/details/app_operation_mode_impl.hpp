#include "app_operation_mode.hpp"
#include "state_machine_owner.hpp"

#include <string>

#include <boost/asio/io_context.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>

#include <tfc/dbus/sd_bus.hpp>
#include <tfc/dbus/sdbusplus_meta.hpp>

namespace tfc {

template <template <typename, typename> typename signal_t, template <typename, typename> typename slot_t>
app_operation_mode<signal_t, slot_t>::app_operation_mode(boost::asio::io_context& ctx)
    : dbus_{ std::make_shared<sdbusplus::asio::connection>(ctx, tfc::dbus::sd_bus_open_system()) },
      dbus_object_server_{ std::make_unique<sdbusplus::asio::object_server>(dbus_) },
      dbus_interface_{ dbus_object_server_->add_interface(
          std::string{ operation::dbus::path.data(), operation::dbus::path.size() },
          std::string{ operation::dbus::name.data(), operation::dbus::name.size() }) },
      state_machine_{ std::make_unique<state_machine_owner_t>(ctx) }, logger_{ "app_operation_mode" } {
  dbus_interface_->register_signal<operation::update_message>(
      std::string(operation::dbus::signal::update.data(), operation::dbus::signal::update.size()));

  dbus_interface_->register_method(operation::dbus::method::set_mode.data(),
                                   [this](operation::mode_e new_mode) { set_mode(new_mode); });

  dbus_interface_->register_property_r<operation::mode_e>(
      std::string{ operation::dbus::property::mode }, sdbusplus::vtable::property_::emits_change,
      [this]([[maybe_unused]] operation::mode_e const& value) -> operation::mode_e {  // getter
        return mode_;
      });

  state_machine_->on_new_mode([this](operation::new_mode new_mode, operation::old_mode old_mode) {
    mode_ = new_mode;
    logger_.trace("sending update signal from state: '{}' to state: '{}'", enum_name(old_mode), enum_name(new_mode));
    auto message{ dbus_interface_->new_signal(operation::dbus::signal::update.data()) };
    message.append(operation::update_message{ .new_mode = new_mode, .old_mode = old_mode });
    message.signal_send();
  });

  dbus_->request_name(operation::dbus::name.data());

  dbus_interface_->initialize();

  set_mode(operation::mode_e::stopped);
}

template <template <typename, typename> typename signal_t, template <typename, typename> typename slot_t>
auto app_operation_mode<signal_t, slot_t>::set_mode(tfc::operation::mode_e new_mode) -> void {
  if (auto const err_code{ state_machine_->set_mode(new_mode) }) {
    logger_.info("Unable to set to state: '{}', error: '{}'", enum_name(new_mode), err_code.message());
    return;
  }
  logger_.trace("set_mode to state: '{}'", enum_name(new_mode));
}

}  // namespace tfc
