#include "artwork/model.hpp"
#include "artwork/online.hpp"

#include <cassert>
#include <algorithm>
#include <array>
#include <cstring>
#include <cstdint>
#include <iostream>
#include <span>
#include <string_view>

int main() {
  using artwork::Source;
  using artwork::Presentation;
  static_assert(artwork::authoritative_source(false, 1) == Source::neutral);
  static_assert(artwork::authoritative_source(true, 0) == Source::neutral);
  static_assert(artwork::authoritative_source(true, 1) == Source::radio);
  static_assert(artwork::authoritative_source(true, 2) == Source::bluetooth);
  static_assert(artwork::authoritative_source(true, 99) == Source::neutral);
  static_assert(artwork::presentation(true) == Presentation::image);
  static_assert(artwork::presentation(false) == Presentation::text_fallback);

  artwork::Model model;
  assert(model.source == Source::neutral);
  const auto initial = model.revision;
  assert(!model.observe(false, 1));             // disconnected never leaks RADIO
  assert(model.observe(true, 1));               // authenticated RADIO
  assert(model.source == Source::radio);
  assert(!model.observe(true, 1));              // ordinary refresh does no work
  assert(model.observe(true, 2));               // RADIO -> BLUETOOTH
  assert(model.source == Source::bluetooth);
  assert(model.observe(false, 2));              // disconnect -> neutral
  assert(model.source == Source::neutral);
  assert(model.observe(true, 2));               // reconnect restores authority
  assert(model.observe(true, 1));               // BLUETOOTH -> RADIO
  assert(model.revision == initial + 5);

  using namespace artwork::online;
  Metadata bt{true, Source::bluetooth, 7, 3, 8, 9,
              "Phone", "Daft Punk", "Get Lucky", nullptr};
  Identity identity{};
  assert(make_identity(bt, 1, identity));
  assert(std::string_view(identity.normalized_artist.data()) == "daft punk");
  assert(std::string_view(identity.normalized_title.data()) == "get lucky");
  Metadata radio{true, Source::radio, 7, 3, 8, 10,
                 "West City Radio", nullptr, nullptr, "Daft Punk - Get Lucky"};
  Identity radio_identity{};
  assert(make_identity(radio, 2, radio_identity));
  assert(std::string_view(radio_identity.artist.data()) == "Daft Punk");
  assert(std::string_view(radio_identity.title.data()) == "Get Lucky");
  auto same = identity; ++same.request_generation;
  assert(same_media(identity, same));
  radio.combined = "Station ident - Listen live";
  assert(!make_identity(radio, 3, radio_identity));
  radio.combined = "Artist - Title - Remix";
  assert(!make_identity(radio, 4, radio_identity));
  radio.combined = "No separator";
  assert(!make_identity(radio, 5, radio_identity));

  Candidate exact{98, {}, {}, {}};
  std::strcpy(exact.artist.data(), "Daft Punk");
  std::strcpy(exact.title.data(), "Get Lucky");
  std::strcpy(exact.release_group.data(), "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee");
  std::array<char, mbid_capacity> selected{};
  assert(select_release_group(identity, std::span(&exact, 1), selected));
  auto ambiguous = exact;
  std::strcpy(ambiguous.release_group.data(), "11111111-2222-3333-4444-555555555555");
  const Candidate matches[]{exact, ambiguous};
  assert(select_release_group(identity, matches, selected));
  assert(selected == exact.release_group); // deterministic first-on-score-tie
  ambiguous.score = 100;
  const Candidate ranked[]{exact, ambiguous};
  assert(select_release_group(identity, ranked, selected));
  assert(selected == ambiguous.release_group); // highest exact score wins
  std::array<std::array<char, mbid_capacity>, 5> release_groups{};
  assert(rank_release_groups(identity, ranked, release_groups) == 2);
  assert(release_groups[0] == ambiguous.release_group);
  assert(release_groups[1] == exact.release_group);
  const Candidate duplicates[]{ambiguous, ambiguous, exact};
  assert(rank_release_groups(identity, duplicates, release_groups) == 2);
  assert(release_groups[0] == ambiguous.release_group);
  assert(release_groups[1] == exact.release_group);
  auto live = exact;
  std::strcpy(live.title.data(), "Get Lucky (Live)");
  assert(!select_release_group(identity, std::span(&live, 1), selected));
  auto weak = exact; weak.score = 89;
  assert(!select_release_group(identity, std::span(&weak, 1), selected));
  constexpr std::string_view mb_json = R"JSON({"release-groups":[
    {"id":"aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee","score":100,"title":"Get Lucky",
     "artist-credit":[{"name":"Daft Punk"}]},
    {"id":"11111111-2222-3333-4444-555555555555","score":88,"title":"Get Lucky (Live)",
     "artist-credit":[{"name":"Daft Punk"}]}
  ]})JSON";
  Candidate parsed[5]{};
  assert(parse_musicbrainz(mb_json, parsed, 5) == 2);
  assert(parsed[0].score == 100);
  assert(std::string_view(parsed[0].artist.data()) == "Daft Punk");
  assert(select_release_group(identity, parsed, selected));
  assert(std::string_view(parsed[0].release_group.data()) ==
         "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee");

  NegativeCache cache;
  cache.remember(identity, 100);
  assert(cache.contains(identity, 101));
  assert(!cache.contains(identity, 100 + negative_ttl_ms));
  RequestGate gate;
  assert(gate.submit(identity));
  assert(!gate.submit(identity));
  auto stale = identity; ++stale.metadata_revision;
  assert(!gate.accept(identity, stale));
  assert(gate.accept(identity, identity));

  const std::uint8_t baseline[] = {
      0xff,0xd8, 0xff,0xc0, 0x00,0x08, 0x08,0x00,0xfa,0x00,0xfa,0x01,
      0xff,0xda,0x00,0x08,0x01,0x01,0x00,0x00,0x3f,0x00, 0xff,0xd9};
  JpegInfo jpeg{};
  assert(inspect_image(baseline, jpeg) == ImageFormat::baseline_jpeg);
  assert(jpeg.width == 250 && jpeg.height == 250);
  auto truncated = std::span(baseline).first(sizeof baseline - 2);
  assert(inspect_image(truncated, jpeg) == ImageFormat::unsupported);
  auto progressive = std::array<std::uint8_t, sizeof baseline>{};
  std::copy(std::begin(baseline), std::end(baseline), progressive.begin());
  progressive[3] = 0xc2;
  assert(inspect_image(progressive, jpeg) == ImageFormat::unsupported);
  assert(eligible({true,true,false,false,false,true,false}));
  assert(!eligible({true,true,false,false,true,true,false})); // OTA exclusion
  assert(!eligible({true,true,true,false,false,true,false})); // provisioning
  assert(!eligible({true,true,false,false,false,true,true})); // sleep/UI pause

  NetworkEligibilityGate network_gate{};
  assert(!eligible(network_gate.observe(false, false, false, false,
                                        false, true, false)));
  assert(eligible(network_gate.observe(true, true, false, false,
                                       false, true, false)));
  // A zero-wait mutex miss retains the last authoritative connected sample.
  assert(eligible(network_gate.observe(false, false, true, true,
                                       false, true, false)));
  // Confirmed network lifecycle changes remain authoritative.
  assert(!eligible(network_gate.observe(true, false, false, false,
                                        false, true, false)));
  assert(!eligible(network_gate.observe(true, true, true, false,
                                        false, true, false)));
  assert(redirect_allowed(307, "https://archive.org/cover.jpg", 0));
  assert(!redirect_allowed(307, "http://archive.org/cover.jpg", 0));
  assert(!redirect_allowed(307, "https://archive.org/cover.jpg", 3));
  assert(payload_allowed(200, "image/jpeg", encoded_limit, encoded_limit, true));
  assert(!payload_allowed(200, "image/png", 100, encoded_limit, true));
  assert(!payload_allowed(200, "image/jpeg", encoded_limit + 1, encoded_limit, true));
  assert(!payload_allowed(200, "image/jpeg", 100, encoded_limit, false));
  assert(transient_search_failure(0));
  assert(transient_search_failure(429));
  assert(transient_search_failure(503));
  assert(!transient_search_failure(404));
  assert(!transient_search_failure(200));

  std::cout << "ARTWORK_MODEL: PASS assertions=69\n";
}
