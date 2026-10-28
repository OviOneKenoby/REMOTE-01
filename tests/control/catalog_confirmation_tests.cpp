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
  assert(confirmation.observe(true, page(CatalogCollection::FAVORITES, 2, true), 101) ==
         CatalogConfirmationDecision::CONFIRMED);

  // Playback may start before the transport drops. Reconnect only queries;
  // it never repeats the mutation.
  confirmation.begin(CatalogCollection::FAVORITES, 2, 7, 0, 1000);
  confirmation.disconnected();
  assert(confirmation.connected() == CatalogConfirmationDecision::QUERY);
  assert(confirmation.connected() == CatalogConfirmationDecision::NONE);
  confirmation.query_started();
  assert(confirmation.observe(true, page(CatalogCollection::FAVORITES, 2, true), 1001) ==
         CatalogConfirmationDecision::CONFIRMED);

  // Admission precedes a deferred stream open. Inactive/not-yet-playing
  // observations remain pending and trigger bounded read-only polling.
  confirmation.begin(CatalogCollection::FAVORITES, 2, 7, 0, 2000);
  confirmation.accepted();
  assert(confirmation.observe(true, page(CatalogCollection::FAVORITES, 2, false), 2001) ==
         CatalogConfirmationDecision::NONE);
  assert(confirmation.query_due(2500) == CatalogConfirmationDecision::NONE);
  assert(confirmation.query_due(2501) == CatalogConfirmationDecision::QUERY);
  auto pending = page(CatalogCollection::FAVORITES, 2, false);
  pending.sequence = 2;
  assert(confirmation.observe(false, pending, 2502) ==
         CatalogConfirmationDecision::NONE);
  assert(confirmation.query_due(3002) == CatalogConfirmationDecision::QUERY);
  auto confirmed = page(CatalogCollection::FAVORITES, 2, true);
  confirmed.sequence = 3;
  assert(confirmation.observe(true, confirmed, 3003) ==
         CatalogConfirmationDecision::CONFIRMED);
  confirmation.begin(CatalogCollection::FAVORITES, 2, 7, 0, 3000);
  confirmation.accepted();
  assert(confirmation.observe(false, page(CatalogCollection::FAVORITES, 2, true), 3001) ==
         CatalogConfirmationDecision::NONE);

  // Older revisions and unrelated collections cannot confirm the operation.
  confirmation.begin(CatalogCollection::FAVORITES, 2, 7, 1, 4000);
  assert(confirmation.observe(true, page(CatalogCollection::FAVORITES, 2, true, 8), 4001) ==
         CatalogConfirmationDecision::NONE);
  auto fresh = page(CatalogCollection::FAVORITES, 2, true, 6);
  fresh.sequence = 2;
  assert(confirmation.observe(true, fresh, 4002) ==
         CatalogConfirmationDecision::NONE);
  fresh = page(CatalogCollection::RECENT, 2, true, 8);
  fresh.sequence = 2;
  assert(confirmation.observe(true, fresh, 4003) ==
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
