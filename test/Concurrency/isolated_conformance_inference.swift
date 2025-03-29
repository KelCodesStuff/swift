// RUN: %target-swift-frontend -typecheck -verify -target %target-swift-5.1-abi-triple -swift-version 6 -enable-experimental-feature IsolatedConformances -enable-experimental-feature InferIsolatedConformances %s

// REQUIRES: swift_feature_IsolatedConformances
// REQUIRES: swift_feature_InferIsolatedConformances
// REQUIRES: concurrency

protocol P {
  func f()
}

@SomeGlobalActor
protocol Q {
  func g()
}

@globalActor
actor SomeGlobalActor {
  static let shared = SomeGlobalActor()
}

@SomeGlobalActor
protocol R {
  func h()
}

@SomeGlobalActor
class CExplicit: P {
  func f() { } // okay! conformance above is isolated
}

// If the protocol itself is isolated, don't do anything.
extension CExplicit: Q {
  func g() { }
}

// expected-error@+3{{conformance of 'CNonIsolated' to protocol 'P' crosses into global actor 'SomeGlobalActor'-isolated code and can cause data races}}
// expected-note@+2{{turn data races into runtime errors with '@preconcurrency'}}
// expected-note@+1{{isolate this conformance to the global actor 'SomeGlobalActor' with '@SomeGlobalActor'}}{{33-33=@SomeGlobalActor }}
nonisolated class CNonIsolated: P {
  @SomeGlobalActor func f() { } // expected-note{{global actor 'SomeGlobalActor'-isolated instance method 'f()' cannot satisfy nonisolated requirement}}
}

func acceptSendablePMeta<T: Sendable & P>(_: T.Type) { }
func acceptSendableQMeta<T: Sendable & Q>(_: T.Type) { }

nonisolated func testConformancesFromNonisolated() {
  let _: any P = CExplicit() // expected-error{{global actor 'SomeGlobalActor'-isolated conformance of 'CExplicit' to 'P' cannot be used in nonisolated context}}

  let _: any P = CNonIsolated()

  // Okay, these are nonisolated conformances.
  let _: any Q = CExplicit()
}
