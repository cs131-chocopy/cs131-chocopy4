#include "BasicBlock.hpp"
#include "Constant.hpp"
#include "GlobalVariable.hpp"
#include "InstGen.hpp"
#include "Module.hpp"
#include "Type.hpp"
#include <cassert>
#include <chocopy_cgen.hpp>
#include <chocopy_lightir.hpp>
#include <fmt/core.h>
#if __cplusplus > 202000L && !defined(__clang__)
#include <ranges>
#endif
#include <regex>
#include <utility>

/** From String to codegen, care about class */
#ifdef LLVM
using namespace llvm;
extern Module chocopy_m;
#else
using namespace lightir;
#endif

namespace cgen {
int getTypeSizeInBytes(Type *type) {
    if (dynamic_cast<ArrayType*>(type)) {
        return 4;
    } else if (type->is_integer_type() || type->is_bool_type()) {
        return 4; // bool and int are 4 bytes
    } else if (auto class_ = dynamic_cast<Class*>(type); class_ && class_->anon_) {
        int ret = 0;
        for (auto attr : *class_->get_attribute()) {
            ret += getTypeSizeInBytes(attr->get_type());
        }
        return ret;
    } else if (type->is_union_type()) {
        // %$union.type = type { i32 }
        // %$union.len = type { i32 }
        // %$union.put = type { i32 }
        // %$union.conslist = type { i32 }
        return 4;
    }
    assert(0);
}

string InstGen::Addr::get_name() const {
    if (str.empty())
        return fmt::format("[{}, #{}]", reg.get_name(), std::to_string(this->offset));
    else
        return str;
}

string InstGen::set_value(const Reg &target, const Constant &source) {
    string asm_code;
    /** switch different instruction to load a word constant */
    return asm_code;
};
string InstGen::instConst(string (*inst)(const Reg &, const Reg &, const Value &, string), const InstGen::Reg &target,
                          const InstGen::Reg &op1, const InstGen::Constant &op2) {
    string asm_code;
    int val = op2.getValue();
    if (target == op1 && op2.getValue() == 0 && (inst == RiscVBackEnd::emit_add || inst == RiscVBackEnd::emit_sub)) {
        return asm_code;
    } else if (0 <= val && val <= imm_8_max) {
        asm_code += inst(target, op1, op2, "");
    } else {
        asm_code += set_value(InstGen::Reg(5), op2);
        asm_code += inst(target, op1, InstGen::Reg(5), "");
    }
    return asm_code;
}
string InstGen::instConst(string (*inst)(const Reg &, const Reg &, const Reg &, string), const InstGen::Reg &target,
                          const InstGen::Reg &op1, const InstGen::Constant &op2) {
    string asm_code;
    int val = op2.getValue();
    if (target == op1 && op2.getValue() == 0 &&
        (inst == RiscVBackEnd::emit_sll || inst == RiscVBackEnd::emit_srl || inst == RiscVBackEnd::emit_or)) {
        return asm_code;
    } else {
        asm_code += set_value(InstGen::Reg(5), op2);
        asm_code += inst(target, op1, InstGen::Reg(5), "");
    }
    return asm_code;
}

string InstGen::instConst(string (*inst)(const Reg &, const Reg &, int, string), const InstGen::Reg &target,
                          const InstGen::Reg &op1, const InstGen::Constant &op2) {
    string asm_code;
    int val = op2.getValue();
    if (target == op1 && op2.getValue() == 0 &&
        (inst == RiscVBackEnd::emit_slli || inst == RiscVBackEnd::emit_srli || inst == RiscVBackEnd::emit_ori)) {
        return asm_code;
    } else { // imm
        asm_code += inst(target, op1, val, "");
    }
    return asm_code;
}

string InstGen::instConst(string (*inst)(const Reg &, const Value &, string), const InstGen::Reg &op1,
                          const InstGen::Constant &op2) {
    string asm_code;
    int val = op2.getValue();
    if (0 <= val && val <= imm_8_max) {
        asm_code += inst(op1, op2, "");
    } else {
        asm_code += set_value(InstGen::Reg(5), op2);
        asm_code += inst(op1, InstGen::Reg(5), "");
    }
    return asm_code;
}

string CodeGen::stackToReg(int offset, int reg) {
    return fmt::format("  lw {}, {}(fp)\n", reg_name[reg], offset);
}
string CodeGen::stackToReg(string name, int reg) {
    return stackToReg(stack_mapping.at(name), reg);
}
string CodeGen::regToStack(int reg, int offset) {
    return fmt::format("  sw {}, {}(fp)\n", reg_name[reg], offset);
}
string CodeGen::regToStack(int reg, string name) {
    return regToStack(reg, stack_mapping.at(name));
}
string CodeGen::valueToReg(Value *v, int reg) {
    if (dynamic_cast<ConstantNull*>(v)) {
        return fmt::format("  li {}, 0\n", reg_name[reg]);
    } else if (auto c = dynamic_cast<ConstantInt*>(v); c) {
        return fmt::format("  li {}, {}\n", reg_name[reg], c->get_value());
    } else if (auto a = dynamic_cast<AllocaInst*>(v)) {
        return fmt::format("  addi {}, fp, {}\n", reg_name[reg], alloca_mapping[a->get_name()]);
    } else if (auto cls = dynamic_cast<Class*>(v); cls) {
        return fmt::format("  la {}, {}\n", reg_name[reg], cls->prototype_label_);
    } else if (auto name = v->get_name(); GOT.contains(name)) {
        return fmt::format("  la {}, {}\n", reg_name[reg], name);
    } else if (register_mapping.contains(v->get_name())) {
        return fmt::format("  addi {}, {}, 0\n", reg_name[reg], reg_name[register_mapping.at(v->get_name())]);
    }
    return stackToReg(v->get_name(), reg);
}

string CodeGen::generateModuleCode() {
    string asm_code;

    /** Symbolic assembler constants defined here (to add others, override
     * initAsmConstants in an extension of CodeGenBase):
     * ecalls:
     *   @sbrk
     *   @fill_line_buffer
     *   @read_string
     *   @print_string
     *   @print_char
     *   @print_int
     *   @exit2
     * Exit codes:
     *   @error_div_zero: Division by 0.
     *   @error_arg: Bad argument.
     *   @error_oob: Out of bounds.
     *   @error_none: Attempt to access attribute of None.
     *   @error_oom: Out of memory.
     *   @error_nyi: Unimplemented operation.
     * Data-structure byte offsets:
     *   @.__obj_size__: Offset of size of object.
     *   @.__len__: Offset of length in chars or words.
     *   @.__str__: Offset of string data.
     *   @.__elts__: Offset of first list item.
     *   @.__int__: Offset of integer value.
     *   @.__bool__: Offset of boolean (1/0) value.
     */

    /** Define @-constants to be used in assembly code. */
#ifdef EMIT
    asm_code += backend->defineSym("sbrk", backend->SBRK_ECALL);
    asm_code += backend->defineSym("print_string", backend->PRINT_STRING_ECALL);
    asm_code += backend->defineSym("print_char", backend->PRINT_CHAR_ECALL);
    asm_code += backend->defineSym("print_int", backend->PRINT_INT_ECALL);
    asm_code += backend->defineSym("exit2", backend->EXIT2_ECALL);
    asm_code += backend->defineSym("read_string", backend->READ_STRING_ECALL);
    asm_code += backend->defineSym("fill_line_buffer", backend->FILL_LINE_BUFFER__ECALL);

    asm_code += backend->defineSym(".__obj_size__", 4);
    asm_code += backend->defineSym(".__len__", 12);
    asm_code += backend->defineSym(".__int__", 12);
    asm_code += backend->defineSym(".__bool__", 12);
    asm_code += backend->defineSym(".__str__", 16);
    asm_code += backend->defineSym(".__elts__", 16);

    asm_code += backend->defineSym("error_div_zero", backend->ERROR_DIV_ZERO);
    asm_code += backend->defineSym("error_arg", backend->ERROR_ARG);
    asm_code += backend->defineSym("error_oob", backend->ERROR_OOB);
    asm_code += backend->defineSym("error_none", backend->ERROR_NONE);
    asm_code += backend->defineSym("error_oom", backend->ERROR_OOM);
    asm_code += backend->defineSym("error_nyi", backend->ERROR_NYI);
#endif

    // FIXME: I don't know what the code below does.
    // asm_code = replaceStringLiteral(asm_code);

    asm_code += generateHeaderCode();

    asm_code += ".data\n";
    for (auto &classInfo : this->module->get_class()) {
        asm_code += backend->emit_prototype(*classInfo);
    }
    asm_code += CodeGen::generateGOT();
    asm_code += generateGlobalVarsCode();

    asm_code += ".text\n";
    for (auto func : this->module->get_functions()) {
        assert(dynamic_cast<Function*>(func));
        if (func->get_basic_blocks().size()) {
            asm_code += CodeGen::generateFunctionCode(func);
        }
    }

    // FIXME: I don't know what the code below does.
    // backend->emit_prototype 已经生成 $object$prototype, $object$dispatchTable
    // 不知道为什么这里要重新生成
    // asm_code += backend->start_code();
    // asm_code += generateClassCode();

    asm_code += generateFooterCode();

    return asm_code;
}

std::map<Value *, int> CodeGen::regAlloc() {
    // TODO: regAlloc algorithm
}

string CodeGen::generateFunctionCode(Function *func) {
    // std::cerr << "Generating code for function " << func->get_name() << std::endl;
    // 注意这里没有保存 a0, a1, a2, a3, a4, a5, a6, a7
    // 因为函数生成 LLVM IR 时会给每个参数 %opx = alloca i32; store %arg, ptr %opx
    // 所以他们只会在函数最初 alloca-store 时被使用
    string asm_code;
    asm_code += fmt::format(".globl {}\n{}:\n", func->get_name(), func->get_name());

    stack_size = 0;
    for(auto b : func->get_basic_blocks()) {
        for (auto i : b->get_instructions()) {
            if (auto alloca = dynamic_cast<AllocaInst*>(i); alloca) {
                stack_size += getTypeSizeInBytes(alloca->get_alloca_type());
            } else {
                stack_size += 4;
            }
        }
    }
    stack_size += 2 * 4; // %sp %fp
    stack_size = (stack_size + 15) & ~15;
    // std::cerr << "Stack size: " << stack_size << std::endl;

    register_mapping.clear();
    stack_mapping.clear();
    alloca_mapping.clear();
    int offset = -stack_size-4;
    for(auto b : func->get_basic_blocks()) {
        for (auto i : b->get_instructions()) {
            if (auto alloca = dynamic_cast<AllocaInst*>(i); alloca) {
                offset += getTypeSizeInBytes(alloca->get_alloca_type());
                alloca_mapping[alloca->get_name()] = offset;
            } else {
                offset += 4;
                stack_mapping[i->get_name()] = offset;
            }

            // create lw for phi nodes at the end of basic block
            if (auto phi = dynamic_cast<PhiInst*>(i); phi) {
                auto ops = phi->get_operands();
                assert(phi->get_operands().size() == 4);
                assert(dynamic_cast<BasicBlock*>(ops[1]) && dynamic_cast<BasicBlock*>(ops[3]));
                auto v1 = ops[0];
                auto b1 = (BasicBlock*)ops[1];
                auto v2 = ops[2];
                auto b2 = (BasicBlock*)ops[3];
                int offset = stack_mapping[phi->get_name()];
                phi_store[b1].push_back({v1, offset});
                phi_store[b2].push_back({v2, offset});
            }
        }
    }
    int args = func->get_num_of_args();
    for (int i = 0; i < std::min(args, 8); i++) {
        register_mapping[fmt::format("arg{}", i)] = i + 10;
    }
    if (args > 8) {
        for(int i = 8; i < args; i++) {
            stack_mapping[fmt::format("arg{}", i)] = (i-8) * 4;
        }
    }

    asm_code += fmt::format("  addi sp, sp, {}\n", -stack_size);
    asm_code += fmt::format("  sw ra, {}(sp)\n", stack_size - 4);
    asm_code += fmt::format("  sw fp, {}(sp)\n", stack_size - 8);
    asm_code += fmt::format("  addi fp, sp, {}\n", stack_size);

    for(auto b : func->get_basic_blocks()) {
        asm_code += fmt::format("{}:\n", getLabelName(b), b->get_name());
        asm_code += generateBasicBlockCode(b);
    }
    return asm_code;
}

void CodeGen::allocateStackSpace(Function *func) {
    // maybe deprecated
}

CodeGen::CodeGen(shared_ptr<Module> module) : module(move(module)), backend(new RiscVBackEnd()) {}

string CodeGen::generateFunctionEntryCode(Function *func) {
    string asm_code;
    asm_code += CodeGen::getLabelName(func, 0) + ":";
    asm_code += CodeGen::comment("function preprocess");
    // maybe deprecated
    return asm_code;
}
[[nodiscard]]  string CodeGen::generateFunctionExitCode() {
    std::string asm_code;
    asm_code += fmt::format("  lw ra, {}(sp)\n", stack_size - 4);
    asm_code += fmt::format("  lw fp, {}(sp)\n", stack_size - 8);
    asm_code += fmt::format("  addi sp, sp, {}\n", stack_size);
    return asm_code;
}

string CodeGen::generateClassCode() {
    string class_asm;
    // maybe deprecated
    return class_asm;
}

string CodeGen::generateHeaderCode() {
    string asm_code;
    // TODO: generate header code
    return asm_code;
}
string CodeGen::generateFooterCode() {
    string asm_code;
    // TODO: generate footer code
    return asm_code;
}
string CodeGen::generateFunctionPostCode(Function *func) {
    std::string asm_code;
    auto tmp_asm_code = CodeGen::getLabelName(func, 1) + ":";
    asm_code += CodeGen::comment(tmp_asm_code, "function postcode");
    return asm_code;
}
string CodeGen::generateBasicBlockCode(BasicBlock *bb) {
    std::string asm_code;
    current_basic_block = bb;
    for (auto &inst : bb->get_instructions()) {
        asm_code += CodeGen::generateInstructionCode(inst);
    }
    asm_code += generateBasicBlockPostCode(bb);
    return asm_code;
}
string CodeGen::generateBasicBlockPostCode(BasicBlock *bb) {
    std::string asm_code;
    if (phi_store.contains(bb)) {
        for(auto& kv : phi_store.at(bb)) {
            auto v = kv.first;
            auto offset = kv.second;
            asm_code += valueToReg(v, 5);
            asm_code += regToStack(5, offset);
        }
    }
    return asm_code;
}
string CodeGen::generateInstructionCode(Instruction *inst) {
    std::string asm_code;
    auto &ops = inst->get_operands();
    // TODO: generate instruction code
    switch (inst->get_instr_type()) {
        case lightir::Instruction::Ret: {
            assert(ops.size() == 0 || ops.size() == 1);
            if (ops.size() == 1) {
                asm_code += valueToReg(ops[0], 10);
            }
            asm_code += generateFunctionExitCode();
            asm_code += "  ret\n";
            break;
        }
        case lightir::Instruction::Br: {
            asm_code += generateBasicBlockPostCode(current_basic_block);
            if (ops.size() == 1) {
                // unconditional branch
                assert(dynamic_cast<BasicBlock*>(ops[0]));
                asm_code += fmt::format("  j {}\n", getLabelName((BasicBlock*)ops[0]));
            } else if (ops.size() == 3) {
                asm_code += valueToReg(ops[0], 5);
                asm_code += fmt::format("  beq t0, zero, {}\n", getLabelName((BasicBlock*)ops[2]));
                asm_code += fmt::format("  j {}\n", getLabelName((BasicBlock*)ops[1]));
            } else {
                assert(0);
            }
            break;
        }
        case lightir::Instruction::Neg: {
            break;
        }
        case lightir::Instruction::Not: {
            break;
        }
        case lightir::Instruction::Add: {
            break;
        }
        case lightir::Instruction::Sub: {
            break;
        }
        case lightir::Instruction::Mul: {
            break;
        }
        case lightir::Instruction::Div: {
            break;
        }
        case lightir::Instruction::Rem: {
            break;
        }
        case lightir::Instruction::And: {
            break;
        }
        case lightir::Instruction::Or: {
            break;
        }
        case lightir::Instruction::Alloca: {
            break;
        }
        case lightir::Instruction::Load: {
            string op = ops[0]->get_name();
            if (alloca_mapping.contains(op)) {
                asm_code += fmt::format("  lw t1, {}(fp)\n", alloca_mapping.at(op));
            } else if (GOT.contains(op)) {
                asm_code += fmt::format("  lui t1, %hi({})\n  lw t1, %lo({})(t1)\n", op, op);
            } else {
                asm_code += valueToReg(ops[0], 5);
                asm_code += fmt::format("  lw t1, 0(t0)\n");
            }
            asm_code += regToStack(6, inst->get_name());
            break;
        }
        case lightir::Instruction::Store: {
            asm_code += valueToReg(ops[0],  5);
            asm_code += valueToReg(ops[1], 6);
            asm_code += fmt::format("  sw t0, 0(t1)\n");
            break;
        }
        case lightir::Instruction::Shl: {
            break;
        }
        case lightir::Instruction::AShr: {
            break;
        }
        case lightir::Instruction::LShr:{
            break;
        }
        case lightir::Instruction::ICmp: {
            break;
        }
        case lightir::Instruction::PHI: {
            break;
        }
        case lightir::Instruction::Call: {
            auto func_name = inst->get_operands()[0]->get_name();
            asm_code += generateFunctionCall(inst, func_name, inst->get_operands(), 10);
            asm_code += regToStack(10, inst->get_name());
            break;
        }
        case lightir::Instruction::GEP: {
            auto gep = (GetElementPtrInst*)inst;
            auto ptr = ops[0];
            assert(dynamic_cast<ArrayType*>(ptr->get_type())  && ((ArrayType*)ptr->get_type())->get_num_of_elements() == -1);
            auto inner_type = ((ArrayType*)ptr->get_type())->get_element_type();
            assert(dynamic_cast<ConstantInt*>(ops[1]));
            auto idx = ((ConstantInt*)ops[1])->get_value();
            if (dynamic_cast<Class*>(inner_type)) {
                // it seems that every attribute is 4 bytes
                asm_code += valueToReg(ptr, 5);
                asm_code += fmt::format("  addi t0, t0, {}\n", idx * 4);
                asm_code += regToStack(5, gep->get_name());
            } else {
                assert(0 && "not implemented");
            }
            break;
        }
        case lightir::Instruction::ZExt: {
            break;
        }
        case lightir::Instruction::InElem: {
            break;
        }
        case lightir::Instruction::ExElem: {
            break;
        }
        case lightir::Instruction::BitCast: {
            asm_code += valueToReg(ops[0], 5);
            asm_code += regToStack(5, inst->get_name());
            break;
        }
        case lightir::Instruction::Trunc: {
            break;
        }
        case lightir::Instruction::VExt: {
            break;
        }
        case lightir::Instruction::ASM: {
            string asm_ = ((AsmInst*)inst)->get_asm();
            asm_ = std::regex_replace(asm_, std::regex("\\\\0A"), "\n  ");
            asm_code += "  " + asm_ + "\n";
            break;
        }
        case lightir::Instruction::ACCSTART:
        case lightir::Instruction::ACCEND:;
    }
    return asm_code;
}
string CodeGen::getLabelName(BasicBlock *bb) { return "." + bb->get_parent()->get_name() + "_" + bb->get_name(); }
string CodeGen::getLabelName(Function *func, int type) {
    const std::vector<std::string> name_list = {"pre", "post"};
    return "." + func->get_name() + "_" + name_list.at(type);
}
string CodeGen::generateFunctionCall(Instruction *inst, const string &func_name, vector<Value *> ops, int return_reg,
                                     int sp_ofs) {
    // ops[0] is the function
    std::string asm_code;
    int args = ops.size() - 1;
    int sp_delta = 0;

    for (int i = 0; i < std::min(8, args); i++) {
        asm_code += valueToReg(ops[i+1], 10 + i);
    }
    if (args > 8) {
        args -= 8;
        sp_delta = args * 4;
        asm_code += fmt::format("  addi sp, sp, {}\n", -sp_delta);
        for (int i = 0; i < args; i++) {
            auto arg = ops[i+9];
            asm_code += valueToReg(arg, 5);
            asm_code += fmt::format("  sw t0, {}(sp)\n", i * 4);
        }
    }
    
    asm_code += fmt::format("  call {}\n", func_name);
    
    if (sp_delta != 0) {
        asm_code += fmt::format("  addi sp, sp, {}\n", +sp_delta);
    }
    if (return_reg != 10) {
        asm_code += fmt::format("   addi {}, a0, 0\n", reg_name[return_reg]);
    }
    return asm_code;
}
string CodeGen::generateGOT() {
    string asm_code;
    this->GOT.clear();
    for (auto &global_var : this->module->get_global_variable()) {
        if (global_var->init_val_ != nullptr) {
            int count = this->GOT.size();
            if (!GOT.count(global_var->get_name())) {
                this->GOT[global_var->get_name()] = count;
                asm_code += fmt::format(".globl {}\n", global_var->get_name());
            }
        }
    }
    LOG(INFO) << asm_code;
    return asm_code;
}
string CodeGen::generateGlobalVarsCode() {
    string asm_code;
    for (auto &global_var : this->module->get_global_variable()) {
        if (global_var->init_val_ != nullptr) {
            asm_code += global_var->get_name() + ":\n";
            if (reinterpret_cast<Type *>(!global_var->get_type()->get_ptr_element_type()) ==
                global_var->get_operands().at(0)->get_type()) {
                asm_code += "  .zero " + std::to_string(global_var->get_type()->get_size()) + "\n";
            } else {
                asm_code +=
                    CodeGen::generateInitializerCode(dynamic_cast<Constant *>(global_var->get_operands().at(0)));
            }
        }
    }
    LOG(INFO) << asm_code;
    return asm_code;
}
string CodeGen::generateInitializerCode(Constant *init) {
    string asm_code;
    auto array_init = dynamic_cast<ConstantArray *>(init);
    auto str_init = dynamic_cast<ConstantStr *>(init);
    if (array_init) {
        auto length = dynamic_cast<ArrayType *>(array_init->get_type())->get_num_of_elements();
        for (int i = 0; i < length; i++) {
            asm_code += CodeGen::generateInitializerCode(array_init->get_element_value(i));
        }
    } else if (str_init) {
        //        CodeGen::generate
        string str = str_init->get_value();
        asm_code += "  .word " + std::to_string(str_init->get_type()->get_type_id()) + "\n";
        size_t objsize = str.size() / 4 + 5;
        asm_code += "  .word " + std::to_string(objsize) += "\n";
        asm_code += "  .word $str$dispatchTable\n";
        asm_code += "  .word " + std::to_string(str.size()) += "\n";
        asm_code += "  .string \"" + str += "\"\n";
        asm_code += "  .align 2\n";
        asm_code += "\n";

    } else {
        auto val = CodeGen::getConstIntVal(init);
        if (!val.second) {
            LOG(ERROR) << "function generateInitializerCode exception!";
            exit(EXIT_FAILURE);
        }
        asm_code += "  .long " + std::to_string(val.first) + "\n";
    }
    return asm_code;
}
pair<int, bool> CodeGen::getConstIntVal(Value *val) {
    auto const_val = dynamic_cast<ConstantInt *>(val);
    auto inst_val = dynamic_cast<Instruction *>(val);
    if (const_val) {
        return std::make_pair(const_val->get_value(), true);
    } else if (inst_val) {
        auto op_list = inst_val->get_operands();
        if (dynamic_cast<BinaryInst *>(val)) {
            auto val_0 = CodeGen::getConstIntVal(op_list.at(0));
            auto val_1 = CodeGen::getConstIntVal(op_list.at(1));
            if (val_0.second && val_1.second) {
                int ret = 0;
                bool flag = true;
                switch (inst_val->get_instr_type()) {
                case Instruction::Add:
                    ret = val_0.first + val_1.first;
                    break;
                case Instruction::Sub:
                    ret = val_0.first - val_1.first;
                    break;
                case Instruction::And:
                    ret = val_0.first & val_1.first;
                    break;
                case Instruction::Or:
                    ret = val_0.first | val_1.first;
                    break;
                case Instruction::Mul:
                    ret = val_0.first * val_1.first;
                    break;
                case Instruction::Div:
                    ret = val_0.first / val_1.first;
                    break;
                case Instruction::Rem:
                    ret = val_0.first % val_1.first;
                    break;
                default:
                    flag = false;
                    break;
                }
                return std::make_pair(ret, flag);
            } else {
                return std::make_pair(0, false);
            }
        }
        if (dynamic_cast<UnaryInst *>(val)) {
            auto val_0 = CodeGen::getConstIntVal(op_list.at(0));
            if (val_0.second) {
                int ret = 0;
                bool flag = true;
                switch (inst_val->get_instr_type()) {
                case Instruction::Not:
                    ret = !val_0.first;
                    break;
                case Instruction::Neg:
                    ret = -val_0.first;
                    break;
                default:
                    flag = false;
                    break;
                }
                return std::make_pair(ret, flag);
            } else {
                return std::make_pair(0, false);
            }
        }
    } else if (dynamic_cast<ConstantNull*>(val)) {
        return std::make_pair(0, true);
    }
    LOG(ERROR) << "Function getConstIntVal exception!";
    exit(EXIT_FAILURE);
}
string CodeGen::comment(const string &s) {
    std::string asm_code;
    asm_code += fmt::format("# {}\n", s);
    return asm_code;
}
string CodeGen::comment(const string &t, const string &s) {
    std::string asm_code;
    asm_code += fmt::format("{:<42}{:<42}\n", t, fmt::format("# {}", s));
    return asm_code;
}
string CodeGen::generateVext(int vlen, int elen, lightir::VExtInst::vv_type type, const InstGen::Reg &len) {
    static int counter;
    std::string asm_code;
    // TODO: generate VextInst
    switch (type) {
    case VExtInst::vv_type::VADD:
    case VExtInst::VLE:
    case VExtInst::VSETVLI:
    case VExtInst::VDIV:
    case VExtInst::VREM:
    case VExtInst::VMUL:
    case VExtInst::VSE:;
    }
    return asm_code;
};

RiscVBackEnd::RiscVBackEnd(int vlen, int vlmax) : vlen(vlen), vlmax(64) {
    /** emplace_back vector reg */
    for (int i = 0; i < vlen / word_size / 4; i++)
        reg_name.emplace_back(fmt::format("x{}", i));
}

string cgen::CodeGen::replaceStringLiteral(const string &str) {
    const string find_left("STRING[");
    const string find_right("]");
    string result(str);
    size_t find_len;
    size_t left_pos, right_pos, from = 0;
    while (string::npos != (left_pos = str.find(find_left, from)) &&
           string::npos != (right_pos = str.find(find_right, left_pos))) {
        find_len = right_pos - left_pos - find_left.size();
        auto const_str = str.substr(left_pos + find_left.size() + 1, find_len - 2);
        auto const_str_label = this->makeConstStr(const_str);
        result = replace_all(result, fmt::format("STRING[\"{}\"]", const_str), std::string(const_str_label.get_name()));
        from = right_pos;
    }
    return result;
}

InstGen::Addr cgen::CodeGen::makeConstStr(const string &str) {
    auto const_str = lightir::StringType::get(str, module.get());
    lightir::GlobalVariable::create(fmt::format("const_{}", module->get_global_variable().size()), module.get(),
                                    const_str, true,
                                    lightir::ConstantStr::get(str, module->get_global_variable().size(), module.get()));
    return {fmt::format("const_{}", module->get_global_variable().size() - 1)};
};

} // namespace cgen

#ifdef PA4
int main(int argc, char *argv[]) {
    string target_path;
    string input_path;
    string IR;
    string asm_code;
    vector<string> passes;

    bool emit = false;
    bool run = false;
    bool assem = false;

    for (int i = 1; i < argc; ++i) {
        if (argv[i] == "-h"s || argv[i] == "--help"s) {
            print_help(argv[0]);
            return 0;
        } else if (argv[i] == "-o"s) {
            if (target_path.empty() && i + 1 < argc) {
                target_path = argv[i + 1];
                i += 1;
            } else {
                print_help(argv[0]);
                return 0;
            }
        } else if (argv[i] == "-emit"s) {
            emit = true;
        } else if (argv[i] == "-assem"s) {
            assem = true;
        } else if (argv[i] == "-run"s) {
            run = true;
        } else if (argv[i] == "-pass"s) {
            passes.push_back(argv[i + 1]);
            i += 1;
        } else {
            if (input_path.empty()) {
                input_path = argv[i];
                target_path = replace_all(input_path, ".py", "");
            } else {
                print_help(argv[0]);
                return 0;
            }
        }
    }

    parser::Program *tree = parse(input_path.c_str());

    auto error = std::make_unique<vector<parser::Err *>>();

    auto symboltableGenerator = semantic::SymbolTableGenerator(error.get());
    tree->accept(symboltableGenerator);

    auto declarationAnalyzer = semantic::DeclarationAnalyzer(error.get(), symboltableGenerator.ignore, std::move(symboltableGenerator.globals));
    tree->accept(declarationAnalyzer);

    auto globalScope = std::move(declarationAnalyzer.globals);

    if (!error->empty()) {
        tree->add_error(error.get());
    } else {
        auto *typeChecker = new semantic::TypeChecker(globalScope.get(), error.get());
        tree->accept(*typeChecker);
        if (!error->empty()) {
          tree->add_error(error.get());
        }
    }

    std::shared_ptr<lightir::Module> m;
    if (!error->empty()) {
        tree->add_error(error.get());
    } else {
        cJSON *a = tree->toJSON();
        char *out = cJSON_Print(a);
        LOG(INFO) << "ChocoPy Language Server:\n" << out << "\n";

        auto *LightWalker = new lightir::LightWalker(globalScope.get());
        tree->accept(*LightWalker);
        m = LightWalker->get_module();
        m->source_file_name_ = input_path;
        lightir::PassManager PM(m.get());
        for (auto &&pass : passes)
            PM.add_pass(pass);
        PM.run();

        IR = m->print();
        std::ofstream output_stream;
        auto output_file = target_path + ".ll";

        output_stream.open(output_file, std::ios::out);
        output_stream << fmt::format("; ModuleID = '{}'\n", m->module_name_);
        output_stream << "source_filename = \"" + m->source_file_name_ + "\"\n\n";
        output_stream << IR;
        output_stream.close();

        cgen::CodeGen code_generator(m);
        asm_code = code_generator.generateModuleCode();
        if (assem) {
            cout << "RiscV Asm:\n";

            cout << asm_code;
        }
        std::ofstream output_stream1;
        auto output_file1 = target_path + ".s";
        output_stream1.open(output_file1, std::ios::out);
        output_stream1 << asm_code;
        output_stream1.close();
    }
#ifdef LLVM
    llvmGetPassPluginInfo(m);
#endif
    if (emit) {
        cout << "\nLLVM IR:\n; ModuleID = 'chocopy'\nsource_filename = \"\"" << input_path << "\"\"\n\n" << IR;
    }

    if (run) {
#if defined(WIN32) || defined(_WIN32) || defined(__APPLE__)
#ifdef RV64
        auto command_string_0 = "riscv64-unknown-elf-gcc -mabi=lp64 -g -ggdb -static  -march=rv64imac -o" +
                                target_path + " " + target_path + ".s -L./ -lchocopy_stdlib";
#else
        auto command_string_0 = "riscv64-unknown-elf-gcc -mabi=ilp32 -g -ggdb -static -march=rv32imac -o" +
                                target_path + " " + target_path + ".s -L./ -lchocopy_stdlib";
#endif
        int re_code_0 = std::system(command_string_0.c_str());
        LOG(INFO) << command_string_0 << re_code_0;
#ifdef RV64
        auto command_string_1 = "spike --isa=rv64imac /opt/homebrew/Cellar/riscv-pk/master/bin/pk " + target_path;
#else
        auto command_string_1 ="spike --isa=rv32gcv0p10 /opt/homebrew/Cellar/riscv-pk_32/master/bin/pk " + target_path;
#endif
        int re_code_1 = std::system(command_string_1.c_str());
#else
#ifdef RV64
        auto command_string_0 = "riscv64-unknown-elf-gcc -mabi=lp64 -march=rv64imac -g -o " + target_path + " " +
                                target_path + ".s -L./ -lchocopy_stdlib";
#else
        auto command_string_0 = "riscv64-unknown-elf-gcc -mabi=ilp32 -march=rv32imac -g -o " + target_path + " " +
                                target_path +
                                ".s -L./ -L./build -lchocopy_stdlib";
#endif
        int re_code_0 = std::system(command_string_0.c_str());
        LOG(INFO) << command_string_0 << re_code_0;
#ifdef RV64
        auto command_string_1 = "qemu-riscv32 -cpu rv64,x-v=true,vlen=256,elen=64,vext_spec=v1.0 " + target_path;
#else
        auto command_string_1 = "qemu-riscv32 " + target_path;
#endif
        int re_code_1 = std::system(command_string_1.c_str());
#endif
        LOG(INFO) << command_string_1 << re_code_1;
    }
result:
    return 0;
}
#endif


#ifdef LLVM
llvm::PassPluginLibraryInfo llvmGetPassPluginInfo(const std::shared_ptr<Module> &m) {
    chocopy_m = m;
    return {LLVM_PLUGIN_API_VERSION, "ChoocoPy", "v0.1", [](llvm::PassBuilder &PB) {
                PB.registerPipelineParsingCallback([](llvm::StringRef, llvm::ModulePassManager &MPM,
                                                      llvm::ArrayRef<llvm::PassBuilder::PipelineElement>) {
                    MPM.addPass(cgen::CodeGen());
                    return true;
                });
            }};
}
#endif

#ifdef ALL
int main(int argc, char *argv[]) {
    string target_path;
    string input_path;
    string IR;
    string asm_code;
    vector<string> passes;

    bool O0 = false;
    bool O3 = false;
    bool assem = false;

    for (int i = 1; i < argc; ++i) {
        if (argv[i] == "-h"s || argv[i] == "--help"s) {
            print_help_all(argv[0]);
            return 0;
        } else if (argv[i] == "-o"s) {
            if (target_path.empty() && i + 1 < argc) {
                target_path = argv[i + 1];
                i += 1;
            } else {
                print_help(argv[0]);
                return 0;
            }
        } else if (argv[i] == "-assem"s) {
            assem = true;
        } else if (argv[i] == "-O3"s) {
            O3 = true;
        } else if (argv[i] == "-O0"s) {
            O0 = true;
        } else {
            if (input_path.empty()) {
                input_path = argv[i];
                target_path = replace_all(input_path, ".py", "");
            } else {
                print_help(argv[0]);
                return 0;
            }
        }
    }

    parser::Program *tree = parse(input_path.c_str());

    auto error = std::make_unique<vector<parser::Err *>>();

    auto symboltableGenerator = semantic::SymbolTableGenerator(error.get());
    tree->accept(symboltableGenerator);

    auto declarationAnalyzer = semantic::DeclarationAnalyzer(error.get(), symboltableGenerator.ignore, std::move(symboltableGenerator.globals));
    tree->accept(declarationAnalyzer);

    auto globalScope = std::move(declarationAnalyzer.globals);

    if (!error->empty()) {
        tree->add_error(error.get());
    } else {
        auto *typeChecker = new semantic::TypeChecker(globalScope.get(), error.get());
        tree->accept(*typeChecker);
        if (!error->empty()) {
          tree->add_error(error.get());
        }
    }

    std::shared_ptr<lightir::Module> m;
    if (!error->empty()) {
        tree->add_error(error.get());
    } else {
        cJSON *a = tree->toJSON();
        char *out = cJSON_Print(a);
        LOG(INFO) << "ChocoPy Language Server:\n" << out << "\n";

        auto *LightWalker = new lightir::LightWalker(globalScope.get());
        tree->accept(*LightWalker);
        m = LightWalker->get_module();
        m->source_file_name_ = input_path;
        lightir::PassManager PM(m.get());
        if (O3 == true) {
            // PM.add_pass<lightir::Dominators>();
            // PM.add_pass<lightir::Mem2Reg>();
            // PM.add_pass<lightir::LoopFind>();
            // PM.add_pass<lightir::Vectorization>();
            // PM.add_pass<lightir::Multithread>();
            // PM.add_pass<lightir::ActiveVars>();
            // PM.add_pass<lightir::ConstPropagation>();
        }
        PM.run();

        IR = m->print();
        std::ofstream output_stream;
        auto output_file = target_path + ".ll";

        output_stream.open(output_file, std::ios::out);
        output_stream << fmt::format("; ModuleID = '{}'\n", m->module_name_);
        output_stream << "source_filename = \"" + m->source_file_name_ + "\"\n\n";
        output_stream << IR;
        output_stream.close();

        cgen::CodeGen code_generator(m);
        asm_code = code_generator.generateModuleCode();
        std::ofstream output_stream1;
        auto output_file1 = target_path + ".s";
        output_stream1.open(output_file1, std::ios::out);
        output_stream1 << asm_code;
        output_stream1.close();
        if (assem) {
            auto command_string = "cat " + target_path + ".s ";
            int re_code = std::system(command_string.c_str());
            LOG(INFO) << command_string << re_code;
        }
    }

#if defined(WIN32) || defined(_WIN32) || defined(__APPLE__)
#ifdef RV64
            auto command_string_0 = "riscv64-unknown-elf-gcc -mabi=lp64 -g -ggdb -static  -march=rv64imac -o" +
                                    target_path + " " + target_path + ".s -L./ -lchocopy_stdlib";
#else
            auto command_string_0 = "riscv64-unknown-elf-gcc -mabi=ilp32 -g -ggdb -static -march=rv32imac -o" +
                                    target_path + " " + target_path + ".s -L./ -lchocopy_stdlib";
#endif
            int re_code_0 = std::system(command_string_0.c_str());
            LOG(INFO) << command_string_0 << re_code_0;
#ifdef RV64
            auto command_string_1 = "spike --isa=rv64imac /opt/homebrew/Cellar/riscv-pk/master/bin/pk " + target_path;
#else
            auto command_string_1 =
                "spike --isa=rv32gcv0p10 /opt/homebrew/Cellar/riscv-pk_32/master/bin/pk " + target_path;
#endif
            int re_code_1 = std::system(command_string_1.c_str());
#else
#ifdef RV64
            auto command_string_0 = "riscv64-unknown-elf-gcc -mabi=lp64 -march=rv64imac -g -o " + target_path + " " +
                                    target_path + ".s -L./ -lchocopy_stdlib";
#else
            auto command_string_0 =
                "riscv64-unknown-elf-gcc -mabi=ilp32 -march=rv32imac -g -o " + target_path + " " + target_path +
                ".s -L./ -L/Users/yiweiyang/project/bak/cmake-build-debug-kali-gcc -lchocopy_stdlib";
#endif
            int re_code_0 = std::system(command_string_0.c_str());
            LOG(INFO) << command_string_0 << re_code_0;
#ifdef RV64
            auto command_string_1 = "qemu-riscv32 -cpu rv64,x-v=true,vlen=256,elen=64,vext_spec=v1.0 " + target_path;
#else
            auto command_string_1 = "qemu-riscv32 " + target_path;
#endif
            int re_code_1 = std::system(command_string_1.c_str());
#endif
            LOG(INFO) << command_string_1 << re_code_1;
result:
    return 0;
}
#endif