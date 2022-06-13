#ifndef CHOCOPY_COMPILER_CHOCOPY_CGEN_HPP
#define CHOCOPY_COMPILER_CHOCOPY_CGEN_HPP

#ifdef LLVM
#include "llvm/ADT/StringRef.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#endif
#include "BasicBlock.hpp"
#include "Constant.hpp"
#include "Function.hpp"
#include "GlobalVariable.hpp"
#include "IRBuilder.hpp"
#include "Module.hpp"
#include "RiscVBackEnd.hpp"
#include "Type.hpp"
#include "User.hpp"
#include "Value.hpp"
#include <algorithm>
#include <iostream>
#include <queue>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

using namespace lightir;
using std::string;
using std::string_view;

#ifdef LLVM
const Module chocopy_m;
#endif
namespace cgen {
class InstGen;
class RiscVBackEnd;

const int op_reg_0 = 5;
const int op_reg_1 = 6;
const int op_reg_2 = 7;

const string attribute = "rv32i2p0_m2p0_a2p0_c2p0";
#ifdef LLVM
class CodeGen : llvm::PassInfoMixin<CodeGen> {
#else
class CodeGen {
#endif
private:
    shared_ptr<Module> module;
    map<std::string, int> register_mapping;
    map<std::string, int> stack_mapping;
    map<std::string, int> alloca_mapping;
    map<std::string, int> GOT;
    map<BasicBlock *, std::vector<std::pair<Value*, int>>> phi_store;
    map<Instruction *, set<Value *>> context_active_vars;
    int stack_size;
    int spill_cost_total;
    int color_bonus_total;
    bool debug;
    RiscVBackEnd *backend;
    BasicBlock* current_basic_block;

public:
    explicit CodeGen(shared_ptr<Module> module);
#ifdef LLVM
    explicit CodeGen() : CodeGen(chocopy_m){};
#endif
    string generateModuleCode();
    string generateFunctionCode(Function *func);
    string generateFunctionEntryCode(Function *func);
    string generateFunctionExitCode();
    string generateFunctionPostCode(Function *func);
    string generateHeaderCode();
    string generateFooterCode();
    string generateClassCode();
    string generateBasicBlockCode(BasicBlock *bb);
    string generateBasicBlockPostCode(BasicBlock *bb);
    string generateInstructionCode(Instruction *inst);
    string getLabelName(BasicBlock *bb);
    string getLabelName(Function *func, int type);
    string generateFunctionCall(Instruction *inst, const string &call_inst, vector<Value *> ops, int return_reg = 0,
                                int sp_ofs = 0);
    void allocateStackSpace(Function *func);
    [[nodiscard]] string stackToReg(int offset, int reg);
    [[nodiscard]] string stackToReg(string name, int reg);
    [[nodiscard]] string valueToReg(Value* v, int reg);
    [[nodiscard]] string regToStack(int reg, int offset);
    [[nodiscard]] string regToStack(int reg, string name);
    string virtualRegMove(vector<Value *> target, vector<Value *> source, int sp_ofs = 0);
    string generateVext(int vlen, int elen, lightir::VExtInst::vv_type type, const InstGen::Reg &len);
    string generateGOT();
    string generateGlobalVarsCode();
    string generateInitializerCode(Constant *init);
    pair<int, bool> getConstIntVal(Value *val);
    string comment(const string &s);
    string comment(const string &t, const string &s);
    map<Value *, int> regAlloc();
    InstGen::Addr makeConstStr(const string &str);
    string replaceStringLiteral(const string &str);
    string vv(const InstGen::Reg &len);
#ifdef LLVM
    llvm::PreservedAnalyses run(llvm::Module &M, llvm::ModuleAnalysisManager &) {
        generateModuleCode(true);
        return llvm::PreservedAnalyses::all();
    }

    static llvm::StringRef name() { return "ChocoPy"; }
#endif
};

} // namespace cgen

#endif