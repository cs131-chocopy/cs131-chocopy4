#ifndef CHOCOPY_COMPILER_CHOCOPY_CGEN_HPP
#define CHOCOPY_COMPILER_CHOCOPY_CGEN_HPP

#include <vector>
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

class Interval {
public:
    std::set<std::pair<int, int>> ranges;
    void addRange(int l, int r);
    void setCreatePosition(int pos);
};

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
    
    std::map<Value*, int> inst_id;
    map<BasicBlock*, int> basic_block_from;
    map<BasicBlock*, int> basic_block_to;
    map<BasicBlock*, std::set<std::string>> live_in;
    map<std::string, Interval> intervals;
    
    std::map<std::string, InstGen::Reg> vreg_to_reg;
    std::map<std::string, InstGen::Addr> vreg_to_stack_slot;
    std::map<InstGen::Reg, std::string> reg_to_vreg;

    map<std::string, int> register_mapping;
    map<std::string, int> stack_mapping;
    map<std::string, int> alloca_mapping;
    map<BasicBlock *, std::vector<std::pair<Value*, int>>> phi_store;
    int stack_size;

    map<std::string, int> GOT;
    bool debug;
    RiscVBackEnd *backend;
    BasicBlock* current_basic_block;
    Function* current_function;

public:
    explicit CodeGen(shared_ptr<Module> module);
#ifdef LLVM
    explicit CodeGen() : CodeGen(chocopy_m){};
#endif
    [[nodiscard]] string generateModuleCode();

    void lifetimeAnalysis();
    void linearScan();

    [[nodiscard]] string generateFunctionCode(Function *func);
    [[nodiscard]] string generateFunctionExitCode();

    [[nodiscard]] string generateBasicBlockCode(BasicBlock *bb);
    [[nodiscard]] string generateBasicBlockPostCode(BasicBlock *bb);

    [[nodiscard]] string generateInstructionCode(Instruction *inst);
    [[nodiscard]] string generateFunctionCall(Instruction *inst, const string &call_inst, vector<Value *> ops);

    [[nodiscard]] string getLabelName(BasicBlock *bb);
    [[nodiscard]] string getLabelName(Function *func, int type);

    [[nodiscard]] string generateGlobalVarsCode();
    [[nodiscard]] string generateInitializerCode(Constant *init);
    [[nodiscard]] pair<int, bool> getConstIntVal(Value *val);    

    [[nodiscard]] string stackToReg(int offset, int reg);
    [[nodiscard]] string stackToReg(string name, int reg);
    [[nodiscard]] string valueToReg(Value* v, int reg);
    [[nodiscard]] string regToStack(int reg, int offset);
    [[nodiscard]] string regToStack(int reg, string name);

    string comment(const string &s);
    string comment(const string &t, const string &s);
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
