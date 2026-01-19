#ifndef LLVM_ADT_DISJOINT_LOCATION_SETS
#define LLVM_ADT_DISJOINT_LOCATION_SETS

#include <cassert>
#include <cstdint>
#include <set>
#include <vector>

#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace llvm {
class DisjointLocationSets {
  // Sets of stackmap locations whose members are known to contain copies of
  // the same value.
  //
  // For example [ {1, 2}, {-32, 0} ] means:
  //  - DWARF registers 1 and 2 contain copies of the same value.
  //  - Register 0 and stack offset -32 contain copies of the same value.
  //
  // A "distinct" location is a location not known to contain a copy of any
  // other location. Conceptually a distinct location would exist in a set
  // containing no other locations (i.e. `Set.size() == 1`). We don't store
  // distinct locations: we assume that any location absent from a set is
  // distinct. This implementation assumes that that distinct locations must
  // be "canonicalised away".
  std::vector<std::set<int64_t>> Sets;

  // Find the set containing `Loc` and return an iterator (over `this.Sets)
  // to it.
  //
  // Returns `notFound()` if no such set can be found.
  std::vector<std::set<int64_t>>::iterator findInSet(int64_t Loc) {
    for (auto It = Sets.begin(); It != Sets.end(); It++) {
      std::set<int64_t> &S = *It;
      if (S.count(Loc) != 0) {
        return It;
      }
    }
    return notFound();
  }

  // Overwrite the set containing `Loc` with `Set`.
  //
  // If calling this causes a set to be erased, then iterators to
  // `this.Sets` are invalidated.
  //
  // Iterators over the set containing `Loc` are always invalidated.
  void updateSet(int64_t Loc, std::set<int64_t> Set) {
    std::vector<std::set<int64_t>>::iterator It = findInSetOrCreate(Loc);
    if (Set.size() > 1) {
      It->clear();
      for (int64_t L : Set) {
        It->insert(L);
      }
    } else {
      // remove the whole set
      Sets.erase(It);
    }
    assert(verify());
  }

  // Remove `Loc` from the its parent set.
  //
  // If calling this causes a set to be erased, then iterators to
  // `this.Sets` are invalidated.
  //
  // Iterators over the set containing `Loc` are always invalidated.
  void removeFromSet(int64_t Loc, std::vector<std::set<int64_t>>::iterator It) {
    assert(It != notFound());
    std::set<int64_t> &S = *It;
    S.erase(Loc);
    // We can remove the whole set if it now contains a sole element, or is
    // empty.
    if (S.size() <= 1) {
      Sets.erase(It);
    }
    assert(verify());
  }

  // Find the set containing `FindLoc` and return an interator to it. If no
  // such set exists, create a new set containing `FindLoc`.
  //
  // Note that `this.Sets` mustn't contain sets with only one element, so
  // if calling this creates such a set, it is the responsibility of the
  // caller to ensure eventual consistency.
  //
  // If calling this creates a new set, then iterators over `this.Sets` are
  // invalidated.
  std::vector<std::set<int64_t>>::iterator findInSetOrCreate(int64_t FindLoc) {
    std::vector<std::set<int64_t>>::iterator Ret = Sets.end();
    std::vector<std::set<int64_t>>::iterator It = findInSet(FindLoc);
    if (It == Sets.end()) {
      // Not found. Create a set.
      std::set<int64_t> New;
      New.insert(FindLoc);
      Sets.push_back(New);
      Ret = std::prev(Sets.end());
    } else {
      // Found.
      Ret = It;
    }
    return Ret;
  }

  // Returns an iterator that flags a `find` operation was unsucessful.
  std::vector<std::set<int64_t>>::iterator notFound() { return Sets.end(); }

public:
  // Find the set containing `Loc` (if it exists) and return the other
  // remaining elements of the set (i.e. those not `Loc`).
  std::set<int64_t> getOthersInSet(int64_t Loc) {
    assert(verify());
    std::vector<std::set<int64_t>>::iterator It = findInSet(Loc);
    if (It == notFound()) {
      return {}; // No remaining elements to speak of.
    }
    // YKFIXME: I would have liked to have returned an iterator and use
    // `std::views::filter` here, but that's C++20 and a the time of writing
    // ykllvm is C++17. Instead we copy the set and manually "filter out"
    // (remove) `Loc`.
    std::set<int64_t> Ret = *It;
    Ret.erase(Loc);
    return Ret;
  }

  // Move `FromLoc` into the set containing `ToLoc`.
  //
  // Use this when `FromLoc` has assumed the value of `ToLoc`.
  void unifyWith(int64_t FromLoc, int64_t ToLoc) {
    if (FromLoc == ToLoc) {
      return;
    }
    std::vector<std::set<int64_t>>::iterator FromIt = findInSet(FromLoc);
    if (FromIt != notFound()) {
      removeFromSet(FromLoc, FromIt);
    }
    std::vector<std::set<int64_t>>::iterator ToIt = findInSetOrCreate(ToLoc);
    ToIt->insert(FromLoc);
    assert(verify());
  }

  // Make `Loc` "distinct". In other words make it disjoint from all other
  // sets.
  //
  // If calling this causes a set to be erased, then iterators to
  // `this.Sets` are invalidated.
  //
  // Iterators over the set containing `Loc` are always invalidated.
  void makeDistinct(int64_t Loc) {
    std::vector<std::set<int64_t>>::iterator It = findInSet(Loc);
    if (It != Sets.end()) {
      removeFromSet(Loc, It);
    }
    assert(verify());
  }

  // Update `this.Sets` by intersecting with `Prev.Sets`
  //
  // Returns whether the new `this.Sets` is different to `Prev.Sets`.
  bool intersect(DisjointLocationSets Prev) {
    std::set<std::set<int64_t>> Before(Prev.Sets.begin(), Prev.Sets.end());
    std::set<std::set<int64_t>> Res;

    for (std::set<int64_t> &S1 : Sets) {
      for (std::set<int64_t> &S2 : Prev.Sets) {
        std::set<int64_t> Inter;
        std::set_intersection(S1.begin(), S1.end(), S2.begin(), S2.end(),
                              std::inserter(Inter, Inter.begin()));
        // If the set is size 1, then it's distinct and we don't store it.
        if (Inter.size() > 1) {
          Res.insert(Inter);
        }
      }
    }

    bool Changed = (Res != Before);
    std::vector<std::set<int64_t>> ResVec(Res.begin(), Res.end());
    Sets = ResVec;

    assert(verify());
    return Changed;
  }

  // Print the contents of the `DisjointLocationSets`.
  void dump() {
    errs() << "--- Begin DisjointLocationSets ---\n";
    for (std::set<int64_t> &S : Sets) {
      errs() << "{ ";
      for (const int64_t &L : S) {
        errs() << L << ", ";
      }
      errs() << "}\n";
    }
    errs() << "--- End DisjointLocationSets ---\n";
  }

  // Check the consistency of this `DisjointLocationSets`.
  //
  // Returns `false` if something is wrong, otherwise `true`.
  bool verify() {
    std::set<int64_t> Seen;
    for (auto &S : Sets) {
      // No set can have only one member. Instead the set is absent from
      // `this.Sets`.
      if (S.size() <= 1) {
        return false;
      }
      for (auto &L : S) {
        // Check that no location exists in more than one set.
        if (Seen.find(L) != Seen.end()) {
          return false;
        }
        Seen.insert(L);
      }
    }
    return true;
  }

  void clear() { Sets.clear(); }
};
} // namespace llvm

#endif // LLVM_ADT_DISJOINT_LOCATION_SETS
