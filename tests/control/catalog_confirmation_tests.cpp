#include "control_home/catalog_confirmation.hpp"

#include <cassert>
#include <cstdio>

using namespace control_home;

CatalogSnapshot page(CatalogCollection collection, std::uint16_t token,
                     bool active, std::uint64_t revision = 8) {
  CatalogSnapshot value{};
  value.connected = true;
  value.supported = true;
  value.collection = collection;
  value.sequence = 1;
  value.revision = revision;
  value.count = 1;
  value.item_count = 1;
  value.items[0].token = token;
  value.items[0].active = active;
  return value;
}

int main() {
  CatalogPlaybackConfirmation confirmation;

  // Immediate acknowledgement followed by authoritative confirmation.
  confirmation.begin(CatalogCollection::FAVORITES, 2, 7, 0, 100);
  confirmation.accepted();
  assert(confirmation.observe(true, page(CatalogCollection::FAVORITES, 2, true)) ==
         CatalogConfirmationDecision::CONFIRMED);

  // Playback may start before the transport drops. Reconnect only queries;
  // it never repeats the mutation.
  confirmation.begin(CatalogCollection::FAVORITES, 2, 7, 0, 1000);
  confirmation.disconnected();
  assert(confirmation.connected() == CatalogConfirmationDecision::QUERY);
  assert(confirmation.connected() == CatalogConfirmationDecision::NONE);
  confirmation.query_started();
  assert(confirmation.observe(true, page(CatalogCollection::FAVORITES, 2, true)) ==
         CatalogConfirmationDecision::CONFIRMED);

  // A different active item and stopped playback both refuse success.
  confirmation.begin(CatalogCollection::FAVORITES, 2, 7, 0, 2000);
  assert(confirmation.observe(true, page(CatalogCollection::FAVORITES, 2, false)) ==
         CatalogConfirmationDecision::FAILED);
  confirmation.begin(CatalogCollection::FAVORITES, 2, 7, 0, 3000);
  assert(confirmation.observe(false, page(CatalogCollection::FAVORITES, 2, true)) ==
         CatalogConfirmationDecision::FAILED);

  // Older revisions and unrelated collections cannot confirm the operation.
  confirmation.begin(CatalogCollection::FAVORITES, 2, 7, 1, 4000);
  assert(confirmation.observe(true, page(CatalogCollection::FAVORITES, 2, true, 8)) ==
         CatalogConfirmationDecision::NONE);
  auto fresh = page(CatalogCollection::FAVORITES, 2, true, 6);
  fresh.sequence = 2;
  assert(confirmation.observe(true, fresh) ==
         CatalogConfirmationDecision::NONE);
  fresh = page(CatalogCollection::RECENT, 2, true, 8);
  fresh.sequence = 2;
  assert(confirmation.observe(true, fresh) ==
         CatalogConfirmationDecision::NONE);

  // The lifecycle is bounded and a new operation replaces only local state.
  assert(confirmation.tick(4000 + CatalogPlaybackConfirmation::deadline_ms) ==
         CatalogConfirmationDecision::EXPIRED);
  confirmation.begin(CatalogCollection::SAVED, 1, 9, 0, 5000);
  confirmation.begin(CatalogCollection::FAVORITES, 3, 10, 0, 5001);
  assert(confirmation.active && confirmation.collection == CatalogCollection::FAVORITES &&
         confirmation.token == 3 && !confirmation.command_accepted);

  std::puts("Catalog playback confirmation: PASS");
}
