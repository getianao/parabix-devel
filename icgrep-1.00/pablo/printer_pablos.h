/*
 *  Copyright (c) 2014 International Characters.
 *  This software is licensed to the public under the Open Software License 3.0.
 *  icgrep is a trademark of International Characters.
 */

#ifndef SHOW_H
#define SHOW_H

#include <pablo/pabloAST.h>
#include <pablo/ps_if.h>
#include <string>
#include <map>

namespace pablo {
    class PabloBlock;
}

class PabloPrinter {
public:
    using DefinedVars = std::vector<pablo::PabloAST *, pablo::PabloAST::VectorAllocator>;
    static void print(const pablo::PabloBlock & block, std::ostream & strm);
    static void print(const pablo::StatementList & stmts, std::ostream & strm);
    static void print(const pablo::StatementList & stmts, std::string indent, std::ostream & strm);
    static void print_vars(const DefinedVars & vars, std::string indent, std::ostream & strm);
    static void print(const pablo::PabloAST * expr, std::ostream & strm);
    static void print(const pablo::Statement *stmt, std::string indent, std::ostream & strm);

    static void printGPU(const pablo::PabloBlock & block, std::ostream & strm);
    static void printGPU(const pablo::StatementList & stmts, std::ostream & strm);
    static void printGPU(const pablo::StatementList & stmts, std::string indent, std::ostream & strm);
    static void printGPU_vars(const DefinedVars & vars, std::string indent, std::ostream & strm);
    static void printGPU(const pablo::PabloAST * expr, std::ostream & strm);
    static void printGPU(const pablo::Statement *stmt, std::string indent, std::ostream & strm);
    static std::string getExprName(const pablo::PabloAST *node);

    static int n_inst;
    static std::map<std::string, uint32_t> map_variable;
};

#endif // SHOW_H
