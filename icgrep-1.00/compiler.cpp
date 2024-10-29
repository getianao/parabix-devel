/*
 *  Copyright (c) 2015 International Characters.
 *  This software is licensed to the public under the Open Software License 3.0.
 *  icgrep is a trademark of International Characters.
 */

#include <compiler.h>
#include <re/re_cc.h>
#include <re/re_nullable.h>
#include <re/re_simplifier.h>
#include <re/re_alt.h>
#include <re/parsefailure.h>
#include <re/re_parser.h>
#include <re/re_compiler.h>
#include <utf8_encoder.h>
#include <cc/cc_compiler.h>
#include <cc/cc_namemap.hpp>
#include <pablo/pablo_compiler.h>
#include <pablo/optimizers/pablo_simplifier.hpp>
#include <pablo/optimizers/pablo_codesinking.hpp>
#include "UCD/precompiled_gc.h"
#include "UCD/precompiled_sc.h"
#include "UCD/precompiled_scx.h"
#include "UCD/precompiled_blk.h"
#include "UCD/precompiled_derivedcoreproperties.h"
#include "UCD/precompiled_proplist.h"

#include "resolve_properties.cpp"

#include "llvm/Support/CommandLine.h"
#include <re/printer_re.h>
#include <pablo/printer_pablos.h>

#include <fstream> 


cl::OptionCategory cRegexOutputOptions("Regex Dump Options",
                                      "These options control printing of intermediate regular expression structures.");

cl::OptionCategory dPabloDumpOptions("Pablo Dump Options",
                                      "These options control printing of intermediate Pablo code.");

static cl::opt<bool> PrintAllREs("print-REs", cl::init(false), cl::desc("print regular expression passes"), cl::cat(cRegexOutputOptions));
static cl::opt<bool> PrintParsedREs("print-parsed-REs", cl::init(false), cl::desc("print out parsed regular expressions"), cl::cat(cRegexOutputOptions));
static cl::opt<bool> PrintStrippedREs("print-stripped-REs", cl::init(false), cl::desc("print out REs with nullable prefixes/suffixes removed"), cl::cat(cRegexOutputOptions));
static cl::opt<bool> PrintNamedREs("print-named-REs", cl::init(false), cl::desc("print out named REs"), cl::cat(cRegexOutputOptions));
static cl::opt<bool> PrintUTF8REs("print-utf8-REs", cl::init(false), cl::desc("print out UTF-8 REs"), cl::cat(cRegexOutputOptions));
static cl::opt<bool> PrintSimplifiedREs("print-simplified-REs", cl::init(false), cl::desc("print out final simplified REs"), cl::cat(cRegexOutputOptions));
static cl::opt<bool> PrintCompiledCCcode("print-CC-pablo", cl::init(false), cl::desc("print Pablo output from character class compiler"), cl::cat(dPabloDumpOptions));
static cl::opt<bool> PrintCompiledREcode("print-RE-pablo", cl::init(false), cl::desc("print Pablo output from the regular expression compiler"), cl::cat(dPabloDumpOptions));
static cl::opt<std::string>
    PrintOptimizedREcode("print-pablo", cl::init("."),
                         cl::desc("print final optimized Pablo code"),
                         cl::cat(dPabloDumpOptions));

static cl::opt<std::string>
    PrintOptimizedREGPUcode("print-GPU-pablo", cl::init("."),
                            cl::desc("print final optimized Pablo GPU code"),
                            cl::cat(dPabloDumpOptions));

cl::OptionCategory cPabloOptimizationsOptions("Pablo Optimizations",
                                              "These options control Pablo optimization passes.");

static cl::opt<bool> DisablePabloCSE("disable-CSE", cl::init(false),
                                      cl::desc("Disable Pablo common subexpression elimination/dead code elimination"),
                                      cl::cat(cPabloOptimizationsOptions));
static cl::opt<bool> PabloSinkingPass("sinking", cl::init(false),
                                      cl::desc("Moves all instructions into the innermost legal If-scope so that they are only executed when needed."),
                                      cl::cat(cPabloOptimizationsOptions));

using namespace re;
using namespace cc;
using namespace pablo;

namespace icgrep {

CompiledPabloFunction compile(const Encoding encoding, const std::vector<std::string> regexps, const ModeFlagSet initialFlags) {
    std::vector<RE *> REs;
    RE * re_ast = nullptr;
    for (int i = 0; i < regexps.size(); i++) {
        try
        {
            re_ast = RE_Parser::parse(regexps[i], initialFlags);
        }
        catch (ParseFailure failure)
        {
            std::cerr << "Regex parsing failure: " << failure.what() << std::endl;
            std::cerr << regexps[i] << std::endl;
            exit(1);
        }
        REs.push_back(re_ast);
    }
    if (REs.size() > 1) {
        re_ast = makeAlt(REs.begin(), REs.end());
    }

    if (PrintAllREs || PrintParsedREs) {
      std::cerr << "Parser:" << std::endl << Printer_RE::PrintRE(re_ast) << std::endl;
    }

    //Optimization passes to simplify the AST.
    re_ast = RE_Nullable::removeNullablePrefix(re_ast);
    if (PrintAllREs || PrintStrippedREs) {
      std::cerr << "RemoveNullablePrefix:" << std::endl << Printer_RE::PrintRE(re_ast) << std::endl;
    }
    re_ast = RE_Nullable::removeNullableSuffix(re_ast);
    if (PrintAllREs || PrintStrippedREs) {
      std::cerr << "RemoveNullableSuffix:" << std::endl << Printer_RE::PrintRE(re_ast) << std::endl;
    }
    
    resolveProperties(re_ast);
    
    
    CC_NameMap nameMap;
    re_ast = nameMap.process(re_ast, UnicodeClass);

    if (PrintAllREs || PrintNamedREs) {
      std::cerr << "Namer:" << std::endl << Printer_RE::PrintRE(re_ast) << std::endl;
      std::cerr << "NameMap:\n" << nameMap.printMap() << std::endl;
    }

    //Add the UTF encoding.
    if (encoding.getType() == Encoding::Type::UTF_8) {
        re_ast = UTF8_Encoder::toUTF8(nameMap, re_ast);
        if (PrintAllREs || PrintUTF8REs) {
          //Print to the terminal the AST that was generated by the utf8 encoder.
          std::cerr << "UTF8-encoder:" << std::endl << Printer_RE::PrintRE(re_ast) << std::endl;
          std::cerr << "NameMap:\n" << nameMap.printMap() << std::endl;
        }
    }
    
    re_ast = RE_Simplifier::simplify(re_ast);
    if (PrintAllREs || PrintSimplifiedREs) {
      //Print to the terminal the AST that was generated by the simplifier.
      std::cerr << "Simplifier:" << std::endl << Printer_RE::PrintRE(re_ast) << std::endl;
    }

    SymbolGenerator symbolGenerator;
    PabloBlock & main = PabloBlock::Create(symbolGenerator);

    CC_Compiler cc_compiler(main, encoding);
    
    cc_compiler.compileByteClasses(re_ast);
    
    auto basisBits = cc_compiler.getBasisBits(nameMap);
    if (PrintCompiledCCcode) {
      //Print to the terminal the AST that was generated by the character class compiler.
      std::cerr << "CC AST:" << std::endl;
      PabloPrinter::print(main.statements(), std::cerr);
    }
    
    RE_Compiler re_compiler(main);
    re_compiler.initializeRequiredStreams(cc_compiler);
    re_compiler.finalizeMatchResult(re_compiler.compile(re_ast));
    if (PrintCompiledREcode) {
      //Print to the terminal the AST that was generated by the pararallel bit-stream compiler.
      std::cerr << "Initial Pablo AST:\n";
      PabloPrinter::print(main.statements(), std::cerr);
    }

    // Scan through the pablo code and perform DCE and CSE
    if (!DisablePabloCSE) {
        Simplifier::optimize(main);
    }
    if (PabloSinkingPass) {
        CodeSinking::optimize(main);
    }

    auto saveToFile = [](const std::string &filename, PabloBlock &main) {
      std::ofstream file(filename);
      if (!file.is_open()) {
        std::cerr << "Failed to open file!" << std::endl;
        exit(1);
      }
      std::ostream &out = file;
      PabloPrinter::print(main.statements(), out);
    };
    if (PrintOptimizedREcode != ".") {
      if (PrintOptimizedREcode.empty()) {
        // Print to the terminal the AST that was generated by the pararallel
        // bit-stream compiler.
        std::cerr << "Final Pablo AST:\n";
        PabloPrinter::print(main.statements(), std::cerr);
      } else {
        saveToFile(PrintOptimizedREcode, main);
        std::cerr << "optimized Pablo code to " << PrintOptimizedREcode
                  << std::endl;
      }
    }

    if (PrintOptimizedREGPUcode != ".") {
      if (PrintOptimizedREGPUcode.empty()) {
        // Print to the terminal the AST that was generated by the pararallel
        // bit-stream compiler.
        std::cerr << "Final Pablo GPU AST:\n";
        PabloPrinter::print(main.statements(), std::cerr);
      } else {
        saveToFile(PrintOptimizedREGPUcode, main);
        std::cerr << "optimized Pablo code to " << PrintOptimizedREGPUcode
                  << std::endl;
      }
    }

    PabloCompiler pablo_compiler(basisBits);
    
    install_property_gc_fn_ptrs(pablo_compiler);
    install_property_sc_fn_ptrs(pablo_compiler);
    install_property_scx_fn_ptrs(pablo_compiler);
    install_property_blk_fn_ptrs(pablo_compiler);
    install_property_DerivedCoreProperties_fn_ptrs(pablo_compiler);
    install_property_PropList_fn_ptrs(pablo_compiler);

    try {
        CompiledPabloFunction retVal = pablo_compiler.compile(main);
        releaseSlabAllocatorMemory();
        return retVal;
    }
    catch (std::runtime_error e) {
        releaseSlabAllocatorMemory();
        std::cerr << "Runtime error: " << e.what() << std::endl;
        exit(1);
    }
}

}
