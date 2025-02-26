// RUN: %target-swift-emit-module-interface(%t.swiftinterface) %s -module-name UserModule -strict-memory-safety
// RUN: %target-swift-typecheck-module-from-interface(%t.swiftinterface) -module-name UserModule
// RUN: %FileCheck %s < %t.swiftinterface

// CHECK: #if compiler(>=5.3) && $MemorySafetyAttributes
// CHECK: @unsafe public func getIntUnsafely() -> Swift.Int
// CHECK: #else
// CHECK: public func getIntUnsafely() -> Swift.Int
// CHECK: #endif
@unsafe public func getIntUnsafely() -> Int { 0 }

// CHECK: @inlinable public func useUnsafeCode()
@inlinable public func useUnsafeCode() {
  // CHECK-NOT: unsafe
  print( unsafe getIntUnsafely())
}

// CHECK: public protocol P
public protocol P {
  func f()
}

// CHECK: public struct X : @unsafe UserModule.P
public struct X: @unsafe P {
// CHECK:  #if compiler(>=5.3) && $MemorySafetyAttributes
// CHECK:  @unsafe public func f()
// CHECK:  #else
// CHECK:  public func f()
// CHECK:  #endif
  @unsafe public func f() { }
}
