#pragma once
#include "model.hpp"
namespace diagnostics {
// NETWORK initializes once; all owner publications are zero-wait and allocation-free.
bool initialize();
void publish(const Core&); void publish(const Power&); void publish(const Network&);
void publish(const RtcpCloseHistory&);
void event(U64,Event,unsigned detail=0);
bool quiesce(); // NETWORK only; false unless HTTP server stopped.
void revoke(); // Task/event-callback safe, no socket I/O.
void service(Address desired); // NETWORK only; deterministic lifecycle/retry.
unsigned errors();
unsigned stack_min();
}
