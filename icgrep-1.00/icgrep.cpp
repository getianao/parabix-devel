/*
 *  Copyright (c) 2014 International Characters.
 *  This software is licensed to the public under the Open Software License 3.0.
 *  icgrep is a trademark of International Characters.
 */

#include <string>
#include <iostream>
#include <fstream>

#include "icgrep.h"
#include "utf_encoding.h"
#include "compiler.h"
#include "pablo/pablo_compiler.h"
#include "do_grep.h"

#include "llvm/Support/CommandLine.h"

int main(int argc, char *argv[]) {
    StringMap<cl::Option*> Map;
    cl::getRegisteredOptions(Map);
    Map["time-passes"]->setHiddenFlag(cl::Hidden);
    Map["disable-spill-fusing"]->setHiddenFlag(cl::Hidden);
    Map["enable-misched"]->setHiddenFlag(cl::Hidden);
    Map["enable-tbaa"]->setHiddenFlag(cl::Hidden);
    Map["exhaustive-register-search"]->setHiddenFlag(cl::Hidden);
    Map["fatal-assembler-warnings"]->setHiddenFlag(cl::Hidden);
    Map["join-liveintervals"]->setHiddenFlag(cl::Hidden);
    Map["limit-float-precision"]->setHiddenFlag(cl::Hidden);
    Map["mc-x86-disable-arith-relaxation"]->setHiddenFlag(cl::Hidden);
    Map["limit-float-precision"]->setHiddenFlag(cl::Hidden);
    Map["print-after-all"]->setHiddenFlag(cl::Hidden);
    Map["print-before-all"]->setHiddenFlag(cl::Hidden);
    Map["print-machineinstrs"]->setHiddenFlag(cl::Hidden);
    Map["regalloc"]->setHiddenFlag(cl::Hidden);
    Map["rng-seed"]->setHiddenFlag(cl::Hidden);
    Map["spiller"]->setHiddenFlag(cl::Hidden);
    Map["stackmap-version"]->setHiddenFlag(cl::Hidden);
    Map["x86-asm-syntax"]->setHiddenFlag(cl::Hidden);
    Map["verify-debug-info"]->setHiddenFlag(cl::Hidden);
    Map["verify-dom-info"]->setHiddenFlag(cl::Hidden);
    Map["verify-loop-info"]->setHiddenFlag(cl::Hidden);
    Map["verify-regalloc"]->setHiddenFlag(cl::Hidden);
    Map["verify-scev"]->setHiddenFlag(cl::Hidden);
    
    
    cl::OptionCategory aRegexSourceOptions("Regular Expression Options",
                                          "These options control the regular expression source.");
    
    cl::OptionCategory bGrepOutputOptions("Output Options",
                                         "These options control the output.");

    cl::OptionCategory cInputstreamOptions("Input Options",
                                         "These options control the Input stream.");
    
    int firstInputFile = 1;  // Normal case when first positional arg is a regex.
    cl::list<std::string> inputFiles(cl::Positional, cl::desc("<regex> <input file ...>"), cl::OneOrMore);
    
    cl::opt<bool> CountOnly("c", cl::desc("Count and display the matching lines per file only."), cl::cat(bGrepOutputOptions));
    cl::alias CountOnlyLong("count", cl::desc("Alias for -c"), cl::aliasopt(CountOnly));
    cl::opt<bool> NormalizeLineBreaks("normalize-line-breaks", cl::desc("Normalize line breaks to std::endl."), cl::init(false),  cl::cat(bGrepOutputOptions));
    
    cl::opt<bool> ShowFileNames("H", cl::desc("Show the file name with each matching line."), cl::cat(bGrepOutputOptions));
    cl::alias ShowFileNamesLong("with-filename", cl::desc("Alias for -H"), cl::aliasopt(ShowFileNames));
    
    cl::opt<bool> CaseInsensitive("i", cl::desc("Ignore case distinctions in the pattern and the file."), cl::cat(aRegexSourceOptions));
    cl::opt<bool> ShowLineNumbers("n", cl::desc("Show the line number with each matching line."), cl::cat(bGrepOutputOptions));
    cl::alias ShowLineNumbersLong("line-number", cl::desc("Alias for -n"), cl::aliasopt(ShowLineNumbers));
    
    cl::list<std::string> regexVector("e", cl::desc("Regular expression"), cl::ZeroOrMore, cl::cat(aRegexSourceOptions));
    
    cl::opt<std::string> RegexFilename("f", cl::desc("Take regular expressions (one per line) from a file"), cl::value_desc("regex file"), cl::init(""), cl::cat(aRegexSourceOptions));
  
    // Does -b mean the byte offset of the line, or the byte offset of the match start within the line?
    //static cl::opt<bool>ShowByteOffsets("b", cl::desc("Show the byte offset with each matching line."));
    //cl::alias ShowByteOffsetsLong("-byte-offset", cl::desc("Alias for -b"), cl::aliasopt(ShowByteOffsets));

    cl::opt<int> DuplicateInput("d", cl::desc("Duplicate the input file"), cl::init(1), cl::cat(cInputstreamOptions));
    cl::opt<int> ByteInputstream("s", cl::desc("Byte size of input stream"), cl::init(-1), cl::cat(cInputstreamOptions)); 



    Encoding encoding(Encoding::Type::UTF_8, 8);

    cl::ParseCommandLineOptions(argc, argv);
    
    //std::vector<std::string> regexVector;
    if (RegexFilename != "") {
        std::ifstream regexFile(RegexFilename.c_str());
        std::string r;
        if (regexFile.is_open()) {
            while (std::getline(regexFile, r)) {
                regexVector.push_back(r);
            }
            regexFile.close();
        }
    }
    
    // if there are no regexes specified through -e or -f, the first positional argument
    // must be a regex, not an input file.
    
    if (regexVector.size() == 0) {
        regexVector.push_back(inputFiles[0]);
        firstInputFile = 1;
    }
    else {
        firstInputFile = 0;
    }
    
    re::ModeFlagSet globalFlags = 0;
    if (CaseInsensitive) globalFlags |= re::CASE_INSENSITIVE_MODE_FLAG;
    
    const auto llvm_codegen = icgrep::compile(encoding, regexVector, globalFlags);

    if (llvm_codegen.FunctionPointer) {
        void (*FP)(const Basis_bits &basis_bits, BitBlock carry_q[], BitBlock advance_q[], Output &output) = 
           (void (*)(const Basis_bits &basis_bits, BitBlock carry_q[], BitBlock advance_q[], Output &output))(void*)llvm_codegen.FunctionPointer;
        GrepExecutor grepEngine = GrepExecutor(llvm_codegen.CarryQueueSize, llvm_codegen.AdvanceQueueSize, FP);
        grepEngine.setCountOnlyOption(CountOnly);
        grepEngine.setNormalizeLineBreaksOption(NormalizeLineBreaks);
        grepEngine.setShowLineNumberOption(ShowLineNumbers);
	    if (inputFiles.size() > (firstInputFile + 1) || ShowFileNames) {
            grepEngine.setShowFileNameOption();
        }
        for (unsigned i = firstInputFile; i != inputFiles.size(); ++i) {
            grepEngine.doGrep(inputFiles[i].c_str(), DuplicateInput, ByteInputstream);
        }
    }
    
    return 0;
}

