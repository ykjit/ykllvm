//===- OutlineUntraceable.cpp - Add yk_outline to untraceable functions -===
////
//
// This pass searches for functions which are unlikely to be traced and marks
// them `yk_outline`. This has two effects:
//
//  - It causes instrumentation passes that run after this to ignore the
//    untraceable functions. This means that we don't have to pay a runtime
//    performance penalty for unnecessary instrumentation.
//
//  - Because `yk_outline` functions don't have Yk IR serialised, it means that
//    the resulting binary is smaller, and we spend less time on serialisation.

#include "llvm/Transforms/Yk/OutlineUntraceable.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/InitializePasses.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/Yk/ControlPoint.h"
#include "llvm/YkIR/YkIRWriter.h"

#include <map>
#include <set>

#define DEBUG_TYPE "yk-outline-untraceable"

using namespace llvm;

namespace llvm {
void initializeOutlineUntraceablePass(PassRegistry &);
} // namespace llvm

namespace {

/// A data structure that tracks the direct callers of each function in the
/// module.
class InvertedCallGraph {
  // Maps a callee to its (direct) callers.
  std::map<Function *, std::set<Function *>> NodeMap;

public:
  InvertedCallGraph(Module &M) {
    for (Function &F : M) {
      NodeMap[&F] = std::set<Function *>();
      for (User *U : F.users()) {
        if (CallBase *CB = dyn_cast<CallBase>(U)) {
          // A call instruction uses the function.
          //
          // Inline asm isn't really a call.
          if (CB->isInlineAsm()) {
            continue;
          }
          // But does it actually call F? It could just be passing a function
          // pointer to F as an argument to a different callee.
          Function *CF =
              dyn_cast<Function>(CB->getCalledOperand()->stripPointerCasts());
          if (CF == nullptr) {
            assert(CB->isIndirectCall());
            assert(F.hasAddressTaken());
            continue;
          }
          if (CF == &F) {
            // It really is a direct call to F.
            Function *ParentF = CB->getFunction();
            NodeMap[&F].insert(ParentF);
          } else {
            assert(F.hasAddressTaken());
          }
        } else {
          if (!isa<BlockAddress>(U)) {
            assert(F.hasAddressTaken());
          }
        }
      }
    }
  }

  std::set<Function *> &callersOf(Function *F) { return NodeMap.at(F); }
};

class OutlineUntraceable : public ModulePass {
public:
  static char ID;
  OutlineUntraceable() : ModulePass(ID) {
    initializeOutlineUntraceablePass(*PassRegistry::getPassRegistry());
  }

  bool runOnModule(Module &M) override {
    InvertedCallGraph IG(M);
    bool Changed = false;

    for (Function &F : M) {
      // Note that the inverted call graph doesn't take into account indirect
      // calls. This means that we will sometimes consider functions that could
      // be traceable (via indirect calls) untraceable.
      //
      // An earlier attempt to consider functions called from functions with
      // their address taken, as potentially traceable, lead to much worse
      // performance overall. We may need to revisit this.
      //
      // To prevent this pass from `yk_outline`ing a function only called
      // indirectly, the interpreter author can annotate it with the
      // `yk_indirect_inline` function attribute.
      if ((!F.isDeclaration()) && (!couldBeTraced(&IG, &F)) &&
          (!F.hasFnAttribute(YK_INDIRECT_INLINE_FNATTR))) {
        F.addFnAttr(YK_OUTLINE_FNATTR);
        Changed = true;
      }
    }
    return Changed;
  }

private:
  bool couldBeTraced(InvertedCallGraph *IG, Function *F) {
    if (containsControlPoint(*F)) {
      // Functions with control points must be traceable.
      //
      // Note that this is true even though they are usually marked yk_outline
      // because they typically contain a loop.
      return true;
    }

    if (F->isDeclaration()) {
      // It's opaque to the JIT, so we can't possibly trace it.
      return false;
    }

    if (F->hasFnAttribute(YK_OUTLINE_FNATTR)) {
      // It's already marked yk_outline, so we can't trace it.
      return false;
    }

    // We've checked all the easy stuff, now we have to inspect the call graph.
    // If there exists at least one direct-call path from the control point to
    // F, then F may be traced, otherwise it can't.
    std::set<Function *> Work;
    std::set<Function *> Seen;
    Work.insert(F);
    while (!Work.empty()) {
      auto It = Work.begin();
      Function *CurF = *It;
      Work.erase(It);

      if (containsControlPoint(*CurF)) {
        return true; // could be traced.
      }

      if (Seen.find(CurF) != Seen.end()) {
        // We've already dealt with this function via another call path.
        continue;
      }
      Seen.insert(CurF);

      if (CurF->hasFnAttribute(YK_OUTLINE_FNATTR)) {
        // F can't be traced by this call-chain.
        continue;
      }

      // Keep going up the call chain.
      for (Function *Caller : IG->callersOf(CurF)) {
        Work.insert(Caller);
      }
    }

    return false; // can't be traced.
  }
};
} // namespace

char OutlineUntraceable::ID = 0;
INITIALIZE_PASS(OutlineUntraceable, DEBUG_TYPE, "yk outline untraceable", false,
                false)

/**
 * @brief Creates a new OutlineUntraceable pass.
 *
 * @return ModulePass* A pointer to the newly created OutlineUntraceable pass.
 */
ModulePass *llvm::createOutlineUntraceablePass() {
  return new OutlineUntraceable();
}
