from pathlib import Path

root = Path(__file__).resolve().parents[2]
source = (root / "components/rtcp_client/mdns_discovery.cpp").read_text(encoding="utf-8")
client = (root / "components/rtcp_client/client.cpp").read_text(encoding="utf-8")
manifest = (root / "components/rtcp_client/idf_component.yml").read_text(encoding="utf-8")

assert 'mdns_query_ptr("_rtcp","_tcp"' in source
assert 'discovery_candidate_limit,&results' in source
assert 'mdns_query_results_free(results)' in source
assert 'xTaskCreateStatic' in source
assert 'portMAX_DELAY' in source
assert 'pairing' not in source.lower() and 'psk' not in source.lower()
assert 'endpoints_.authenticated(association_)' in client
assert 'endpoints_.clear()' in client and 'discovery_set_network(false)' in client
assert 'discovery_query_interval_ms' in client
assert 'espressif/mdns: "1.14.0"' in manifest
print("RTCP DISCOVERY POLICY 10/10 PASS")
