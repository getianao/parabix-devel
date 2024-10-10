/*
 *  Part of the Parabix Project, under the Open Software License 3.0.
 *  SPDX-License-Identifier: OSL-3.0
 */

#pragma once

#include <string>
#include <llvm/Support/FileSystem.h>

namespace llvm { namespace cl { class OptionCategory; } }

namespace pablo {

enum PabloDebugFlags {
    VerifyPablo, DumpTrace
};
extern std::string ShowOptimizedPabloOption;
extern std::string ShowOptimizedPabloGPUOption;
extern std::string ShowPabloOption;

enum PabloCompilationFlags {
    Flatten,
    DisableSimplification,
    DisableCodeMotion,
    EnableDistribution,
    EnableSchedulingPrePass,
    EnableProfiling,
    EnableTernaryOpt
};

enum class PabloCarryMode {
    BitBlock,
    Compressed
};
extern PabloCarryMode CarryMode;

enum class BitMovementMode {
    Advance,
    LookAhead
};
extern BitMovementMode MovementMode;

std::string BitMovementMode_string(BitMovementMode m);

extern llvm::sys::fs::OpenFlags PabloOutputFileFlag;
extern llvm::sys::fs::OpenFlags PabloOptimizedOutputFileFlag;
extern llvm::sys::fs::OpenFlags PabloOptimizedGPUOutputFileFlag;

extern std::string PabloIllustrateBitstreamRegEx;

const llvm::cl::OptionCategory * pablo_toolchain_flags();

bool DebugOptionIsSet(const PabloDebugFlags flag);

bool CompileOptionIsSet(const PabloCompilationFlags flag);

}
