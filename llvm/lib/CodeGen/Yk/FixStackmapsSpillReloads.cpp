//===-- FixStackmapsSpillReloads.cpp - Fix spills before stackmaps --------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This pass fixes stackmaps in regards to spill reloads inserted by the
// register allocator. For example, if we have the LLVM IR
//
//   call foo($10, $11)
//   call llvm.experimental.stackmaps(1, 0, $8, $9)
//
// After register allocation we might get something like
//
//   movrr $rbx, $rsi
//   movmr $rbp, -8, $rdi
//   ...
//   call foo($rsi, $rdi)
//   movrr $rsi, $rbx
//   movrm $rdi, $rbp, -8
//   STACKMAP $rsi, $rdi
//
// In order to pass arguments to foo, the register allocator had to spill the
// values in $rdi and $rsi into another register or onto the stack before the
// call. Then immediately after the call it inserted instructions to reload
// the spilled values back into the original registers. Since during
// deoptimisation we return to immediately after the call, the stackmap is now
// tracking the wrong values, e.g. in this case $rdi and $rsi instead of the
// spill locations.
//
// This pass interates over all basic blocks, finds spill reloads inserted
// inbetween a call and stackmap, replaces the stackmap operands with the
// spill reloads, and then moves the stackmap instruction up just below the
// call.
//
// The following gives an overview of the cases this pass handles.
//
// ## The easy cases
//
// ### Copy
//
// ```
// call foo
// $rbx = $rax
// STACKMAP $rbx
// ```
//
// A simple spill reload via another register. Move the instruction after the
// STACKMAP and replace the `$rbx` operand in the stackmap with `$rax`:
//
// ```
// call foo
// STACKMAP $rax
// $rbx = $rax
// ```
//
// [From here on we omit the `call foo` to simplify the examples.]
//
// ### Load from stack
//
// ```
// $rbx = load $rbp-8
// STACKMAP $rbx
// ```
//
// A simple spill reload from the stack. Replace `$rbx` in the stackmap with
// `$rbp-8`:
//
// ```
// STACKMAP $rbp-8
// $rbx = load $rbp-8
// ```
//
// ### Remove invalid implicit kills
//
// ```
// $rbx = $rax
// STACKMAP $rbx, implicit killed $rax
// ```
//
// The stackmap contains an implicitly killed register. But when we move the
// instruction, this `implicit killed` becomes invalid. Remove the `implicit
// killed $rax` as `$rax` is now live:
//
// ```
// STACKMAP $rax
// $rbx = $rax
// ```
//
// ### Adjust killed flags
//
// ```
// mov rax, killed rcx
// STACKMAP rax
// ```
//
// When replacing STACKMAP operands, make sure we don't copy over `killed`
// flags that are now no longer valid. We should not replace `$rax` with
// `killed $rcx`, but instead with `$rcx`. In short, remove all kill flags of
// all register operands we add to the stackmap.
//
// ```
// STACKMAP rcx
// mov rax, killed rcx
// ```
//
// ### Other defines that are not spill reloads
//
// ```
// rcx = lea $rbp-8
// STACKMAP killed rcx
// ```
//
// Some tracked registers do no longer need to be tracked after moving around
// instructions. In this case, `$rcx` was "reloaded" via a `lea` instruction.
// Moving the `lea` instruction below the STACKMAP means we always compute the
// correct value of `$rcx` after deopt so we no longer need to track `$rcx`. We
// can't just remove it from the STACKMAP as that will mess with the live
// variable ordering. Instead, we replace it with a constant `0xdead` (57005)
// so we can easily recognise it when things go wrong.
//
// ```
// STACKMAP 0xdead
// rcx = lea $rbp-8
// ```
//
// ### Stack reload that isn't a spill reload
//
// ```
// mov rax, rbp + 10
// STACKMAP rax
// ```
//
// This instruction looks like a spill reload, but actually isn't one. In this
// example, we are loading a value from the previous stack frame, likely an
// argument passed into this function via the stack. We don't need to track
// this and can also replace it with a constant. We can check if this is a real
// spill using `MI->getRestoreSize`, which returns an `std::optional`. No
// result means this isn't a real spill reload.
//
// ```
// STACKMAP 0xdead
// mov rax, rbp + 10
// ```
//===----------------------------------------------------------------------===//

#include "llvm/CodeGen/LivePhysRegs.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineOperand.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/StackMaps.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/IR/DebugLoc.h"
#include "llvm/InitializePasses.h"
#include "llvm/Support/Debug.h"
#include "llvm/Transforms/Yk/BasicBlockTracer.h"

#include <map>
#include <set>

using namespace llvm;

#define DEBUG_TYPE "fix-stackmaps-spill-reloads"

namespace {

class FixStackmapsSpillReloads : public MachineFunctionPass {
public:
  static char ID;

  FixStackmapsSpillReloads() : MachineFunctionPass(ID) {
    initializeFixStackmapsSpillReloadsPass(*PassRegistry::getPassRegistry());
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesCFG();
    MachineFunctionPass::getAnalysisUsage(AU);
  }

  StringRef getPassName() const override {
    return "Stackmaps Fix Post RegAlloc Pass";
  }

  bool runOnMachineFunction(MachineFunction &MF) override;
};

} // namespace

char FixStackmapsSpillReloads::ID = 0;
char &llvm::FixStackmapsSpillReloadsID = FixStackmapsSpillReloads::ID;

INITIALIZE_PASS(FixStackmapsSpillReloads, DEBUG_TYPE, "Fixup Stackmap Spills",
                false, false)

const TargetRegisterInfo *TRI;

bool FixStackmapsSpillReloads::runOnMachineFunction(MachineFunction &MF) {
  TRI = MF.getSubtarget().getRegisterInfo();
  bool Changed = false;
  const TargetInstrInfo *TII = MF.getSubtarget().getInstrInfo();
  for (MachineBasicBlock &MBB : MF) {
    bool Collect = false;
    std::set<MachineInstr *> Erased;
    std::set<MachineInstr *> NewInstrs;
    MachineInstr *LastCall = nullptr;
    std::map<Register, MachineInstr *> Spills;
    for (MachineInstr &MI : MBB) {
      if (MI.isCall() && !MI.isInlineAsm()) {
        // YKFIXME: Do we need to check for intrinsics here or have they been
        // removed during lowering?
        if (MI.getOpcode() != TargetOpcode::STACKMAP &&
            MI.getOpcode() != TargetOpcode::PATCHPOINT) {
          MachineOperand Op = MI.getOperand(0);
          if (Op.isGlobal() &&
              (Op.getGlobal()->getName() == YK_TRACE_FUNCTION)) {
            // Op.getGlobal()->getGlobalIdentifier() == YK_TRACE_FUNCTION) {
            // `YK_TRACE_FUNCTION` calls don't require stackmaps so we don't
            // need to adjust anything here. In fact, doing so will skew any
            // stackmap that follows.
            continue;
          }
          // If we see a normal function call we know it will be followed by a
          // STACKMAP instruction. Set `Collect` to `true` to collect all spill
          // reload instructions between this call and the STACKMAP instruction.
          // Also remember this call, so we can insert the new STACKMAP
          // instruction right below it.
          Collect = true;
          LastCall = &MI;
          Spills.clear();
          continue;
        }
      }

      if (MI.getOpcode() == TargetOpcode::STACKMAP) {
        if (LastCall == nullptr) {
          // There wasn't a call preceeding this stackmap, so this must be
          // attached to a branch instruction.
          continue;
        }
        Collect = false;
        // Assemble a new stackmap instruction by copying over the operands of
        // the old instruction to the new one, while replacing spilled operands
        // as we go.
        MachineInstr *NewMI = MF.CreateMachineInstr(
            TII->get(TargetOpcode::STACKMAP), MI.getDebugLoc(), true);
        MachineInstrBuilder MIB(MF, NewMI);
        // Copy ID and shadow
        auto *MOI = MI.operands_begin();
        MIB.add(*MOI); // ID
        MOI++;
        MIB.add(*MOI); // Shadow
        MOI++;
        while (MOI != MI.operands_end()) {
          if (MOI->isReg()) {
            unsigned int Reg = getDwarfRegNum(MOI->getReg(), TRI);
            // Check if the register operand in the stackmap is a restored
            // spill.
            if (MOI->isImplicit()) {
              if (MOI->isKill()) {
                // implicit kills are not required.
                MOI++;
                continue;
              }
              // Leave other implicits (e.g. defs) as is.
              // FIXME: Should we replace implicit defs using the `Spills` map?
              MIB.add(*MOI);
            } else if (Spills.count(Reg) == 0) {
              // A tracked register not related to spills. Leave as is.
              MIB.add(*MOI);
            } else {
              // This register is related to a spill.
              MachineInstr *SMI = Spills[Reg];
              int FI;
              if (TII->isCopyInstr(*SMI)) {
                // If the reload is a simple copy, e.g. $rax = $rbx,
                // just replace the stackmap operand with the source of the
                // copy instruction.
                MachineOperand &Op = SMI->getOperand(1);
                Op.setIsKill(false);
                MIB.add(Op);
              } else if (TII->isLoadFromStackSlotPostFE(*SMI, FI)) {
                // If the reload is a load from the stack, replace the operand
                // with multiple operands describing a stack location.
                std::optional<LocationSize> Size = SMI->getRestoreSize(TII);
                if (!Size.has_value()) {
                  // This reload isn't a spill, e.g. this could be loading an
                  // argument passed via the stack and look like this:
                  //
                  //   mov rax, rbp + 10
                  //   STACKMAP rax
                  //
                  // We know that, after moving the stackmap, following the
                  // stackmap instruction we'll immediately write to the
                  // tracked register, so there is no need to track it at all.
                  // However, we can't just remove the operand as that would
                  // change the order of the tracked values and means we can't
                  // match them to LLVM IR operands anymore. Leaving the
                  // operand in also doesn't work as the verifier then
                  // sometimes complains about the use of undefined registers.
                  // Instead, we can just insert a constant `0xdead`, which has
                  // no consequences for deopt (it will just be ignored) but is
                  // something we can easily spot if things go wrong.
                  MIB.addImm(StackMaps::ConstantOp);
                  MIB.addImm(0xdead);
                } else {
                  MIB.addImm(StackMaps::IndirectMemRefOp);
                  MIB.addImm(Size.value().getValue()); // Size
                  MIB.add(SMI->getOperand(1));         // Register
                  MIB.add(SMI->getOperand(4));         // Offset
                }
              } else {
                // This tracked operand maps to an instruction that isn't a
                // spill, e.g. `$rax = lea $rbp-8`. We can't determine another
                // source so instead we insert a constant and hope that the
                // instruction has no dependencies and thus will correctly
                // recover the value after deopt.
                MIB.addImm(StackMaps::ConstantOp);
                MIB.addImm(0xdead);
              }
            }
            MOI++;
            continue;
          }
          // Copy all other operands over as is.
          MIB.add(*MOI);
          switch (MOI->getImm()) {
          default:
            llvm_unreachable("Unrecognized operand type.");
          case StackMaps::DirectMemRefOp: {
            MOI++;
            MIB.add(*MOI); // Register
            MOI++;
            MIB.add(*MOI); // Offset
            break;
          }
          case StackMaps::IndirectMemRefOp: {
            MOI++;
            MIB.add(*MOI); // Size
            MOI++;
            MIB.add(*MOI); // Register
            MOI++;
            MIB.add(*MOI); // Offset
            break;
          }
          case StackMaps::ConstantOp: {
            MOI++;
            MIB.add(*MOI);
            break;
          }
          case StackMaps::NextLive: {
            break;
          }
          }
          MOI++;
        }
        MachineOperand *Last = NewMI->operands_end() - 1;
        if (!Last->isReg() || !Last->isImplicit()) {
          // Unless the last operand is an implicit register, the last operand
          // needs to be `NextLive` due to the way stackmaps are parsed.
          MIB.addImm(StackMaps::NextLive);
        }
        // Insert the new stackmap instruction just after the last call.
        MI.getParent()->insertAfter(LastCall, NewMI);
        // Remember the old stackmap instruction for deletion later.
        Erased.insert(&MI);
        NewInstrs.insert(NewMI);
        LastCall = nullptr;
        Changed = true;
      }

      // Collect spill reloads that appear between a call and its corresponding
      // STACKMAP instruction.
      if (Collect) {
        for (MachineOperand &Op : MI.defs()) {
          // Collect each defined register and map it to its source
          // instruction.
          unsigned int Reg = getDwarfRegNum(Op.getReg(), TRI);
          Spills[Reg] = &MI;
        }
        if (TII->isCopyInstr(MI)) {
          unsigned int Op0 = getDwarfRegNum(MI.getOperand(0).getReg(), TRI);
          unsigned int Op1 = getDwarfRegNum(MI.getOperand(1).getReg(), TRI);
          if (TII->isCopyInstr(MI) && Spills.count(Op1)) {
            // The source for this copy instruction is itself a spill reload.
            // So we need to lookup the spill for the source and apply this
            // instead.
            Spills[Op0] = Spills[Op1];
          }
        }
      }
    }
    // Remove old stackmap instructions.
    for (MachineInstr *E : Erased) {
      E->eraseFromParent();
    }
    if (Erased.size() > 0) {
      // We have made changes so we need to recompute register liveness which
      // may no longer be accurate.
      recomputeLivenessFlags(MBB);
    }
  }
  if (Changed) {
    // If we've made any changes, verify them.
    MF.verify();
  }
  return Changed;
}
