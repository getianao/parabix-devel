/*
 *  Copyright (c) 2014 International Characters.
 *  This software is licensed to the public under the Open Software License 3.0.
 *  icgrep is a trademark of International Characters.
 */

#include "printer_pablos.h"
#include <iostream>
#include <ostream>

//Regular Expressions
#include <re/re_re.h>
#include <re/re_cc.h>
#include <re/re_start.h>
#include <re/re_end.h>
#include <re/re_seq.h>
#include <re/re_name.h>

//Pablo Expressions
#include <pablo/pabloAST.h>
#include <pablo/pe_advance.h>
#include <pablo/pe_and.h>
#include <pablo/pe_call.h>
#include <pablo/pe_matchstar.h>
#include <pablo/pe_not.h>
#include <pablo/pe_or.h>
#include <pablo/pe_scanthru.h>
#include <pablo/pe_sel.h>
#include <pablo/pe_var.h>
#include <pablo/pe_xor.h>
#include <pablo/ps_assign.h>
#include <pablo/ps_if.h>
#include <pablo/ps_while.h>
#include <pablo/pe_zeroes.h>
#include <pablo/pe_ones.h>
#include <pablo/codegenstate.h>

#include <sstream>
#include <format>

using namespace re;
using namespace pablo;

static void get_operand_idx(std::string operand, uint32_t &operand_idx,
                            std::map<std::string, uint32_t> map_variable) {
  if (map_variable.find(operand) == map_variable.end()) {
    throw std::runtime_error("Error: operand variable not found: " + operand);
  }
  operand_idx = map_variable[operand];
};

// NOT, ASSIGN
static std::string getGPUInst(uint32_t n_inst, std::string operation,
                              std::string operand1, std::string dest,
                              std::map<std::string, uint32_t> &map_variable) {
  uint32_t operand1_idx, dest_idx;

  // operands
  get_operand_idx(operand1, operand1_idx, map_variable);

  // dest
  if (map_variable.find(dest) != map_variable.end()) {
    std::cerr << "Warning: dest variable already exists: " << dest << "\n";
    dest_idx = map_variable[dest];
  } else {
    dest_idx = map_variable.size();
    map_variable[dest] = dest_idx;
    // printf(" insert %s %d\n", dest.c_str(), dest_idx);
  }
  return std::format("ri[{0}].init(Re_Inst::{1}, {2}, {3});", n_inst, operation,
                     operand1_idx, dest_idx);
}

// SEL
static std::string getGPUInst(uint32_t n_inst, std::string operation,
                              std::string operand1, std::string operand2,
                              std::string operand3, std::string dest,
                              std::map<std::string, uint32_t> &map_variable) {
  uint32_t operand1_idx, operand2_idx, operand3_idx, dest_idx;

  // operands
  get_operand_idx(operand1, operand1_idx, map_variable);
  get_operand_idx(operand2, operand2_idx, map_variable);
  get_operand_idx(operand3, operand3_idx, map_variable);

  // dest
  if (map_variable.find(dest) != map_variable.end()) {
    std::cerr << "Warning: dest variable already exists: " << dest << "\n";
    dest_idx = map_variable[dest];
  } else {
    dest_idx = map_variable.size();
    map_variable[dest] = dest_idx;
    // printf(" insert %s %d\n", dest.c_str(), dest_idx);
  }
  return std::format("ri[{0}].init(Re_Inst::{1}, {2}, {3}, {4}, {5});", n_inst,
                     operation, operand1_idx, operand2_idx, operand3_idx,
                     dest_idx);
}

// ternary
static std::string getGPUInst(uint32_t n_inst, std::string operation,
                              std::string operand1, std::string operand2,
                              std::string operand3, std::string operand4,
                              std::string dest,
                              std::map<std::string, uint32_t> &map_variable) {
  uint32_t operand1_idx, operand2_idx, operand3_idx, operand4_idx, dest_idx;

  // operands
  get_operand_idx(operand2, operand2_idx, map_variable);
  get_operand_idx(operand3, operand3_idx, map_variable);
  get_operand_idx(operand4, operand4_idx, map_variable);

  // dest
  if (map_variable.find(dest) != map_variable.end()) {
    std::cerr << "Warning: dest variable already exists: " << dest << "\n";
    dest_idx = map_variable[dest];
  } else {
    dest_idx = map_variable.size();
    map_variable[dest] = dest_idx;
    // printf(" insert %s %d\n", dest.c_str(), dest_idx);
  }
  return std::format("ri[{0}].init(Re_Inst::{1}, {2}, {3}, {4}, {5}, {6});",
                     n_inst, operation, operand1, operand2_idx, operand3_idx,
                     operand4_idx, dest_idx);
}

static std::string getGPUInst(uint32_t n_inst, std::string operation,
                              std::string operand1, std::string operand2,
                              std::string dest,
                              std::map<std::string, uint32_t> &map_variable) {

  if (operation == "ADVANCE") {
    uint32_t operand1_idx;
    get_operand_idx(operand1, operand1_idx, map_variable);
    // dest
    uint32_t dest_idx;
    if (map_variable.find(dest) != map_variable.end()) {
      std::cerr << "Warning: dest variable already exists: " << dest << "\n";
      dest_idx = map_variable[dest];
    } else {
      dest_idx = map_variable.size();
      map_variable[dest] = dest_idx;
      // printf(" insert %s %d\n", dest.c_str(), dest_idx);
    }
    return std::format("ri[{0}].init(Re_Inst::{1}, {2}, {3}, {4});", n_inst,
                       operation, operand1_idx, operand2, dest_idx);
  }

  if (operation == "IF" || operation == "WHILE") {
    uint32_t operand1_idx;
    get_operand_idx(operand1, operand1_idx, map_variable);
    return std::format("ri[{0}].init(Re_Inst::{1}, {2}, {3}, {4});", n_inst,
                       operation, operand1_idx, operand2, dest);
  }

  uint32_t operand1_idx, operand2_idx, dest_idx;

  // operands
  get_operand_idx(operand1, operand1_idx, map_variable);
  get_operand_idx(operand2, operand2_idx, map_variable);

  // dest
  if (map_variable.find(dest) != map_variable.end()) {
    std::cerr << "Warning: dest variable already exists: " << dest << "\n";
    dest_idx = map_variable[dest];
  } else {
    dest_idx = map_variable.size();
    map_variable[dest] = dest_idx;
    // printf(" insert %s %d\n", dest.c_str(), dest_idx);
  }

  return std::format("ri[{0}].init(Re_Inst::{1}, {2}, {3}, {4});", n_inst,
                     operation, operand1_idx, operand2_idx, dest_idx);
}

int PabloPrinter::n_inst = 0;
std::map<std::string, uint32_t> PabloPrinter::map_variable;

std::string PabloPrinter::getExprName(const PabloAST *node) {
  std::ostringstream os;
  printGPU(node, os);
  return os.str();
}


void PabloPrinter::printGPU(const PabloBlock & block, std::ostream & strm)
{
    printGPU(block.statements(), "  ", strm);
}

void PabloPrinter::printGPU(const StatementList & stmts, std::ostream & strm) {
    n_inst = 0;
    for (int i = 0; i < 8; i++) {
      std::string variable = "basis" + std::to_string(i);
      map_variable[variable] = map_variable.size();
    }
    map_variable["0"] = map_variable.size();
    map_variable["1"] = map_variable.size();
    printGPU(stmts, "  ", strm);
}

void PabloPrinter::printGPU(const StatementList & stmts, std::string indent, std::ostream & strm) {
    for (const Statement * stmt : stmts) {
        printGPU(stmt, indent, strm);
        if (!isa<If>(stmt) && !isa<While>(stmt))
          strm << std::endl;
    }
}

void PabloPrinter::printGPU_vars(const DefinedVars & vars, std::string indent, std::ostream & strm) {
  for (int i = 0; i < vars.size(); i++) {
    PabloAST *v = vars[i];
    std::string operand1 = "0", dest;
    dest = getExprName(dyn_cast<Assign>(v));
    if (i != 0)
      strm << indent;
    strm << getGPUInst(n_inst, "ASSIGN", operand1, dest, map_variable);
    strm << "  // ";
    strm << dyn_cast<Assign>(v)->getName() << " = 0\n";
    n_inst++;
  }
}

void PabloPrinter::printGPU(const Statement * stmt, std::string indent, std::ostream & strm) {
    strm << indent;
    if (stmt == nullptr) {
        strm << "<null-stmt>";
    }
    else if (const Assign * an = dyn_cast<const Assign>(stmt)) {
      std::string operand1, dest;
      dest = getExprName(an);
      if (an->isOutputAssignment()) {
        dest = "output." + dest;
      }
      operand1 = getExprName(an->getExpr());

      strm << getGPUInst(n_inst, "ASSIGN", operand1, dest, map_variable);
      n_inst++;
    }
    else if (const Next * next = dyn_cast<const Next>(stmt)) {        
        // strm << "Next(" << next->getName() << ") = ";
        // print(next->getExpr(), strm);
        // throw std::runtime_error("Error: Next not supported");
        std::string operand1, dest;
        dest = getExprName(next);
        operand1 = getExprName(next->getExpr());
        strm << getGPUInst(n_inst, "ASSIGN", operand1, dest, map_variable);
        n_inst++;
    }
    else if (const If * ifstmt = dyn_cast<const If>(stmt)) {
        printGPU_vars(ifstmt->getDefined(), indent + "", strm);

        std::string condition, pos_true, pos_false;
        condition = getExprName(ifstmt->getCondition());
        uint32_t pos_if = n_inst;
        n_inst++;
        std::ostringstream bodyos;
        printGPU(ifstmt->getBody(), indent + "  ", bodyos);
        pos_true = std::to_string(pos_if + 1);
        pos_false = std::to_string(n_inst);
        // strm << indent;
        strm << getGPUInst(pos_if, "IF", condition, pos_true, pos_false,
                          map_variable);
        strm << "  // " << "if ";
        print(ifstmt->getCondition(), strm);
        strm << "\n";               
        strm << bodyos.str();
    }
    else if (const While * whl = dyn_cast<const While>(stmt)) {
        std::string condition, pos_true, pos_false;
        condition = getExprName(whl->getCondition());
        uint32_t pos_while = n_inst;
        n_inst++;
        std::ostringstream bodyos;
        printGPU(whl->getBody(), indent + "  ", bodyos);
        pos_true = std::to_string(pos_while + 1);
        pos_false = std::to_string(n_inst);
        // strm << indent;
        strm << getGPUInst(pos_while, "WHILE", condition, pos_true, pos_false,
                          map_variable);
        strm << "  // " << "while ";
        print(whl->getCondition(), strm);
        strm << "\n";               
        strm << bodyos.str();
    }
    else if (const Call * pablo_call = dyn_cast<const Call>(stmt)) {
        print(pablo_call, strm);
        strm << " = " << pablo_call->getCallee() << "()";
        throw std::runtime_error("Error: Call not supported");
    } else if (const And *pablo_and = dyn_cast<const And>(stmt)) {
      std::string operand1, operand2, dest;
      dest = getExprName(pablo_and);
      operand1 = getExprName(pablo_and->getExpr1());
      operand2 = getExprName(pablo_and->getExpr2());
      strm << getGPUInst(n_inst, "AND", operand1, operand2, dest, map_variable);
      n_inst++;
    } else if (const Or *pablo_or = dyn_cast<const Or>(stmt)) {
      std::string operand1, operand2, dest;
      dest = getExprName(pablo_or);
      operand1 = getExprName(pablo_or->getExpr1());
      operand2 = getExprName(pablo_or->getExpr2());
      strm << getGPUInst(n_inst, "OR", operand1, operand2, dest, map_variable);
      n_inst++;
    } else if (const Xor *pablo_xor = dyn_cast<const Xor>(stmt)) {
      std::string operand1, operand2, dest;
      dest = getExprName(pablo_xor);
      operand1 = getExprName(pablo_xor->getExpr1());
      operand2 = getExprName(pablo_xor->getExpr2());
      strm << getGPUInst(n_inst, "XOR", operand1, operand2, dest, map_variable);
      n_inst++;
    } else if (const Sel *pablo_sel = dyn_cast<const Sel>(stmt)) {
      std::string operand1, operand2, operand3, dest;
      dest = getExprName(pablo_sel);
      operand1 = getExprName(pablo_sel->getCondition());
      operand2 = getExprName(pablo_sel->getTrueExpr());
      operand3 = getExprName(pablo_sel->getFalseExpr());
      strm << getGPUInst(n_inst, "SEL", operand1, operand2, operand3, dest, map_variable);
      n_inst++;
    } else if (const Not *pablo_not = dyn_cast<const Not>(stmt)) {
      std::string operand1, dest;
      dest = getExprName(pablo_not);
      operand1 = getExprName(pablo_not->getExpr());
      strm << getGPUInst(n_inst, "NOT", operand1, dest, map_variable);
      n_inst++;
    } else if (const Advance *adv = dyn_cast<const Advance>(stmt)) {
      std::string operand1, operand2, dest;
      dest = getExprName(adv);
      operand1 = getExprName(adv->getExpr());
      operand2 = std::to_string(adv->getAdvanceAmount());
      strm << getGPUInst(n_inst, "ADVANCE", operand1, operand2, dest, map_variable);
      n_inst++;
    } else if (const MatchStar *mstar = dyn_cast<const MatchStar>(stmt)) {
      std::string operand1, operand2, dest;
      dest = getExprName(mstar);
      operand1 = getExprName(mstar->getMarker());
      operand2 = getExprName(mstar->getCharClass());
      strm << getGPUInst(n_inst, "MATCHSTAR", operand1, operand2, dest, map_variable);
      n_inst++;
    } else if (const ScanThru *sthru = dyn_cast<const ScanThru>(stmt)) {
      std::string operand1, operand2, dest;
      dest = getExprName(sthru);
      operand1 = getExprName(sthru->getScanFrom());
      operand2 = getExprName(sthru->getScanThru());
      strm << getGPUInst(n_inst, "SCANTHRU", operand1, operand2, dest, map_variable);
      n_inst++;
    } else {
      strm << indent << "**UNKNOWN Pablo Statement type **" << std::endl;
    }

    if (!isa<const If>(stmt) && !isa<const While>(stmt)) {
      strm << "  // ";
      print(stmt, "", strm);
    }
}

void PabloPrinter::printGPU(const PabloAST * expr, std::ostream & strm) {
    if (expr == nullptr) {
        strm << "<null-expr>";
    }
    else if (isa<const Zeroes>(expr)) {
        strm << "0";
    }
    else if (isa<const Ones>(expr)) {
        strm << "1";
    }
    else if (const Var * var = dyn_cast<const Var>(expr)) {
        strm  << var->getName();
    }
    else if (const Statement * stmt = dyn_cast<Statement>(expr)) {
        strm << stmt->getName();
    }
    else {
        strm << "**UNKNOWN Pablo Expression type **\n" << std::endl;
    }
}
