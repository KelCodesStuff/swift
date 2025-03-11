//===--- ExecutorBridge.cpp - C++ side of executor bridge -----------------===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2025 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//

#if !SWIFT_CONCURRENCY_EMBEDDED
#include <dispatch/dispatch.h>
#endif

#include "Error.h"
#include "ExecutorBridge.h"

using namespace swift;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreturn-type-c-linkage"

extern "C" SWIFT_CC(swift)
void _swift_exit(int result) {
  exit(result);
}

extern "C" SWIFT_CC(swift)
void _swift_task_checkIsolatedSwift(
  HeapObject *executor,
  const Metadata *executorType,
  const SerialExecutorWitnessTable *witnessTable
);

extern "C" SWIFT_CC(swift)
void _swift_task_checkIsolatedSwift(HeapObject *executor,
                                    const Metadata *executorType,
                                    const SerialExecutorWitnessTable *witnessTable);

extern "C" SWIFT_CC(swift)
void _swift_task_isIsolatingCurrentContextSwift(
  HeapObject *executor,
  const Metadata *executorType,
  const SerialExecutorWitnessTable *witnessTable
);

extern "C" SWIFT_CC(swift)
bool _swift_task_isMainExecutorSwift(
  HeapObject *executor,
  const Metadata *executorType,
  const SerialExecutorWitnessTable *witnessTable
);

extern "C" SWIFT_CC(swift)
void swift_task_checkIsolatedImpl(SerialExecutorRef executor) {
  HeapObject *identity = executor.getIdentity();

  // We might be being called with an actor rather than a "proper"
  // SerialExecutor; in that case, we won't have a SerialExecutor witness
  // table.
  if (executor.hasSerialExecutorWitnessTable()) {
    _swift_task_checkIsolatedSwift(identity,
                                   swift_getObjectType(identity),
                                   executor.getSerialExecutorWitnessTable());
  } else {
    const Metadata *objectType = swift_getObjectType(executor.getIdentity());
    auto typeName = swift_getTypeName(objectType, true);

    swift_Concurrency_fatalError(
      0, "Incorrect actor executor assumption; expected '%.*s' executor.\n",
      (int)typeName.length, typeName.data);
  }
}

extern "C" SWIFT_CC(swift)
void swift_task_isIsolatingCurrentContextImpl(SerialExecutorRef executor) {
  HeapObject *identity = executor.getIdentity();

  // We might be being called with an actor rather than a "proper"
  // SerialExecutor; in that case, we won't have a SerialExecutor witness
  // table.
  if (executor.hasSerialExecutorWitnessTable()) {
    return _swift_task_isIsolatingCurrentContextSwift(identity,
                                   swift_getObjectType(identity),
                                   executor.getSerialExecutorWitnessTable());
  } else {
    const Metadata *objectType = swift_getObjectType(executor.getIdentity());
    auto typeName = swift_getTypeName(objectType, true);

    swift_Concurrency_fatalError(
      0, "Incorrect actor executor assumption; expected '%.*s' executor.\n",
      (int)typeName.length, typeName.data);
  }
}

extern "C" SWIFT_CC(swift)
bool swift_task_isMainExecutorImpl(SerialExecutorRef executor) {
  HeapObject *identity = executor.getIdentity();
  return executor.hasSerialExecutorWitnessTable()
    && _swift_task_isMainExecutorSwift(identity,
                                       swift_getObjectType(identity),
                                       executor.getSerialExecutorWitnessTable());
}

extern "C" SWIFT_CC(swift)
uint8_t swift_job_getPriority(Job *job) {
  return (uint8_t)(job->getPriority());
}

extern "C" SWIFT_CC(swift)
uint8_t swift_job_getKind(Job *job) {
  return (uint8_t)(job->Flags.getKind());
}

extern "C" SWIFT_CC(swift)
void *swift_job_getExecutorPrivateData(Job *job) {
  return &job->SchedulerPrivate[0];
}

#if !SWIFT_CONCURRENCY_EMBEDDED
extern "C" SWIFT_CC(swift)
void *swift_createDispatchEventC(void (*handler)(void *), void *context) {
  dispatch_source_t source = dispatch_source_create(DISPATCH_SOURCE_TYPE_DATA_OR,
                                                    0, 0,
                                                    dispatch_get_main_queue());
  dispatch_source_set_event_handler_f(source, handler);
  dispatch_activate(source);

  return source;
}

extern "C" SWIFT_CC(swift)
void swift_destroyDispatchEventC(void *event) {
  dispatch_source_t source = (dispatch_source_t)event;

  dispatch_source_cancel(source);
  dispatch_release(source);
}

extern "C" SWIFT_CC(swift)
void *swift_getDispatchEventContext(void *event) {
  return dispatch_get_context((dispatch_source_t)event);
}

extern "C" SWIFT_CC(swift)
void swift_signalDispatchEvent(void *event) {
  dispatch_source_t source = (dispatch_source_t)event;

  dispatch_source_merge_data(source, 1);
}

extern "C" SWIFT_CC(swift) __attribute__((noreturn))
void swift_dispatchMain() {
  dispatch_main();
}

extern "C" SWIFT_CC(swift)
void swift_dispatchAssertMainQueue() {
  dispatch_assert_queue(dispatch_get_main_queue());
}

#endif // !SWIFT_CONCURRENCY_EMBEDDED

#pragma clang diagnostic pop
