#include "rtcp_client/wire.hpp"

#include <array>
#include <cstring>
#include <cstdio>
#include <string_view>

namespace {
unsigned assertions = 0;
#define CHECK(x) do { ++assertions; if (!(x)) { std::printf("FAIL line=%d expr=%s\n", __LINE__, #x); return 1; } } while (0)

void p64(std::uint8_t *p, std::uint64_t value) {
  for (int i = 7; i >= 0; --i) { p[i] = std::uint8_t(value); value >>= 8; }
}
void p16(std::uint8_t *p, std::uint16_t value) {
  p[0] = std::uint8_t(value >> 8); p[1] = std::uint8_t(value);
}
}

int main() {
  using namespace rtcp_client::wire;
  std::array<std::uint8_t, 512> bytes{};
  CHECK(full_control_capability_request(bytes) == 1 && bytes[0] == 16);
  CHECK(source_state_request(bytes) == 1 && bytes[0] == 18);
  CHECK(menu_state_request(bytes) == 1 && bytes[0] == 21);
  CHECK(catalog_query(CatalogCollection::favorites, 5, 5, bytes) == 5);
  CHECK(bytes[0] == 23 && bytes[1] == 2 && bytes[2] == 0 &&
        bytes[3] == 5 && bytes[4] == 5);
  CHECK(!catalog_query(CatalogCollection::favorites, 0, 6, bytes));
  CHECK(source_select(0x0102030405060708ULL, Source::bluetooth, 9, bytes) == 18);
  const std::array<std::uint8_t, 18> expected{
      20,1,2,3,4,5,6,7,8,2,0,0,0,0,0,0,0,9};
  CHECK(std::equal(expected.begin(), expected.end(), bytes.begin()));
  CHECK(!source_select(1, Source::none, 9, bytes));
  CHECK(!source_select(1, Source::radio, 0, bytes));

  const std::array<std::uint8_t, 20> capabilities{
      17,1,0,0,0,0,0,1,7,0xd0,0,0,0,0,0,0,0,0,0,1};
  FullControlCapabilities capability{};
  CHECK(parse(capabilities, capability));
  CHECK(capability.negotiated && capability.source_switch() &&
        capability.freshness_ms == 2000 && capability.revision == 1);
  auto invalid_capability = capabilities;
  invalid_capability[7] = 2;
  CHECK(!parse(invalid_capability, capability));
  auto menu_capability = capabilities;
  menu_capability[2] = 1; menu_capability[7] = 3; menu_capability[19] = 2;
  CHECK(parse(menu_capability, capability));
  CHECK(capability.source_switch() && capability.menu_projection() &&
        capability.revision == 2);
  auto catalog_capability = capabilities;
  catalog_capability[2] = 2; catalog_capability[7] = 15;
  catalog_capability[19] = 3;
  CHECK(parse(catalog_capability, capability));
  CHECK(capability.source_switch() && capability.menu_projection() &&
        capability.catalog_query() && capability.catalog_command() &&
        capability.revision == 3);

  std::array<std::uint8_t, 24> state{};
  state[0] = 19; state[1] = 1; state[2] = 2; state[3] = 0; state[4] = 1;
  p64(state.data() + 8, 11); p64(state.data() + 16, 7);
  SourceState value{};
  CHECK(parse(state, value));
  CHECK(value.source == Source::bluetooth && value.bluetooth_connected &&
        value.sequence == 11 && value.generation == 7);
  state[2] = 1;
  CHECK(!parse(state, value)); // radio cannot claim a Bluetooth connection
  state[4] = 0;
  CHECK(parse(state, value) && value.source == Source::radio);

  std::array<std::uint8_t, 45> menu{};
  menu[0]=22;menu[1]=1;menu[2]=std::uint8_t(MenuMode::favorites);
  p16(menu.data()+4,1);p16(menu.data()+6,2);p16(menu.data()+8,0);menu[10]=2;
  p64(menu.data()+12,12);p64(menu.data()+20,4);
  p16(menu.data()+28,0);menu[30]=0;menu[31]=5;
  std::memcpy(menu.data()+32,"Alpha",5);
  p16(menu.data()+37,1);menu[39]=1;menu[40]=4;
  std::memcpy(menu.data()+41,"Beta",4);
  MenuState menu_value{};
  CHECK(parse(menu,menu_value));
  CHECK(menu_value.mode==MenuMode::favorites&&menu_value.selected==1&&
        menu_value.window_count==2&&menu_value.items[1].active&&
        std::string_view(menu_value.items[1].name.data())=="Beta");
  auto invalid_menu=menu;invalid_menu[39]=2;
  CHECK(!parse(invalid_menu,menu_value));
  invalid_menu=menu;invalid_menu[38]=2;
  CHECK(!parse(invalid_menu,menu_value));

  std::array<std::uint8_t,46> catalog{};
  catalog[0]=24;catalog[1]=1;catalog[2]=std::uint8_t(CatalogCollection::favorites);
  catalog[3]=0;p16(catalog.data()+4,7);p16(catalog.data()+6,5);
  catalog[8]=2;catalog[9]=10;p64(catalog.data()+12,18);p64(catalog.data()+20,12);
  p16(catalog.data()+28,5);catalog[30]=0;catalog[31]=5;
  std::memcpy(catalog.data()+32,"Alpha",5);
  p16(catalog.data()+37,6);catalog[39]=1;catalog[40]=5;
  std::memcpy(catalog.data()+41,"B\xc3\xa9ta",5);
  CatalogPage page{};
  CHECK(parse(catalog,page));
  CHECK(page.collection==CatalogCollection::favorites&&page.start==5&&
        page.item_count==2&&page.items[1].active&&
        std::string_view(page.items[1].name.data())=="B\xc3\xa9ta");
  auto invalid_catalog=catalog;invalid_catalog[39]=2;
  CHECK(!parse(invalid_catalog,page));
  CHECK(catalog_command(7,CatalogOperation::remove,CatalogCollection::favorites,
                        6,12,bytes)==21);
  CHECK(bytes[0]==25&&bytes[8]==7&&bytes[9]==2&&bytes[10]==2&&
        bytes[11]==0&&bytes[12]==6&&bytes[20]==12);

  const std::array<std::uint8_t,16> discovery{
      12,1,0,0x3f,96,127,7,0xd0,0,0,0,0,0,0,0,1};
  BtxCapabilities btx{};
  CHECK(parse(discovery,btx));
  auto reserved = discovery; reserved[3] |= 0x80;
  CHECK(!parse(reserved,btx));
  const std::array<std::uint8_t,15> identity_bytes{
      2,1,1,0,10,'1','.','1','.','1','2','+','f','c','1'};
  Identity identity{};
  CHECK(parse(identity_bytes,identity) && identity.full_control_advertised());
  auto identity2=identity_bytes;identity2.back()='2';
  CHECK(parse(identity2,identity) && identity.full_control_advertised());
  auto identity3=identity_bytes;identity3.back()='3';
  CHECK(parse(identity3,identity) && identity.full_control_advertised());
  auto legacy_identity=identity_bytes;legacy_identity[4]=6;
  CHECK(parse({legacy_identity.data(),11},identity)&&!identity.full_control_advertised());
  std::printf("FULL_CONTROL: PASS=1 FAIL=0 assertions=%u\n", assertions);
  return 0;
}
