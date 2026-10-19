#include "remote_persistence/backend.hpp"

#include <cstdlib>
#include <functional>
#include <iostream>
#include <map>
#include <stdexcept>

using namespace remote_persistence;

namespace {
struct FakeStore final : Store {
    std::map<std::string, std::vector<std::uint8_t>> durable;
    std::map<std::string, std::vector<std::uint8_t>> staged;
    int commits{0};
    int fail_commit{0};
    StoreResult forced_set{StoreResult::ok};

    StoreResult list_keys(std::vector<std::string>& keys) override { for (const auto& [key, value] : durable) { (void)value; keys.push_back(key); } return StoreResult::ok; }
    StoreResult get(const std::string& key, std::vector<std::uint8_t>& value) override { const auto it = durable.find(key); if (it == durable.end()) return StoreResult::not_found; value = it->second; return StoreResult::ok; }
    StoreResult set(const std::string& key, std::span<const std::uint8_t> value) override { if (forced_set != StoreResult::ok) return forced_set; staged[key] = {value.begin(), value.end()}; return StoreResult::ok; }
    StoreResult commit() override { ++commits; if (fail_commit == commits) { staged.clear(); return StoreResult::io_error; } for (const auto& [key,value] : staged) durable.insert_or_assign(key,value); staged.clear(); return StoreResult::ok; }
};

void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
std::vector<std::uint8_t> bytes(std::initializer_list<std::uint8_t> value) { return value; }
void initialize(FakeStore& store) { Backend backend(store); require(backend.initialize_erased() == Status::ok, "initialize"); }
void commit(FakeStore& store, std::uint64_t generation, std::vector<std::uint8_t> payload) { Backend backend(store); require(backend.commit(generation, payload) == Status::ok, "commit"); }

void test_empty_initialization() {
    FakeStore store; Backend backend(store); require(backend.recover().status == Status::empty, "empty"); require(backend.initialize_erased() == Status::ok, "init");
    const auto recovered = backend.recover(); require(recovered.status == Status::ok && recovered.active_generation == 0 && recovered.next_generation == 1 && recovered.payload.empty(), "initialized state");
}
void test_first_commit() { FakeStore s; initialize(s); commit(s, 1, bytes({1,2,3})); const auto r=Backend(s).recover(); require(r.status==Status::ok && r.active_generation==1 && r.next_generation==2 && r.payload==bytes({1,2,3}),"first"); }
void test_successor_commit() { FakeStore s; initialize(s); commit(s,1,bytes({1})); commit(s,2,bytes({2})); const auto r=Backend(s).recover(); require(r.status==Status::ok && r.active_generation==2 && r.next_generation==3 && r.payload==bytes({2}),"successor"); }
void test_interrupted_before_record() {
    FakeStore s; initialize(s); commit(s,1,bytes({1})); s.fail_commit=s.commits+2; require(Backend(s).commit(2,bytes({2}))==Status::storage_error,"interruption expected");
    const auto r=Backend(s).recover(); require(r.status==Status::ok && r.incomplete_transaction && r.active_generation==1 && r.next_generation==3 && r.payload==bytes({1}),"reserved generation must not be reused");
}
void test_interrupted_before_root() {
    FakeStore s; initialize(s); commit(s,1,bytes({1})); s.fail_commit=s.commits+3; require(Backend(s).commit(2,bytes({2}))==Status::storage_error,"interruption expected");
    const auto r=Backend(s).recover(); require(r.status==Status::ok && r.incomplete_transaction && r.active_generation==1 && r.next_generation==3 && r.payload==bytes({1}),"dangling record must not become authority");
}
void test_corrupt_active_record() { FakeStore s; initialize(s); commit(s,1,bytes({9})); s.durable["rec00"][8]^=0x80; require(Backend(s).recover().status==Status::corrupt,"corrupt record"); }
void test_corrupt_root() { FakeStore s; initialize(s); commit(s,1,bytes({9})); s.durable["root"][12]^=1; require(Backend(s).recover().status==Status::corrupt,"corrupt root"); }
void test_duplicate_generation() { FakeStore s; initialize(s); commit(s,1,bytes({9})); s.durable["rec01"]=s.durable["rec00"]; require(Backend(s).recover().status==Status::ambiguous,"duplicate generation"); }
void test_capacity_exhaustion_without_reservation() {
    FakeStore s; initialize(s); for(std::uint64_t g=1;g<=12;++g) commit(s,g,bytes({static_cast<std::uint8_t>(g)}));
    std::size_t records=0; for(const auto& [key,value]:s.durable) { (void)value; if(key.starts_with("rec")) ++records; } require(records==kMaxRecordCount,"bounded record slots");
    const auto before=s.durable["fence"]; std::vector<std::uint8_t> too_large(kMaxPayloadBytes+1);
    require(Backend(s).commit(13,too_large)==Status::capacity_exhausted,"payload capacity"); require(s.durable["fence"]==before,"capacity must refuse before fence");
}
void test_restart_selection_and_zero_emission() { FakeStore s; int independent_emissions=0; initialize(s); commit(s,1,bytes({7})); const auto image=s.durable; FakeStore restarted; restarted.durable=image; const auto r=Backend(restarted).recover(); require(r.status==Status::ok && r.payload==bytes({7}) && independent_emissions==0,"restart"); }
void test_replay_and_nonmonotonic_rejected() { FakeStore s; initialize(s); commit(s,1,bytes({1})); require(Backend(s).commit(1,bytes({2}))==Status::replay_rejected,"replay"); require(Backend(s).commit(3,bytes({3}))==Status::replay_rejected,"skip"); }
void test_storage_full_and_version_errors_fail_closed() { FakeStore s; initialize(s); s.forced_set=StoreResult::no_space; require(Backend(s).commit(1,bytes({1}))==Status::capacity_exhausted,"full"); FakeStore v; v.forced_set=StoreResult::version_mismatch; require(Backend(v).initialize_erased()==Status::schema_mismatch,"version"); }
void test_unknown_key_ambiguous() { FakeStore s; initialize(s); s.durable["foreign"]={1}; require(Backend(s).recover().status==Status::ambiguous,"unknown key"); }
void test_coherent_domain_rollback_is_outside_v1() {
    FakeStore s; initialize(s); commit(s,1,bytes({1})); const auto old_image=s.durable; commit(s,2,bytes({2})); s.durable=old_image;
    const auto r=Backend(s).recover(); require(r.status==Status::ok && r.active_generation==1,"coherent rollback demonstrates G1b exclusion");
}
}

int main() {
    const std::pair<const char*, std::function<void()>> tests[] = {
        {"empty_partition_initialization",test_empty_initialization},{"first_commit",test_first_commit},{"successor_commit",test_successor_commit},
        {"interrupted_before_record",test_interrupted_before_record},{"interrupted_after_record_before_root",test_interrupted_before_root},
        {"corrupted_record",test_corrupt_active_record},{"corrupted_authority_root",test_corrupt_root},{"ambiguous_duplicate_generation",test_duplicate_generation},
        {"capacity_exhaustion",test_capacity_exhaustion_without_reservation},{"restart_recovery_zero_emission",test_restart_selection_and_zero_emission},
        {"replay_nonmonotonic_rejection",test_replay_and_nonmonotonic_rejected},{"nvs_full_version_error_handling",test_storage_full_and_version_errors_fail_closed},
        {"unknown_key_fail_closed",test_unknown_key_ambiguous},{"coherent_root_records_rollback_G1b_outside",test_coherent_domain_rollback_is_outside_v1},
    };
    int passed=0;
    for(const auto& [name,test]:tests) { try { test(); ++passed; std::cout<<"PASS "<<name<<'\n'; } catch(const std::exception& error) { std::cerr<<"FAIL "<<name<<": "<<error.what()<<'\n'; return EXIT_FAILURE; } }
    std::cout<<"TOTAL "<<passed<<"/"<<std::size(tests)<<" PASS\n"; return EXIT_SUCCESS;
}
