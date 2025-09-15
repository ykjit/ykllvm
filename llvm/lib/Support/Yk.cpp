#include "llvm/Support/Yk.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ManagedStatic.h"

using namespace llvm;

bool YkExtendedLLVMBBAddrMapSection;
namespace {
struct CreateYkExtendedLLVMBBAddrMapSectionParser {
  static void *call() {
    return new cl::opt<bool, true>(
        "yk-extended-llvmbbaddrmap-section",
        cl::desc("Use the extended Yk `.llvmbbaddrmap` section format"),
        cl::NotHidden, cl::location(YkExtendedLLVMBBAddrMapSection));
  }
};
} // namespace
static ManagedStatic<cl::opt<bool, true>,
                     CreateYkExtendedLLVMBBAddrMapSectionParser>
    YkExtendedLLVMBBAddrMapSectionParser;

bool YkStackMapOffsetFix;
namespace {
struct CreateYkStackMapOffsetFixParser {
  static void *call() {
    return new cl::opt<bool, true>(
        "yk-stackmap-offset-fix",
        cl::desc(
            "Apply a fix to stackmaps that corrects the reported instruction "
            "offset in the presence of calls. (deprecated by "
            "yk-stackmap-spillreloads-fix)"),
        cl::NotHidden, cl::location(YkStackMapOffsetFix));
  }
};
} // namespace
static ManagedStatic<cl::opt<bool, true>, CreateYkStackMapOffsetFixParser>
    YkStackMapOffsetFixParser;

bool YkStackMapAdditionalLocs;
namespace {
struct CreateYkStackMapAdditionalLocsParser {
  static void *call() {
    return new cl::opt<bool, true>(
        "yk-stackmap-add-locs",
        cl::desc("Encode additional locations for registers into stackmaps."),
        cl::NotHidden, cl::location(YkStackMapAdditionalLocs));
  }
};
} // namespace
static ManagedStatic<cl::opt<bool, true>, CreateYkStackMapAdditionalLocsParser>
    YkStackMapAdditionalLocsParser;

bool YkStackmapsSpillReloadsFix;
namespace {
struct CreateYkStackmapsSpillReloadsFixParser {
  static void *call() {
    return new cl::opt<bool, true>(
        "yk-stackmap-spillreloads-fix",
        cl::desc(
            "Revert stackmaps and its operands after the register allocator "
            "has emitted spill reloads."),
        cl::NotHidden, cl::location(YkStackmapsSpillReloadsFix));
  }
};
} // namespace
static ManagedStatic<cl::opt<bool, true>,
                     CreateYkStackmapsSpillReloadsFixParser>
    YkStackmapsSpillFixParser;

bool YkMarkTraceableOptNone;
namespace {
struct CreateYkMarkTraceableOptNoneParser {
  static void *call() {
    return new cl::opt<bool, true>(
        "yk-mark-traceable-optnone-after-ir-passes",
        cl::desc(
            "Apply `optnone` to all traceable functions prior to instruction selection."),
        cl::NotHidden, cl::location(YkMarkTraceableOptNone));
  }
};
} // namespace
static ManagedStatic<cl::opt<bool, true>,
  CreateYkMarkTraceableOptNoneParser> YkMarkTraceableOptNoneParser;

bool YkEmbedIR;
namespace {
struct CreateYkEmbedIRParser {
  static void *call() {
    return new cl::opt<bool, true>(
        "yk-embed-ir",
        cl::desc(
            "Embed Yk IR into the binary."),
        cl::NotHidden, cl::location(YkEmbedIR));
  }
};
} // namespace
static ManagedStatic<cl::opt<bool, true>, CreateYkEmbedIRParser> YkEmbedIRParser;

bool YkDontOptFuncABI;
namespace {
struct CreateYkDontOptFuncABIParser {
  static void *call() {
    return new cl::opt<bool, true>(
        "yk-dont-opt-func-abi",
        cl::desc(
            "Don't change the ABIs of functions during optimisation"),
        cl::NotHidden, cl::location(YkDontOptFuncABI));
  }
};
} // namespace
static ManagedStatic<cl::opt<bool, true>, CreateYkDontOptFuncABIParser> YkDontOptFuncABIParser;

bool YkPatchCtrlPoint;
namespace {
struct CreateYkPatchCtrlPointParser {
  static void *call() {
    return new cl::opt<bool, true>(
        "yk-patch-control-point",
        cl::desc("Patch yk control points"),
        cl::NotHidden, cl::location(YkPatchCtrlPoint));
  }
};
} // namespace
static ManagedStatic<cl::opt<bool, true>, CreateYkPatchCtrlPointParser> YkPatchCtrlPointParser;

bool YkPatchIdempotent;
namespace {
struct CreateYkPatchIdempotentParser {
  static void *call() {
    return new cl::opt<bool, true>(
        "yk-patch-idempotent",
        cl::desc("Patch in idempotent function recorders"),
        cl::NotHidden, cl::location(YkPatchIdempotent));
  }
};
} // namespace
static ManagedStatic<cl::opt<bool, true>, CreateYkPatchIdempotentParser> YkPatchIdempotentParser;

bool YkOutlineUntraceable;
namespace {
struct CreateYkOutlineUntraceableParser {
  static void *call() {
    return new cl::opt<bool, true>(
        "yk-outline-untraceable",
        cl::desc("Mark functions that could never be traced with `yk_outline`"),
        cl::NotHidden, cl::location(YkOutlineUntraceable));
  }
};
} // namespace
static ManagedStatic<cl::opt<bool, true>, CreateYkOutlineUntraceableParser> YkOutlineUntraceableParser;

void llvm::initYkOptions() {
  *YkExtendedLLVMBBAddrMapSectionParser;
  *YkStackMapOffsetFixParser;
  *YkStackMapAdditionalLocsParser;
  *YkStackmapsSpillFixParser;
  *YkMarkTraceableOptNoneParser;
  *YkEmbedIRParser;
  *YkDontOptFuncABIParser;
  *YkPatchCtrlPointParser;
  *YkPatchIdempotentParser;
  *YkOutlineUntraceableParser;
}
