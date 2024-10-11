/*
 *  Part of the Parabix Project, under the Open Software License 3.0.
 *  SPDX-License-Identifier: OSL-3.0
 */

#include <pablo/printer_pablos.h>

#include <pablo/arithmetic.h>
#include <pablo/boolean.h>
#include <pablo/branch.h>
#include <pablo/codegenstate.h>
#include <pablo/pablo_intrinsic.h>
#include <pablo/pablo_kernel.h>
#include <pablo/pe_advance.h>
#include <pablo/pe_count.h>
#include <pablo/pe_debugprint.h>
#include <pablo/pe_everynth.h>
#include <pablo/pe_infile.h>
#include <pablo/pe_integer.h>
#include <pablo/pe_lookahead.h>
#include <pablo/pe_matchstar.h>
#include <pablo/pe_ones.h>
#include <pablo/pe_pack.h>
#include <pablo/pe_repeat.h>
#include <pablo/pe_scanthru.h>
#include <pablo/pe_string.h>
#include <pablo/pe_var.h>
#include <pablo/pe_zeroes.h>
#include <pablo/ps_assign.h>
#include <pablo/ps_terminate.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/DerivedTypes.h>  // for get getSequentialElementType
#include <llvm/Support/raw_os_ostream.h>

#include <format>
#include <map>

#define INDENT_WIDTH 4

using namespace llvm;

namespace pablo {

static void PrintStatement(Statement const * stmt, raw_ostream & out, const bool expandNested, unsigned indent) noexcept;
static void PrintExpression(PabloAST const * expr, raw_ostream & out) noexcept;


static void PrintStatement(Statement const * stmt, raw_ostream & out, const bool expandNested, unsigned indent) noexcept {
    out.indent(indent);
    if (stmt == nullptr) {
        out << "<null-stmt>";
    } else if (const Assign * assign = dyn_cast<Assign>(stmt)) {
        PrintExpression(assign->getVariable(), out);
        out << " = ";
        PrintExpression(assign->getValue(), out);
        out << "\n";
    } else if (const Branch * br = dyn_cast<Branch>(stmt)) {
        if (isa<If>(br)) {
            out << "if ";
        } else if (isa<While>(br)) {
            out << "while ";
        }
        PrintExpression(br->getCondition(), out);
        if (expandNested) {
            out << " {\n";
            PabloPrinter::print(br->getBody(), out, expandNested, indent + INDENT_WIDTH);
            out.indent(indent) << "}\n";
        } else {
            out << " {...}\n";
        }
    } else {

        PrintExpression(cast<PabloAST>(stmt), out);
        out << " = ";

        if (const And * andNode = dyn_cast<And>(stmt)) {
            for (unsigned i = 0; i != andNode->getNumOperands(); ++i) {
                if (i) out << " & ";
                PrintExpression(andNode->getOperand(i), out);
            }
        } else if (const Or * orNode = dyn_cast<Or>(stmt)) {
            for (unsigned i = 0; i != orNode->getNumOperands(); ++i) {
                if (i) out << " | ";
                PrintExpression(orNode->getOperand(i), out);
            }
        } else if (const Xor * xorNode = dyn_cast<Xor>(stmt)) {
            for (unsigned i = 0; i != xorNode->getNumOperands(); ++i) {
                if (i) out << " ^ ";
                PrintExpression(xorNode->getOperand(i), out);
            }
        } else if (const Sel * selNode = dyn_cast<Sel>(stmt)) {
            out << "Sel(";
            PrintExpression(selNode->getCondition(), out);
            out << ", ";
            PrintExpression(selNode->getTrueExpr(), out);
            out << ", ";
            PrintExpression(selNode->getFalseExpr(), out);
            out << ")";
        } else if (const Not * notNode = dyn_cast<Not>(stmt)) {
            out << "~";
            PrintExpression(notNode->getExpr(), out);
        } else if (IntrinsicCall const * call = dyn_cast<IntrinsicCall>(stmt)) {
            out << call->getIntrinsicName() << "(";
            for (size_t i = 0; i < call->getNumOperands(); ++i) {
                if (i) out << ", ";
                PrintExpression(call->getOperand(i), out);
            }
            out << ")";
        } else if (const Advance * adv = dyn_cast<Advance>(stmt)) {
            out << "Advance(";
            PrintExpression(adv->getExpression(), out);
            out << ", " << std::to_string(adv->getAmount()) << ")";
        } else if (const IndexedAdvance * adv = dyn_cast<IndexedAdvance>(stmt)) {
            out << "IndexedAdvance(";
            PrintExpression(adv->getExpression(), out);
            out << ", ";
            PrintExpression(adv->getIndex(), out);
            out << ", " << std::to_string(adv->getAmount()) << ")";
        } else if (const Lookahead * adv = dyn_cast<Lookahead>(stmt)) {
            out << "Lookahead(";
            PrintExpression(adv->getExpression(), out);
            out << ", " << std::to_string(adv->getAmount()) << ")";
        } else if (const MatchStar * mstar = dyn_cast<MatchStar>(stmt)) {
            out << "MatchStar(";
            PrintExpression(mstar->getMarker(), out);
            out << ", ";
            PrintExpression(mstar->getCharClass(), out);
            out << ")";
        } else if (const ScanThru * sthru = dyn_cast<ScanThru>(stmt)) {
            out << "ScanThru(";
            PrintExpression(sthru->getScanFrom(), out);
            out << ", ";
            PrintExpression(sthru->getScanThru(), out);
            out << ")";
        } else if (const ScanTo * sto = dyn_cast<ScanTo>(stmt)) {
            out << "ScanTo(";
            PrintExpression(sto->getScanFrom(), out);
            out << ", ";
            PrintExpression(sto->getScanTo(), out);
            out << ")";
        } else if (const AdvanceThenScanThru * sthru = dyn_cast<AdvanceThenScanThru>(stmt)) {
            out << "AdvanceThenScanThru(";
            PrintExpression(sthru->getScanFrom(), out);
            out << ", ";
            PrintExpression(sthru->getScanThru(), out);
            out << ")";
        } else if (const AdvanceThenScanTo * sto = dyn_cast<AdvanceThenScanTo>(stmt)) {
            out << "AdvanceThenScanTo(";
            PrintExpression(sto->getScanFrom(), out);
            out << ", ";
            PrintExpression(sto->getScanTo(), out);
            out << ")";
        } else if (const Ternary * tern = dyn_cast<Ternary>(stmt)) {
            out << "Ternary(";
            out.write_hex(tern->getMask()->value());
            out << ", ";
            PrintExpression(tern->getA(), out);
            out << ", ";
            PrintExpression(tern->getB(), out);
            out << ", ";
            PrintExpression(tern->getC(), out);
            out << ")";
        } else if (const Count * count = dyn_cast<Count>(stmt)) {
            out << "Count(";
            PrintExpression(count->getExpr(), out);
            out << ")";
        } else if (const Repeat * splat = dyn_cast<Repeat>(stmt)) {
            out << "Repeat(";
            PrintExpression(splat->getFieldWidth(), out);
            out << ", ";
            auto fw = splat->getValue()->getType()->getIntegerBitWidth();
            out << "Int" << fw << "(";
            PrintExpression(splat->getValue(), out);
            out << "))";
        } else if (const PackH * p = dyn_cast<PackH>(stmt)) {
            out << " = PackH(";
            PrintExpression(p->getFieldWidth(), out);
            out << ", ";
            PrintExpression(p->getValue(), out);
            out << ")";
        } else if (const PackL * p = dyn_cast<PackL>(stmt)) {
            out << " = PackL(";
            PrintExpression(p->getFieldWidth(), out);
            out << ", ";
            PrintExpression(p->getValue(), out);
            out << ")";
        } else if (const DebugPrint * e = dyn_cast<DebugPrint>(stmt)) {
            out << "DebugPrint(";
            PrintExpression(e->getExpr(), out);
            out << ")";
        } else if (const InFile * e = dyn_cast<InFile>(stmt)) {
            out << "InFile(";
            PrintExpression(e->getExpr(), out);
            out << ")";
        } else if (const AtEOF * e = dyn_cast<AtEOF>(stmt)) {
            out << "AtEOF(";
            PrintExpression(e->getExpr(), out);
            out << ")";
        } else if (const TerminateAt * s = dyn_cast<TerminateAt>(stmt)) {
            out << "TerminateAt(";
            PrintExpression(s->getExpr(), out);
            out << ", " << std::to_string(s->getSignalCode()) << ")";
        } else if (const EveryNth * e = dyn_cast<EveryNth>(stmt)) {
            out << "EveryNth(";
            out << e->getN()->value();
            out << ", ";
            PrintExpression(e->getExpr(), out);
            out << ")";
        } else {
            out << "???";
        }

        out << "\n";
    }
}

static void PrintExpression(PabloAST const * expr, raw_ostream & out) noexcept {
    assert (expr);
    if (isa<Zeroes>(expr)) {
        out << "<0>";
    } else if (const If * ifStmt = dyn_cast<If>(expr)) {
        out << "if ";
        PrintExpression(ifStmt->getCondition(), out);
        out << " {...}";
    } else if (const While * whileStmt = dyn_cast<While>(expr)) {
        out << "while ";
        PrintExpression(whileStmt->getCondition(), out);
        out << " {...}";
    } else if (isa<Ones>(expr)) {
        out << "<1>";
    } else if (const Integer * i = dyn_cast<Integer>(expr)) {
        out << i->value();
    } else if (Extract const * extract = dyn_cast<Extract>(expr)) {
        PrintExpression(extract->getArray(), out);
        out << "[";
        PrintExpression(extract->getIndex(), out);
        out << "]";
    } else if (Var const * var = dyn_cast<Var>(expr)) {
        out << var->getName();
    } else if (isa<Integer>(expr)) {
        out << cast<Integer>(expr)->value();
    } else if (const Add * op = dyn_cast<Add>(expr)) {
        PrintExpression(op->getLH(), out);
        out << " + ";
        PrintExpression(op->getRH(), out);
    } else if (const Subtract * op = dyn_cast<Subtract>(expr)) {
        PrintExpression(op->getLH(), out);
        out << " - ";
        PrintExpression(op->getRH(), out);
    } else if (const LessThan * op = dyn_cast<LessThan>(expr)) {
        PrintExpression(op->getLH(), out);
        out << " < ";
        PrintExpression(op->getRH(), out);
    } else if (const LessThanEquals * op = dyn_cast<LessThanEquals>(expr)) {
        PrintExpression(op->getLH(), out);
        out << " <= ";
        PrintExpression(op->getRH(), out);
    } else if (const Equals * op = dyn_cast<Equals>(expr)) {
        PrintExpression(op->getLH(), out);
        out << " == ";
        PrintExpression(op->getRH(), out);
    } else if (const GreaterThanEquals * op = dyn_cast<GreaterThanEquals>(expr)) {
        PrintExpression(op->getLH(), out);
        out << " >= ";
        PrintExpression(op->getRH(), out);
    } else if (const GreaterThan * op = dyn_cast<GreaterThan>(expr)) {
        PrintExpression(op->getLH(), out);
        out << " > ";
        PrintExpression(op->getRH(), out);
    } else if (const NotEquals * op = dyn_cast<NotEquals>(expr)) {
        PrintExpression(op->getLH(), out);
        out << " != ";
        PrintExpression(op->getRH(), out);
    } else if (const Statement * stmt = dyn_cast<Statement>(expr)) {
        out << stmt->getName();
    } else {
        out << "???";
    }
}


static const StringRef printKernelNameAnnotations(StringRef name, raw_ostream & out) {
    auto pos = name.find_first_of("+-");
    if (pos == StringRef::npos) {
        return name;
    }
    out << "#\n" << "# Pablo Debug Annotations:\n";
    auto kernelName = name.substr(0, pos);
    while (pos != StringRef::npos) {
        char a = name[pos];
        auto pos2 = name.find_first_of("+-", pos+1);
        auto annotation = name.substr(pos + 1, pos2);
        out << "# " << a << annotation << "\n";
        pos = pos2;
    }
    out << "#\n";
    return kernelName;
}


void PabloPrinter::print(PabloKernel const * kernel, raw_ostream & out) noexcept {
    auto kernelName = printKernelNameAnnotations(kernel->getName(), out);
    out << "kernel " << kernelName << " :: ";
    out << "[";
    for (size_t i = 0; i < kernel->getInputStreamSetBindings().size(); ++i) {
        if (i) out << ", ";
        auto const & streamBinding = kernel->getInputStreamSetBinding(i);
        uint32_t numStreams = streamBinding.getNumElements();
        uint32_t fw = streamBinding.getFieldWidth();
        out << "<i" << fw << ">[" << numStreams << "] ";
        out << streamBinding.getName();
    }
    for (auto const & scalarBinding : kernel->getInputScalarBindings()) {
        out << *scalarBinding.getType();
    }

    out << "] -> [";

    for (size_t i = 0; i < kernel->getOutputStreamSetBindings().size(); ++i) {
        if (i) out << ", ";
        auto const & streamBinding = kernel->getOutputStreamSetBinding(i);
        uint32_t numStreams = streamBinding.getNumElements();
        uint32_t fw = streamBinding.getFieldWidth();
        out << "<i" << fw << ">[" << numStreams << "] ";
        out << streamBinding.getName();
    }
    for (auto const & scalarBinding : kernel->getOutputScalarBindings()) {
        out << *scalarBinding.getType() << " " << scalarBinding.getName();
    }

    out << "] {\n";
    print(kernel->getEntryScope(), out, true, INDENT_WIDTH);
    out << "}\n\n";
}


void PabloPrinter::print(PabloAST const * node, raw_ostream & out) noexcept {
    PrintExpression(node, out);
}

void PabloPrinter::print(PabloBlock const * block, raw_ostream & out, const bool expandNested, unsigned indent) noexcept {
    for (auto const stmt : *block) {
        PrintStatement(stmt, out, expandNested, indent);
    }
}


void PabloPrinter::print(Statement const * stmt, raw_ostream & out, const bool expandNested, unsigned indent) noexcept {
    PrintStatement(stmt, out, expandNested, indent);
}

static uint32_t alias(const std::string operand1, const std::string &dest,
                      std::map<std::string, uint32_t> &map_variable) {
  uint32_t operand1_idx;
  if (map_variable.find(operand1) == map_variable.end()) {
    throw std::runtime_error("Error: operand1 variable not found");
  }
  operand1_idx = map_variable[operand1];
  map_variable[dest] = operand1_idx;
//   printf(" insert %s %d\n", dest.c_str(), operand1_idx);
  return operand1_idx;
}

// NOT, ASSIGN
static std::string getGPUInst(uint32_t n_inst, std::string operation,
                              std::string operand1, std::string dest,
                              std::map<std::string, uint32_t> &map_variable) {
  bool is_bs_basic1 = false;
  uint32_t operand1_idx, dest_idx;

  // operands
  if (operand1.find("UTF8_basis") != std::string::npos ||
      operand1.find("Byte_basis") != std::string::npos) {
    is_bs_basic1 = true;
    operand1_idx = std::stoi(operand1.substr(operand1.find('[') + 1));
  } else {
    if (map_variable.find(operand1) == map_variable.end()) {
      throw std::runtime_error("Error: operand1 variable not found: " +
                               operand1);
    }
    operand1_idx = map_variable[operand1];
  }

  // dest
  if (map_variable.find(dest) != map_variable.end()) {
    errs() << "Warning: dest variable already exists: " << dest << "\n";
    dest_idx = map_variable[dest];
  } else {
    dest_idx = map_variable.size();
    map_variable[dest] = dest_idx;
    // printf(" insert %s %d\n", dest.c_str(), dest_idx);
  }
  return std::format("ri[{0}].init(Re_Inst::{1}, {2}, {3}, {4});", n_inst,
                     operation, operand1_idx, dest_idx, is_bs_basic1);
}

static std::string getGPUInst(uint32_t n_inst, std::string operation,
                              std::string operand1, std::string operand2,
                              std::string dest,
                              std::map<std::string, uint32_t> &map_variable) {
  // cpp_code.append(f'ri[{instruction_index}].init(Re_Inst::ADVANCE,
  // {operand1_value}, {operand2}, {instruction_index}, {is_bs_basic1},
  // false);')

  if (operation == "ADVANCE") {
    if (map_variable.find(operand1) == map_variable.end()) {
      throw std::runtime_error("Error: operand1 variable not found: " +
                               operand1);
    }
    uint32_t operand1_idx = map_variable[operand1];
    uint32_t dest_idx = alias(operand1, dest, map_variable);
    return std::format("ri[{0}].init(Re_Inst::{1}, {2}, {3}, {4}, {5}, {6});",
                       n_inst, operation, operand1_idx, operand2, dest_idx,
                       false, false);
  }

  if (operation == "IF") {
    uint32_t operand1_idx;
    bool is_bs_basic1 = false;
    if (operand1.find("UTF8_basis") != std::string::npos ||
      operand1.find("Byte_basis") != std::string::npos) {
      is_bs_basic1 = true;
      operand1_idx = std::stoi(operand1.substr(operand1.find('[') + 1));
    } else {
      if (map_variable.find(operand1) == map_variable.end()) {
        throw std::runtime_error("Error: operand1 variable not found: " +
                                 operand1);
      }
      operand1_idx = map_variable[operand1];
    }
    return std::format("ri[{0}].init(Re_Inst::{1}, {2}, {3}, {4}, {5});",
                       n_inst, operation, operand1_idx, operand2, dest,
                       is_bs_basic1);
  }

  bool is_bs_basic1 = false, is_bs_basic2 = false;
  uint32_t operand1_idx, operand2_idx, dest_idx;

  // operands
  if (operand1.find("UTF8_basis") != std::string::npos ||
      operand1.find("Byte_basis") != std::string::npos) {
    is_bs_basic1 = true;
    operand1_idx = std::stoi(operand1.substr(operand1.find('[') + 1));
  } else {
    if (map_variable.find(operand1) == map_variable.end()) {
      throw std::runtime_error("Error: operand1 variable not found: " +
                               operand1);
    }
    operand1_idx = map_variable[operand1];
  }
  if (operand2.find("UTF8_basis") != std::string::npos ||
      operand2.find("Byte_basis") != std::string::npos) {
    is_bs_basic2 = true;
    operand2_idx = std::stoi(operand2.substr(operand2.find('[') + 1));
  } else {
    if (map_variable.find(operand2) == map_variable.end()) {
      throw std::runtime_error("Error: operand2 variable not found: " +
                               operand2);
    }
    operand2_idx = map_variable[operand2];
  }

  // dest
  if (map_variable.find(dest) != map_variable.end()) {
    errs() << "Warning: dest variable already exists: " << dest << "\n";
    dest_idx = map_variable[dest];
  } else {
    dest_idx = map_variable.size();
    map_variable[dest] = dest_idx;
    // printf(" insert %s %d\n", dest.c_str(), dest_idx);
  }

  return std::format("ri[{0}].init(Re_Inst::{1}, {2}, {3}, {4}, {5}, {6});",
                     n_inst, operation, operand1_idx, operand2_idx, dest_idx,
                     is_bs_basic1, is_bs_basic2);
}

static std::string getOperandName(const PabloAST *node) {
  std::string name;
  llvm::raw_string_ostream os(name);
  PrintExpression(node, os);
  os.flush();
  return name;
}

static void PrintExpressionGPU(PabloAST const * expr, raw_ostream & out) noexcept {
    assert (expr);
    if (isa<Zeroes>(expr)) {
        out << "<0>";
    } else if (const If * ifStmt = dyn_cast<If>(expr)) {
        out << "if ";
        PrintExpression(ifStmt->getCondition(), out);
        out << " {...}";
    } else if (const While * whileStmt = dyn_cast<While>(expr)) {
        out << "while ";
        PrintExpression(whileStmt->getCondition(), out);
        out << " {...}";
    } else if (isa<Ones>(expr)) {
        out << "<1>";
    } else if (const Integer * i = dyn_cast<Integer>(expr)) {
        out << i->value();
    } else if (Extract const * extract = dyn_cast<Extract>(expr)) {
        PrintExpression(extract->getArray(), out);
        out << "[";
        PrintExpression(extract->getIndex(), out);
        out << "]";
    } else if (Var const * var = dyn_cast<Var>(expr)) {
        out << var->getName();
    } else if (isa<Integer>(expr)) {
        out << cast<Integer>(expr)->value();
    } else if (const Add * op = dyn_cast<Add>(expr)) {
        PrintExpression(op->getLH(), out);
        out << " + ";
        PrintExpression(op->getRH(), out);
    } else if (const Subtract * op = dyn_cast<Subtract>(expr)) {
        PrintExpression(op->getLH(), out);
        out << " - ";
        PrintExpression(op->getRH(), out);
    } else if (const LessThan * op = dyn_cast<LessThan>(expr)) {
        PrintExpression(op->getLH(), out);
        out << " < ";
        PrintExpression(op->getRH(), out);
    } else if (const LessThanEquals * op = dyn_cast<LessThanEquals>(expr)) {
        PrintExpression(op->getLH(), out);
        out << " <= ";
        PrintExpression(op->getRH(), out);
    } else if (const Equals * op = dyn_cast<Equals>(expr)) {
        PrintExpression(op->getLH(), out);
        out << " == ";
        PrintExpression(op->getRH(), out);
    } else if (const GreaterThanEquals * op = dyn_cast<GreaterThanEquals>(expr)) {
        PrintExpression(op->getLH(), out);
        out << " >= ";
        PrintExpression(op->getRH(), out);
    } else if (const GreaterThan * op = dyn_cast<GreaterThan>(expr)) {
        PrintExpression(op->getLH(), out);
        out << " > ";
        PrintExpression(op->getRH(), out);
    } else if (const NotEquals * op = dyn_cast<NotEquals>(expr)) {
        PrintExpression(op->getLH(), out);
        out << " != ";
        PrintExpression(op->getRH(), out);
    } else if (const Statement * stmt = dyn_cast<Statement>(expr)) {
        out << stmt->getName();
    } else {
        out << "???";
    }
}


static void PrintStatementGPU(Statement const *stmt, raw_ostream &out,
                              uint32_t &n_inst,
                              std::map<std::string, uint32_t> &map_variable,
                              const bool expandNested,
                              unsigned indent) noexcept {
  out.indent(indent);
  if (stmt == nullptr) {
    out << "<null-stmt>";
  } else if (const Assign *assign = dyn_cast<Assign>(stmt)) {
    std::string operand1, dest;
    operand1 = getOperandName(assign->getValue());
    dest = getOperandName(assign->getVariable());
    out << getGPUInst(n_inst, "ASSIGN", operand1, dest, map_variable);
    out << "\n";
    n_inst++;
  } else if (const Branch *br = dyn_cast<Branch>(stmt)) {
    uint32_t pos_if = n_inst;
    n_inst++; // insert branch as empty inst
    if (isa<If>(br)) {
      ;
    } else if (isa<While>(br)) {
      out << "while ";
    }
    if (expandNested) {
      std::string nestedCode;
      llvm::raw_string_ostream rso(nestedCode);
      PabloPrinter::print_gpu(br->getBody(), rso, n_inst, map_variable,
                              expandNested, indent + INDENT_WIDTH);

      std::string condition, pos_true, pos_false;
      condition = getOperandName(br->getCondition());
      pos_true = std::to_string(pos_if + 1);
      pos_false = std::to_string(n_inst);
      out << getGPUInst(pos_if, "IF", condition, pos_true, pos_false,
                        map_variable);
      rso.flush();
      out << "\n" << nestedCode << "\n";
    } else {
      throw std::runtime_error("Branch not expandNested not supported in GPU");
    }
  } else {
    std::string dest;
    dest = getOperandName(cast<PabloAST>(stmt));

    if (const And *andNode = dyn_cast<And>(stmt)) {
      if (andNode->getNumOperands() != 2) {
        errs() << "Error: AND operation with more than 2 operands\n";
        exit(1);
      }
      std::string operand1, operand2;
      operand1 = getOperandName(andNode->getOperand(0));
      operand2 = getOperandName(andNode->getOperand(1));
      out << getGPUInst(n_inst, "AND", operand1, operand2, dest, map_variable);
    } else if (const Or *orNode = dyn_cast<Or>(stmt)) {
      if (orNode->getNumOperands() != 2) {
        errs() << "Error: AND operation with more than 2 operands\n";
        exit(1);
      }
      std::string operand1, operand2;
      operand1 = getOperandName(orNode->getOperand(0));
      operand2 = getOperandName(orNode->getOperand(1));
      out << getGPUInst(n_inst, "OR", operand1, operand2, dest, map_variable);
    } else if (const Xor *xorNode = dyn_cast<Xor>(stmt)) {
      if (xorNode->getNumOperands() != 2) {
        errs() << "Error: AND operation with more than 2 operands\n";
        exit(1);
      }
      std::string operand1, operand2;
      operand1 = getOperandName(xorNode->getOperand(0));
      operand2 = getOperandName(xorNode->getOperand(1));
      out << getGPUInst(n_inst, "XOR", operand1, operand2, dest, map_variable);
    } else if (const Sel *selNode = dyn_cast<Sel>(stmt)) {
      throw std::runtime_error("Sel operation not supported in GPU");
      out << "Sel(";
      PrintExpression(selNode->getCondition(), out);
      out << ", ";
      PrintExpression(selNode->getTrueExpr(), out);
      out << ", ";
      PrintExpression(selNode->getFalseExpr(), out);
      out << ")";
    } else if (const Not *notNode = dyn_cast<Not>(stmt)) {
      std::string operand1;
      operand1 = getOperandName(notNode->getExpr());
      out << getGPUInst(n_inst, "NOT", operand1, dest, map_variable);
    } else if (IntrinsicCall const *call = dyn_cast<IntrinsicCall>(stmt)) {
      out << call->getIntrinsicName() << "(";
      for (size_t i = 0; i < call->getNumOperands(); ++i) {
        if (i)
          out << ", ";
        PrintExpression(call->getOperand(i), out);
      }
      out << ")";
    } else if (const Advance *adv = dyn_cast<Advance>(stmt)) {

      std::string operand1, operand2;
      operand1 = getOperandName(adv->getExpression());
      operand2 = std::to_string(adv->getAmount());
      out << getGPUInst(n_inst, "ADVANCE", operand1, operand2, dest, map_variable);
    } else if (const IndexedAdvance *adv = dyn_cast<IndexedAdvance>(stmt)) {
      out << "IndexedAdvance(";
      PrintExpression(adv->getExpression(), out);
      out << ", ";
      PrintExpression(adv->getIndex(), out);
      out << ", " << std::to_string(adv->getAmount()) << ")";
    } else if (const Lookahead *adv = dyn_cast<Lookahead>(stmt)) {
      out << "Lookahead(";
      PrintExpression(adv->getExpression(), out);
      out << ", " << std::to_string(adv->getAmount()) << ")";
    } else if (const MatchStar *mstar = dyn_cast<MatchStar>(stmt)) {
      out << "MatchStar(";
      PrintExpression(mstar->getMarker(), out);
      out << ", ";
      PrintExpression(mstar->getCharClass(), out);
      out << ")";
    } else if (const ScanThru *sthru = dyn_cast<ScanThru>(stmt)) {
      out << "ScanThru(";
      PrintExpression(sthru->getScanFrom(), out);
      out << ", ";
      PrintExpression(sthru->getScanThru(), out);
      out << ")";
    } else if (const ScanTo *sto = dyn_cast<ScanTo>(stmt)) {
      out << "ScanTo(";
      PrintExpression(sto->getScanFrom(), out);
      out << ", ";
      PrintExpression(sto->getScanTo(), out);
      out << ")";
    } else if (const AdvanceThenScanThru *sthru =
                   dyn_cast<AdvanceThenScanThru>(stmt)) {
      out << "AdvanceThenScanThru(";
      PrintExpression(sthru->getScanFrom(), out);
      out << ", ";
      PrintExpression(sthru->getScanThru(), out);
      out << ")";
    } else if (const AdvanceThenScanTo *sto =
                   dyn_cast<AdvanceThenScanTo>(stmt)) {
      out << "AdvanceThenScanTo(";
      PrintExpression(sto->getScanFrom(), out);
      out << ", ";
      PrintExpression(sto->getScanTo(), out);
      out << ")";
    } else if (const Ternary *tern = dyn_cast<Ternary>(stmt)) {
      out << "Ternary(";
      out.write_hex(tern->getMask()->value());
      out << ", ";
      PrintExpression(tern->getA(), out);
      out << ", ";
      PrintExpression(tern->getB(), out);
      out << ", ";
      PrintExpression(tern->getC(), out);
      out << ")";
    } else if (const Count *count = dyn_cast<Count>(stmt)) {
      out << "Count(";
      PrintExpression(count->getExpr(), out);
      out << ")";
    } else if (const Repeat *splat = dyn_cast<Repeat>(stmt)) {
      out << "Repeat(";
      PrintExpression(splat->getFieldWidth(), out);
      out << ", ";
      auto fw = splat->getValue()->getType()->getIntegerBitWidth();
      out << "Int" << fw << "(";
      PrintExpression(splat->getValue(), out);
      out << "))";
    } else if (const PackH *p = dyn_cast<PackH>(stmt)) {
      out << " = PackH(";
      PrintExpression(p->getFieldWidth(), out);
      out << ", ";
      PrintExpression(p->getValue(), out);
      out << ")";
    } else if (const PackL *p = dyn_cast<PackL>(stmt)) {
      out << " = PackL(";
      PrintExpression(p->getFieldWidth(), out);
      out << ", ";
      PrintExpression(p->getValue(), out);
      out << ")";
    } else if (const DebugPrint *e = dyn_cast<DebugPrint>(stmt)) {
      out << "DebugPrint(";
      PrintExpression(e->getExpr(), out);
      out << ")";
    } else if (const InFile *e = dyn_cast<InFile>(stmt)) {
      alias(getOperandName(e->getExpr()), getOperandName(stmt), map_variable);
      n_inst--;
    } else if (const AtEOF *e = dyn_cast<AtEOF>(stmt)) {
      out << "AtEOF(";
      PrintExpression(e->getExpr(), out);
      out << ")";
    } else if (const TerminateAt *s = dyn_cast<TerminateAt>(stmt)) {
      out << "TerminateAt(";
      PrintExpression(s->getExpr(), out);
      out << ", " << std::to_string(s->getSignalCode()) << ")";
    } else if (const EveryNth *e = dyn_cast<EveryNth>(stmt)) {
      out << "EveryNth(";
      out << e->getN()->value();
      out << ", ";
      PrintExpression(e->getExpr(), out);
      out << ")";
    } else {
      out << "???";
    }

    out << "\n";
    n_inst++;
  }
}

void PabloPrinter::print_gpu(PabloBlock const *block, raw_ostream &out,
                             uint32_t &n_inst,
                             std::map<std::string, uint32_t> &map_variable,
                             const bool expandNested,
                             unsigned indent) noexcept {
  for (auto const stmt : *block) {
    // PrintStatement(stmt, out, expandNested, indent);
    PrintStatementGPU(stmt, out, n_inst, map_variable, expandNested, indent);
  }
}

void PabloPrinter::print_gpu(PabloKernel const *kernel,
                             raw_ostream &out) noexcept {
  uint32_t n_inst = 0;
  std::map<std::string, uint32_t> map_variable;
  map_variable["<0>"] = 0U;
  map_variable["<1>"] = 1U;
  map_variable["mBarrier[0]"] = 2U;

  print_gpu(kernel->getEntryScope(), out, n_inst, map_variable, true,
            INDENT_WIDTH);
}

} // namespace pablo
