/*
 *  Part of the Parabix Project, under the Open Software License 3.0.
 *  SPDX-License-Identifier: OSL-3.0
 */

#pragma once

#include <stdint.h>
#include <map>
#include <string>

namespace llvm { class raw_ostream; }

namespace pablo {

class PabloKernel;
class PabloBlock;
class Statement;
class PabloAST;

class PabloPrinter {
public:
    static void print(PabloAST const * node, llvm::raw_ostream & out) noexcept;
    static void print(PabloKernel const * kernel, llvm::raw_ostream & out) noexcept;
    static void print(PabloBlock const * block, llvm::raw_ostream & out, const bool expandNested = false, unsigned indent = 0) noexcept;
    static void print(Statement const * stmt, llvm::raw_ostream & out, const bool expandNested = false, unsigned indent = 0) noexcept;

    static void print_gpu(PabloBlock const *block, llvm::raw_ostream &out,
                          uint32_t &n_inst,
                          std::map<std::string, uint32_t> &map_variable,
                          const bool expandNested = false,
                          unsigned indent = 0) noexcept;
    static void print_gpu(PabloKernel const * kernel, llvm::raw_ostream & out) noexcept;
};


}
