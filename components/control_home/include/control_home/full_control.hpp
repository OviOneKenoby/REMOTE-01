#pragma once

#include <array>
#include <cstdint>

namespace control_home {

enum class TunerSource : std::uint8_t { UNKNOWN = 0, RADIO = 1, BLUETOOTH = 2 };
enum class SourceTransition : std::uint8_t {
  STABLE = 0,
  STARTING_BLUETOOTH = 1,
  SWITCHING_RADIO = 2,
  FAILED = 3
};

struct FullControlSnapshot {
  bool connected = false;
  bool supported = false;
  bool bluetooth_connected = false;
  bool command_pending = false;
  bool result_present = false;
  bool result_accepted = false;
  std::uint8_t last_error = 0;
  TunerSource source = TunerSource::UNKNOWN;
  TunerSource requested_source = TunerSource::UNKNOWN;
  SourceTransition transition = SourceTransition::STABLE;
  std::uint64_t source_generation = 0;
};

enum class TunerMenuMode : std::uint8_t {
  UNAVAILABLE = 0,
  STATION_SELECT = 1,
  PLAYING_RADIO = 2,
  BLUETOOTH = 3,
  BROWSE_LETTER = 4,
  BROWSE_COUNTRY = 5,
  BROWSE_STATION_LETTER = 6,
  BROWSE_TAG = 7,
  BROWSE_RESULTS = 8,
  FAVORITES = 9,
  RECENT = 10,
  IDLE = 11,
  ERROR = 12,
  OTHER = 13
};

struct MenuItemSnapshot {
  std::uint16_t token = 0;
  bool active = false;
  std::array<char, 49> name{};
};

struct MenuSnapshot {
  bool connected = false;
  bool supported = false;
  bool loading = false;
  bool failed = false;
  std::uint8_t error = 0;
  TunerMenuMode mode = TunerMenuMode::UNAVAILABLE;
  std::uint16_t selected = 0;
  std::uint16_t count = 0;
  std::uint16_t window_start = 0;
  std::uint8_t window_count = 0;
  std::uint64_t revision = 0;
  std::array<MenuItemSnapshot, 5> items{};
};

enum class CatalogCollection : std::uint8_t {
  SAVED = 1,
  FAVORITES = 2,
  RECENT = 3
};
enum class CatalogOperation : std::uint8_t {
  PLAY = 1,
  REMOVE = 2,
  ADD_FAVORITE = 3
};
struct CatalogItemSnapshot {
  std::uint16_t token = 0;
  bool active = false;
  std::array<char, 49> name{};
};
struct CatalogSnapshot {
  bool connected = false;
  bool supported = false;
  bool loading = false;
  bool command_pending = false;
  bool result_present = false;
  bool result_accepted = false;
  std::uint8_t last_error = 0;
  CatalogCollection collection = CatalogCollection::SAVED;
  CatalogOperation requested_operation = CatalogOperation::PLAY;
  std::uint16_t requested_token = 0;
  std::uint8_t flags = 0;
  std::uint8_t capacity = 0;
  std::uint8_t item_count = 0;
  std::uint16_t count = 0;
  std::uint16_t start = 0;
  std::uint64_t sequence = 0;
  std::uint64_t revision = 0;
  std::array<CatalogItemSnapshot, 5> items{};
};

bool full_control_read(FullControlSnapshot &snapshot, std::uint64_t &generation);
bool menu_read(MenuSnapshot &snapshot, std::uint64_t &generation);
bool catalog_read(CatalogSnapshot &snapshot, std::uint64_t &generation);
bool request_source(TunerSource source);
bool request_catalog(CatalogCollection collection, std::uint16_t offset);
bool request_catalog_action(CatalogOperation operation,
                            CatalogCollection collection,
                            std::uint16_t token,
                            std::uint64_t revision);

}  // namespace control_home
