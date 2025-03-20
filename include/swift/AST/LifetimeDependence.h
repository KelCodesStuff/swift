//===--- LifetimeDependence.h ---------------------------------*- C++ -*-===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2024 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//
//
// This file defines types and utilities related to Lifetime Dependence
//
//===----------------------------------------------------------------------===//

#ifndef SWIFT_AST_LIFETIMEDEPENDENCE_H
#define SWIFT_AST_LIFETIMEDEPENDENCE_H

#include "swift/AST/DeclContext.h"
#include "swift/AST/Identifier.h"
#include "swift/AST/IndexSubset.h"
#include "swift/AST/Ownership.h"
#include "swift/Basic/Debug.h"
#include "swift/Basic/OptionSet.h"
#include "swift/Basic/SourceLoc.h"

#include "llvm/ADT/ArrayRef.h"
#include "llvm/Support/TrailingObjects.h"

namespace swift {

class AbstractFunctionDecl;
class FunctionTypeRepr;
class LifetimeDependentTypeRepr;
class SILParameterInfo;
class SILResultInfo;

enum class ParsedLifetimeDependenceKind : uint8_t {
  Default = 0,
  Scope,
  Inherit // Only used with deserialized decls
};

enum class LifetimeDependenceKind : uint8_t { Inherit = 0, Scope };

struct LifetimeDescriptor {
  enum IsAddressable_t {
    IsNotAddressable,
    IsConditionallyAddressable,
    IsAddressable,
  };

  union Value {
    struct {
      Identifier name;
    } Named;
    struct {
      unsigned index;
      IsAddressable_t isAddress;
    } Ordered;
    struct {
    } Self;
    Value(Identifier name) : Named({name}) {}
    Value(unsigned index, IsAddressable_t isAddress)
      : Ordered({index, isAddress}) {}
    Value() : Self() {}
  } value;

  enum class DescriptorKind { Named, Ordered, Self } kind;

  ParsedLifetimeDependenceKind parsedLifetimeDependenceKind;

  SourceLoc loc;

private:
  LifetimeDescriptor(Identifier name,
                     ParsedLifetimeDependenceKind parsedLifetimeDependenceKind,
                     SourceLoc loc)
      : value{name}, kind(DescriptorKind::Named),
        parsedLifetimeDependenceKind(parsedLifetimeDependenceKind), loc(loc) {}
  LifetimeDescriptor(unsigned index, IsAddressable_t isAddress,
                     ParsedLifetimeDependenceKind parsedLifetimeDependenceKind,
                     SourceLoc loc)
      : value{index, isAddress}, kind(DescriptorKind::Ordered),
        parsedLifetimeDependenceKind(parsedLifetimeDependenceKind), loc(loc) {}
  LifetimeDescriptor(ParsedLifetimeDependenceKind parsedLifetimeDependenceKind,
                     SourceLoc loc)
      : value{}, kind(DescriptorKind::Self),
        parsedLifetimeDependenceKind(parsedLifetimeDependenceKind), loc(loc) {}

public:
  static LifetimeDescriptor
  forNamed(Identifier name,
           ParsedLifetimeDependenceKind parsedLifetimeDependenceKind,
           SourceLoc loc) {
    return {name, parsedLifetimeDependenceKind, loc};
  }
  static LifetimeDescriptor
  forOrdered(unsigned index,
             ParsedLifetimeDependenceKind parsedLifetimeDependenceKind,
             SourceLoc loc,
             IsAddressable_t isAddress = IsNotAddressable) {
    return {index, isAddress, parsedLifetimeDependenceKind, loc};
  }
  static LifetimeDescriptor
  forSelf(ParsedLifetimeDependenceKind parsedLifetimeDependenceKind,
          SourceLoc loc) {
    return {parsedLifetimeDependenceKind, loc};
  }

  ParsedLifetimeDependenceKind getParsedLifetimeDependenceKind() const {
    return parsedLifetimeDependenceKind;
  }

  Identifier getName() const {
    assert(kind == DescriptorKind::Named);
    return value.Named.name;
  }

  unsigned getIndex() const {
    assert(kind == DescriptorKind::Ordered);
    return value.Ordered.index;
  }
  
  IsAddressable_t isAddressable() const {
    return kind == DescriptorKind::Ordered
      ? value.Ordered.isAddress
      : IsNotAddressable;
  }

  DescriptorKind getDescriptorKind() const { return kind; }

  SourceLoc getLoc() const { return loc; }

  bool isImmortal() const {
    if (getDescriptorKind() != LifetimeDescriptor::DescriptorKind::Named) {
      return false;
    }
    return getName().str() == "immortal";
  }

  std::string getString() const {
    switch (kind) {
    case DescriptorKind::Named:
      return getName().str().str();
    case DescriptorKind::Ordered:
      return std::to_string(getIndex());
    case DescriptorKind::Self:
      return "self";
    }
    llvm_unreachable("Invalid DescriptorKind");
  }
};

class LifetimeEntry final
    : private llvm::TrailingObjects<LifetimeEntry, LifetimeDescriptor> {
  friend TrailingObjects;

private:
  SourceLoc startLoc, endLoc;
  unsigned numSources;
  std::optional<LifetimeDescriptor> targetDescriptor;

  LifetimeEntry(
      SourceLoc startLoc, SourceLoc endLoc,
      ArrayRef<LifetimeDescriptor> sources,
      std::optional<LifetimeDescriptor> targetDescriptor = std::nullopt)
      : startLoc(startLoc), endLoc(endLoc), numSources(sources.size()),
        targetDescriptor(targetDescriptor) {
    std::uninitialized_copy(sources.begin(), sources.end(),
                            getTrailingObjects<LifetimeDescriptor>());
  }

  size_t numTrailingObjects(OverloadToken<LifetimeDescriptor>) const {
    return numSources;
  }

public:
  static LifetimeEntry *
  create(const ASTContext &ctx, SourceLoc startLoc, SourceLoc endLoc,
         ArrayRef<LifetimeDescriptor> sources,
         std::optional<LifetimeDescriptor> targetDescriptor = std::nullopt);

  SourceLoc getLoc() const { return startLoc; }
  SourceLoc getStartLoc() const { return startLoc; }
  SourceLoc getEndLoc() const { return endLoc; }

  ArrayRef<LifetimeDescriptor> getSources() const {
    return {getTrailingObjects<LifetimeDescriptor>(), numSources};
  }

  std::optional<LifetimeDescriptor> getTargetDescriptor() const {
    return targetDescriptor;
  }

  std::string getString() const {
    std::string result = "@lifetime(";
    if (targetDescriptor.has_value()) {
      result += targetDescriptor->getString();
      result += ": ";
    }

    bool firstElem = true;
    for (auto source : getSources()) {
      if (!firstElem) {
        result += ", ";
      }
      switch (source.getParsedLifetimeDependenceKind()) {
      case ParsedLifetimeDependenceKind::Scope:
        result += "borrow ";
        break;
      case ParsedLifetimeDependenceKind::Inherit:
        result += "copy ";
        break;
      default:
        break;
      }
      result += source.getString();
      firstElem = false;
    }
    result += ")";
    return result;
  }
};

class LifetimeDependenceInfo {
  IndexSubset *inheritLifetimeParamIndices;
  IndexSubset *scopeLifetimeParamIndices;
  llvm::PointerIntPair<IndexSubset *, 1, bool>
    addressableParamIndicesAndImmortal;
  IndexSubset *conditionallyAddressableParamIndices;

  unsigned targetIndex;

public:
  LifetimeDependenceInfo(IndexSubset *inheritLifetimeParamIndices,
                         IndexSubset *scopeLifetimeParamIndices,
                         unsigned targetIndex, bool isImmortal,
                         // set during SIL type lowering
                         IndexSubset *addressableParamIndices = nullptr,
                         IndexSubset *conditionallyAddressableParamIndices = nullptr)
      : inheritLifetimeParamIndices(inheritLifetimeParamIndices),
        scopeLifetimeParamIndices(scopeLifetimeParamIndices),
        addressableParamIndicesAndImmortal(addressableParamIndices, isImmortal),
        conditionallyAddressableParamIndices(conditionallyAddressableParamIndices),
        targetIndex(targetIndex) {
    assert(this->isImmortal() || inheritLifetimeParamIndices ||
           scopeLifetimeParamIndices);
    assert(!inheritLifetimeParamIndices ||
           !inheritLifetimeParamIndices->isEmpty());
    assert(!scopeLifetimeParamIndices || !scopeLifetimeParamIndices->isEmpty());
    assert((!conditionallyAddressableParamIndices
            || (addressableParamIndices
                && conditionallyAddressableParamIndices
                    ->isSubsetOf(addressableParamIndices)))
     && "conditionally-addressable params not a subset of addressable params?");
  }

  operator bool() const { return !empty(); }

  bool empty() const {
    return !isImmortal() && inheritLifetimeParamIndices == nullptr &&
           scopeLifetimeParamIndices == nullptr;
  }

  bool isImmortal() const { return addressableParamIndicesAndImmortal.getInt(); }

  unsigned getTargetIndex() const { return targetIndex; }

  bool hasInheritLifetimeParamIndices() const {
    return inheritLifetimeParamIndices != nullptr;
  }
  bool hasScopeLifetimeParamIndices() const {
    return scopeLifetimeParamIndices != nullptr;
  }
  bool hasAddressableParamIndices() const {
    return addressableParamIndicesAndImmortal.getPointer() != nullptr;
  }

  IndexSubset *getInheritIndices() const { return inheritLifetimeParamIndices; }

  IndexSubset *getScopeIndices() const { return scopeLifetimeParamIndices; }

  /// Return the set of parameters which have addressable dependencies.
  ///
  /// This indicates that any dependency on the parameter value is dependent
  /// not only on the value, but the memory location of a particular instance
  /// of the value.
  IndexSubset *getAddressableIndices() const {
    return addressableParamIndicesAndImmortal.getPointer();
  }
  /// Return the set of parameters which may have addressable dependencies
  /// depending on the type of the parameter.
  ///
  /// Generic parameters need to be conservatively treated as addressable in
  /// situations where the substituted type may end up being addressable-for-
  /// dependencies. If substitution at a call site or specialization results
  /// in the type becoming concretely non-addressable-for-dependencies,
  /// then the lifetime dependency can be considered a normal value
  /// dependency.
  IndexSubset *getConditionallyAddressableIndices() const {
    return conditionallyAddressableParamIndices;
  }

  bool checkInherit(int index) const {
    return inheritLifetimeParamIndices
      && inheritLifetimeParamIndices->contains(index);
  }

  bool checkScope(int index) const {
    return scopeLifetimeParamIndices
      && scopeLifetimeParamIndices->contains(index);
  }

  bool checkAddressable(int index) const {
    return hasAddressableParamIndices()
      && getAddressableIndices()->contains(index);
  }

  std::string getString() const;
  void Profile(llvm::FoldingSetNodeID &ID) const;
  void getConcatenatedData(SmallVectorImpl<bool> &concatenatedData) const;

  /// Builds LifetimeDependenceInfo from a swift decl, either from the explicit
  /// lifetime dependence specifiers or by inference based on types and
  /// ownership modifiers.
  static std::optional<ArrayRef<LifetimeDependenceInfo>>
  get(AbstractFunctionDecl *decl);

  /// Builds LifetimeDependenceInfo from SIL
  static std::optional<llvm::ArrayRef<LifetimeDependenceInfo>>
  getFromSIL(FunctionTypeRepr *funcRepr, ArrayRef<SILParameterInfo> params,
             ArrayRef<SILResultInfo> results, DeclContext *dc);

  bool operator==(const LifetimeDependenceInfo &other) const {
    return this->isImmortal() == other.isImmortal() &&
           this->getTargetIndex() == other.getTargetIndex() &&
           this->getInheritIndices() == other.getInheritIndices() &&
           this->getAddressableIndices() == other.getAddressableIndices() &&
           this->getScopeIndices() == other.getScopeIndices();
  }

  bool operator!=(const LifetimeDependenceInfo &other) const {
    return !(*this == other);
  }
  
  SWIFT_DEBUG_DUMPER(dump());
};

std::optional<LifetimeDependenceInfo>
getLifetimeDependenceFor(ArrayRef<LifetimeDependenceInfo> lifetimeDependencies,
                         unsigned index);
} // namespace swift

#endif
