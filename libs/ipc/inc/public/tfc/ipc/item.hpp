#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <units/generic/angle.h>
#include <units/isq/si/area.h>
#include <units/isq/si/length.h>
#include <units/isq/si/mass.h>
#include <units/isq/si/volume.h>
#include <glaze/core/common.hpp>

#include <tfc/stx/basic_fixed_string.hpp>
#include <tfc/utils/units_glaze_meta.hpp>

namespace tfc::ipc::item {

namespace details {
enum struct category_e : std::uint8_t {
  unknown = 0,
  box,
  tub,
  bag,
  pallet,
  fish,
  meat,
  poultry,
  ice,
  // to be continued
};
enum struct quality_e : std::uint8_t {
  unknown = 0,
  inferior = 10,  // (lowest grade)
  weak = 20,
  ordinary = 30,
  exceptional = 40,
  superior = 50,  // (highest grade)
};
struct color {
  std::uint8_t red{ 0 };
  std::uint8_t green{ 0 };
  std::uint8_t blue{ 0 };
};
struct supplier {
  std::string name{};
  std::string contact_info{};
  std::string country{};  // todo should this be an enum
};
struct destination {
  // todo decide what destination can be
};
}  // namespace details

// Food and Agriculture Organization of the United Nations
// Reference: https://www.fao.org/fishery/en/collection/asfis/en
namespace fao {
struct species {
  bool outside_spec{ false };  // if struct is used for outside the specification, represented with `!` as first character
  tfc::stx::basic_fixed_string<char, 3> code{};

  static constexpr auto from_int(std::uint16_t input) -> std::optional<species> {
    species res{};
    if (input >= offset) {
      res.outside_spec = true;
      input -= offset;
    }
    unsigned int cnt{ 3 };
    while (input > 0 && cnt > 0) {
      --cnt;
      res.code[cnt] = alphabet[input % alphabet.size()];
      input /= alphabet.size();
    }
    if (input != 0) {
      // remainder is left so the input was too big
      return std::nullopt;
    }
    return res;
  }
  [[nodiscard]] constexpr auto to_int() const noexcept -> std::uint16_t {
    static constexpr auto impl{ [](auto& input) -> std::uint16_t {
      static constexpr auto const_toupper{ [](char character) -> char {
        if ('a' <= character && character <= 'z') {
          return static_cast<char>(character - ('a' - 'A'));
        }
        return character;
      } };
      std::uint16_t res = 0;
      static_assert(alphabet.size() == 27);
      for (auto const& character : input) {
        res *= alphabet.size();
        res += const_toupper(character) - 'A';
      }
      return res;
    } };
    if (outside_spec) {
      return offset + impl(code);
    }
    return impl(code);
  }
  constexpr auto operator==(species const& rhs) const noexcept -> bool = default;

  static constexpr std::string_view alphabet{ "ABCDEFGHIJKLMNOPQRSTUVWXYZ!" };  // note the ending !
  // Todo remake labelled database, this offset is the result of a database labelling error, as in the - 1
  // todo this can overlap !!!
  static constexpr std::uint16_t offset{ (alphabet.size() - 1) * (alphabet.size() - 1) * (alphabet.size() - 1) };
};
inline constexpr auto atlantic_cod{ species{ .code{ "COD" } } };
inline constexpr auto atlantic_herring{ species{ .code{ "HER" } } };
inline constexpr auto haddock{ species{ .code{ "HAD" } } };
inline constexpr auto european_sprat{ species{ .code{ "SPR" } } };
inline constexpr auto whiting{ species{ .code{ "WHG" } } };
inline constexpr auto blue_whiting{ species{ .code{ "WHB" } } };
inline constexpr auto red_gurnard{ species{ .code{ "GUR" } } };
inline constexpr auto boarfish{ species{ .code{ "BOC" } } };
inline constexpr auto mackerels_nei{ species{ .code{ "MAX" } } };
inline constexpr auto european_hake{ species{ .code{ "HKE" } } };
inline constexpr auto pollack{ species{ .code{ "POL" } } };
inline constexpr auto atlantic_halibut{ species{ .code{ "HAL" } } };
inline constexpr auto atlantic_horse_mackerel{ species{ .code{ "HOM" } } };
inline constexpr auto norway_pout{ species{ .code{ "NOP" } } };
inline constexpr auto greater_argentine{ species{ .code{ "ARU" } } };
inline constexpr auto atlantic_salmon{ species{ .code{ "SAL" } } };
// outside spec
inline constexpr auto incomplete{ species{ .outside_spec = true, .code{ "ICP" } } };
inline constexpr auto damaged{ species{ .outside_spec = true, .code{ "DMG" } } };
inline constexpr auto single{ species{ .outside_spec = true, .code{ "SNG" } } };
inline constexpr auto double_{ species{ .outside_spec = true, .code{ "DBL" } } };
inline constexpr auto unsure{ species{ .outside_spec = true, .code{ "UNS" } } };
inline constexpr auto empty{ species{ .outside_spec = true, .code{ "EMP" } } };
inline constexpr auto gigolo{ species{ .outside_spec = true, .code{ "GIG" } } };  // example half of a fish in an image
inline constexpr auto garbage{ species{ .outside_spec = true, .code{ "GAR" } } };

namespace test {
// notice that if you pass into from_int an integer outside the scope of the parser a nullopt is returned
static_assert(species::from_int(std::numeric_limits<std::uint16_t>::max()) == std::nullopt);

// 25 is Z and 27 is the alphabet size
static_assert(species{ .outside_spec = false, .code{ "ZZZ" } }.to_int() == (25 * 27 + 25) * 27 + 25);  // 18925
// THIS below should be true but is not because of the issue mentioned in lower part of struct
// static_assert(species::from_int(species::offset-1) == species{.outside_spec=false, .code{"ZZZ"}});
// This will result in overlapping if you are not careful
static_assert(species::from_int(species::offset - 1) == species{ .outside_spec = false, .code{ "YCZ" } });

static_assert(atlantic_cod.to_int() == 1839);
static_assert(atlantic_cod == species::from_int(atlantic_cod.to_int()));
static_assert(red_gurnard.to_int() == 4931);
static_assert(red_gurnard == species::from_int(red_gurnard.to_int()));
static_assert(damaged.to_int() == 20093);
static_assert(damaged == species::from_int(damaged.to_int()));
static_assert(gigolo.to_int() == 22172);
static_assert(gigolo == species::from_int(gigolo.to_int()));

}  // namespace test

}  // namespace fao

namespace si = units::isq::si;

/// \struct item
/// \brief given attributes of an item
struct item {
  static constexpr auto make() {
    // todo generate uuid
    // todo generate entry time point
    return item{};
  }

  // ids
  std::optional<std::string> item_id{ std::nullopt };
  std::optional<std::string> batch_id{ std::nullopt };
  std::optional<std::string> barcode{ std::nullopt };
  std::optional<std::string> qr_code{ std::nullopt };

  // type
  std::optional<details::category_e> category{ std::nullopt };
  std::optional<fao::species> fao_species{ std::nullopt };
  std::optional<std::string> sub_type{ std::nullopt };  // any specification of the given type

  // dimensions
  std::optional<si::mass<si::milligram, std::uint64_t>> item_weight{ std::nullopt };
  std::optional<si::mass<si::milligram, std::uint64_t>> target_weight{ std::nullopt };
  std::optional<si::mass<si::milligram, std::uint64_t>> min_weight{ std::nullopt };
  std::optional<si::mass<si::milligram, std::uint64_t>> max_weight{ std::nullopt };
  std::optional<si::length<si::millimetre, std::uint64_t>> length{ std::nullopt };
  std::optional<si::length<si::millimetre, std::uint64_t>> width{ std::nullopt };
  std::optional<si::length<si::millimetre, std::uint64_t>> height{ std::nullopt };
  std::optional<si::area<si::square_millimetre, std::uint64_t>> area{ std::nullopt };
  std::optional<si::volume<si::cubic_millimetre, std::uint64_t>> volume{ std::nullopt };
  // TODO temperature in celsius
  std::optional<units::angle<units::degree, double>> angle{ std::nullopt };

  // attributes
  std::optional<details::color> color{ std::nullopt };
  std::optional<details::quality_e> quality{ std::nullopt };
  std::optional<std::chrono::system_clock::time_point> entry_timestamp{ std::nullopt };
  std::optional<std::chrono::system_clock::time_point> production_date{ std::nullopt };
  std::optional<std::chrono::system_clock::time_point> expiration_date{ std::nullopt };
  std::optional<std::string> description{ std::nullopt };
  std::optional<details::supplier> supplier{ std::nullopt };
  std::optional<details::destination> destination{ std::nullopt };

  // item may contain other items
  std::optional<std::vector<item>> items{ std::nullopt };
};

}  // namespace tfc::ipc::item

namespace glz {

template <>
struct meta<tfc::ipc::item::details::category_e> {
  using enum tfc::ipc::item::details::category_e;
  static constexpr auto name{ "item_category" };
  // clang-format off
  static constexpr auto value{ glz::enumerate("unknown", unknown,
                                              "box", box,
                                              "tub", tub,
                                              "bag", bag,
                                              "pallet", pallet,
                                              "fish", fish,
                                              "meat", meat,
                                              "poultry", poultry,
                                              "ice", ice
                                              ) };
  // clang-format on
};
template <>
struct meta<tfc::ipc::item::details::quality_e> {
  using enum tfc::ipc::item::details::quality_e;
  static constexpr auto name{ "item_quality" };
  // clang-format off
  static constexpr auto value{ glz::enumerate("unknown", unknown,
                                              "inferior", inferior,
                                              "weak", weak,
                                              "ordinary", ordinary,
                                              "exceptional", exceptional,
                                              "superior", superior
                                              ) };
  // clang-format on
};
template <>
struct meta<tfc::ipc::item::details::color> {
  using type = tfc::ipc::item::details::color;
  static constexpr auto name{ "item_color" };
  // clang-format off
  static constexpr auto value{ glz::object("red", &type::red, "Red value 0-255",
                                           "green", &type::green, "Green value 0-255",
                                           "blue", &type::blue, "Blue value 0-255"
                                          ) };
  // clang-format on
};
template <>
struct meta<tfc::ipc::item::details::supplier> {
  using type = tfc::ipc::item::details::supplier;
  static constexpr auto name{ "item_supplier" };
  // clang-format off
  static constexpr auto value{ glz::object("name", &type::name, "Supplier name",
                                           "contact_info", &type::contact_info, "Supplier contact information",
                                           "origin", &type::country, "Supplier country"
                                           ) };
  // clang-format on
};
template <>
struct meta<tfc::ipc::item::fao::species> {
  using type = tfc::ipc::item::fao::species;
  static constexpr auto name{ "fao_species" };
  // clang-format off
  static constexpr auto value{ glz::object("code", &type::code, "3 letter food and agriculture organization code",
                                           "outside_spec", &type::outside_spec, "Code is not according to FAO"
                                           ) };
  // clang-format on
};

template <>
struct meta<tfc::ipc::item::item> {
  using type = tfc::ipc::item::item;
  static constexpr auto name{ "ipc_item" };
  // clang-format off
  static constexpr auto value{ glz::object("id", &type::item_id, "Unique id of this item",
                                           "batch_id", &type::batch_id, "Unique id of this batch",
                                           "barcode", &type::barcode, "Unique barcode of this item",
                                           "qr_code", &type::qr_code, "Unique QR code of this item",
                                           "category", &type::category, "Item category",
                                           "fao_species", &type::fao_species, "Food and agriculture organization species code",
                                           "sub_type", &type::sub_type, "More specific type related information",
                                           "item_weight", &type::item_weight, "Weight of item",
                                           "target_weight", &type::target_weight, "Presumed weight of item",
                                           "min_weight", &type::min_weight, "Minimum acceptable weight of item",
                                           "max_weight", &type::max_weight, "Maximum acceptable weight of item",
                                           "length", &type::length, "Length of item",
                                           "width", &type::width, "Width of item",
                                           "height", &type::height, "Height of item",
                                           "area", &type::area, "Area of item",
                                           "volume", &type::volume, "Volume of item",
//                                           "temperature", &type::temperature, "Temperature in celsius",
                                           "angle", &type::angle, "Angle of item in its place",
                                           "color", &type::color, "RGB color value",
                                           "quality", &type::quality, "Quality/grade of item",
                                           "entry_timestamp", &type::entry_timestamp, "First entry timestamp of item appearing in system",
                                           "production_date", &type::production_date, "Production date of item",
                                           "expiration_date", &type::expiration_date, "Expiration date of item",
                                           "description", &type::description, "Description of item, some kind of metadata",
                                           "supplier", &type::supplier, "Supplier information of item",
//                                           "destination", &type::destination, "Routing destination of item",
                                           "items", &type::items, "List of owning items, like tub of 100 fishes"
                                           ) };
  // clang-format on
};

}  // namespace glz