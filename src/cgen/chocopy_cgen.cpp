#include "BasicBlock.hpp"
#include "Constant.hpp"
#include "Function.hpp"
#include "GlobalVariable.hpp"
#include "InstGen.hpp"
#include "Module.hpp"
#include "RiscVBackEnd.hpp"
#include "Type.hpp"
#include "Value.hpp"
#include <cassert>
#include <chocopy_cgen.hpp>
#include <chocopy_lightir.hpp>
#include <fmt/core.h>
#include <string>
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
        return fmt::format("{}({})",std::to_string(this->offset), reg.get_name());
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

void Interval::addRange(int l, int r) {
    auto it = ranges.lower_bound({l, 0});
    if (it != ranges.end() && it->first == l) {
        if (it->second < r) {
            ranges.erase(it);
            ranges.insert({l, r});
        }
    } else {
        ranges.insert({l, r});
    }
}
void Interval::setCreatePosition(int pos) {
    if (ranges.size() > 0) {
        std::pair<int, int> s = *ranges.begin();
        ranges.erase(ranges.begin());
        ranges.insert({pos, s.second});
    }
}
bool Interval::overlaps(const Interval& i) const {
    for (const auto& range : i.ranges) {
        auto it = ranges.lower_bound({range.second, 0});
        if (it == ranges.begin())
            continue;
        --it;
        if (it->second > range.first) {
            return true;
        }
    }
    return false;
}

string CodeGen::stackToReg(InstGen::Addr addr, InstGen::Reg reg) {
    if (-2048 <= addr.getOffset() && addr.getOffset() < 2048) {
        return backend->emit_lw(reg, addr.getReg(), addr.getOffset());
    } else {
        string asm_code;
        const auto t0 = InstGen::Reg("t0");
        asm_code += backend->emit_li(t0, addr.getOffset());
        asm_code += backend->emit_add(t0, addr.getReg(), t0);
        asm_code += backend->emit_lw(reg, t0, 0);
        return asm_code;
    }
}
string CodeGen::regToStack(InstGen::Reg reg, InstGen::Addr addr) {
    if (-2048 <= addr.getOffset() && addr.getOffset() < 2048) {
        return backend->emit_sw(reg, addr.getReg(), addr.getOffset());
    } else {
        string asm_code;
        const auto t0 = InstGen::Reg("t0");
        asm_code += backend->emit_li(t0, addr.getOffset());
        asm_code += backend->emit_add(t0, addr.getReg(), t0);
        asm_code += backend->emit_sw(reg, t0, 0);
        return asm_code;
    }
}
string CodeGen::regToStack(const string &vreg) {
    assert(vreg_to_reg.contains(vreg) && vreg_to_stack_slot.contains(vreg));
    return regToStack(vreg_to_reg.at(vreg), vreg_to_stack_slot.at(vreg));
}
InstGen::Reg CodeGen::getReg(const string &vreg, int fallback) {
    if (vreg_to_stack_slot.contains(vreg)) 
        return InstGen::Reg(fallback);
    else if (vreg_to_reg.contains(vreg))
        return vreg_to_reg.at(vreg);
    else
        return InstGen::Reg(fallback);
}
string CodeGen::vregToReg(Value* vreg, InstGen::Reg reg) {
    if (dynamic_cast<ConstantNull*>(vreg)) {
        return backend->emit_li(reg, 0);
    } else if (auto c = dynamic_cast<ConstantInt*>(vreg); c) {
        return backend->emit_li(reg, c->get_value());
    } else if (auto a = dynamic_cast<AllocaInst*>(vreg)) {
        assert (stack_size != 0);
        return backend->emit_addi(reg, InstGen::Reg("fp"), alloca_to_stack_slot.at(a->get_name()).getOffset());
    } else if (auto cls = dynamic_cast<Class*>(vreg); cls) {
        return backend->emit_la(reg, InstGen::Addr(cls->prototype_label_));
    } else if (auto name = vreg->get_name(); GOT.contains(name)) {
        return backend->emit_la(reg, InstGen::Addr(name));
    }

    if (vreg_to_stack_slot.contains(vreg->get_name())) {
        return stackToReg(vreg_to_stack_slot.at(vreg->get_name()), reg);
    } else {
        auto r = vreg_to_reg.at(vreg->get_name());
        if (r == reg) return "";
        else return backend->emit_mv(reg, r);
    }
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
    asm_code += ".data\n";
    for (auto &classInfo : this->module->get_class()) {
        asm_code += backend->emit_prototype(*classInfo);
    }
    asm_code += generateGlobalVarsCode();

    asm_code += ".text\n";
    for (auto func : this->module->get_functions()) {
        assert(dynamic_cast<Function*>(func));
        if (func->get_basic_blocks().size()) {
            asm_code += CodeGen::generateFunctionCode(func);
        }
    }
    return asm_code;
}

void CodeGen::lifetimeAnalysis() {
    // std::cerr << "Lifetime analysis for function " << current_function->get_name() << std::endl;
    basic_block_from.clear();
    basic_block_to.clear();
    inst_id.clear();
    live_in.clear();
    intervals.clear();
    auto& basic_blocks = current_function->get_basic_blocks();

    int inst_count = 0;
    map<BasicBlock*, std::vector<PhiInst*>> phi_instructions;
    for (auto &bb : basic_blocks) {
        basic_block_from[bb] = inst_count++;
        for (auto &inst : bb->get_instructions()) {
            inst_id[inst] = inst_count++;
            if (auto phi = dynamic_cast<PhiInst*>(inst); phi) {
                assert(phi->get_num_operand() == 4);
                phi_instructions[bb].push_back(phi);
            }
        }
        basic_block_to[bb] = inst_count++;
    }

    bool changed = true;
    while (changed) {
        changed = false;
        for (auto bb_iter = basic_blocks.crbegin(); bb_iter != basic_blocks.crend(); ++bb_iter) {
            const auto bb = *bb_iter;
            // std::cerr << "Lifetime analysis for block " << bb->get_name() << std::endl;
            std::set<std::string> live;

            // std::cerr << "union of successor.liveIn" << std::endl;
            for (const auto succ_bb : bb->get_succ_basic_blocks()) {
                const auto& succ_bb_live_in = live_in[succ_bb];
                // std::cerr << "  " << succ_bb->get_name() << ": ";
                // for (const auto& opd : succ_bb_live_in) {
                //     std::cerr << opd << " ";
                // }
                live.insert(succ_bb_live_in.cbegin(), succ_bb_live_in.cend());
                for(const auto phi : phi_instructions[succ_bb]) {
                    const auto& ops = phi->get_operands();
                    for(int i = 0; i < 4; i += 2) {
                        const auto v = ops[i];
                        const auto pred = ops[i+1];
                        if (pred == bb) {
                            live.insert(v->get_name());
                        }
                    }
                }
            }
            // std::cerr << "end union" << std::endl;

            int bb_from = basic_block_from[bb];
            int bb_to = basic_block_to[bb];
            for (const auto& opd : live) {
                // std::cerr << opd << " add range " << bb_from << " " << bb_to << std::endl;
                intervals[opd].addRange(bb_from, bb_to);
            }

            for (auto inst_iter = bb->get_instructions().rbegin(); inst_iter != bb->get_instructions().rend(); ++inst_iter) {
                const auto inst = *inst_iter;
                int id = inst_id[inst];

                live.erase(inst->get_name());
                if (dynamic_cast<PhiInst*>(inst)) continue;
                // std::cerr << " " << inst->get_name() << " setCreatePosition " << id << std::endl;
                intervals[inst->get_name()].setCreatePosition(id);

                if (dynamic_cast<AsmInst*>(inst)) continue;;
                for (const auto& op : inst->get_operands()) {
                    if (dynamic_cast<GlobalVariable*>(op) || dynamic_cast<Class*>(op) || dynamic_cast<Function*>(op) || dynamic_cast<BasicBlock*>(op))
                        continue;
                    // std::cerr << " " << op->get_name() << " addRange " << bb_from << ' ' << id << std::endl;
                    intervals[op->get_name()].addRange(bb_from, id);
                    live.insert(op->get_name());
                }
            }

            live.erase("");
            // std::cerr << "-!-!-!-live in: ";
            // for (const auto& opd : live) {
            //     std::cerr << opd << " ";
            // }
            // std::cerr << std::endl;
            // std::cerr << "current live size: " << live.size() << " before: " << live_in[bb].size() << std::endl;
            if (live.size() != live_in[bb].size()) {
                changed = true;
                live_in[bb] = std::move(live);
            }
            // std::cerr << "Done lifetime analysis for block " << bb->get_name() << std::endl << std::endl;
        }
    }
    intervals.erase("");

    // std::cerr << "result of lifetime analysis" << std::endl;

    // for (auto &bb : basic_blocks) {
    //     std::cerr << basic_block_from[bb] << " " << bb->get_name() << ":" << std::endl;
    //     for (auto &inst : bb->get_instructions()) {
    //         std::cerr << inst_id[inst] << ": " << inst->print() << std::endl; 
    //     }
    //     std::cerr << basic_block_to[bb] << " " << bb->get_name() << " end" << std::endl;
    // }

    // for (const auto& kv : intervals) {
    //     std::cerr << kv.first;
    //     std::cerr << ": intervals:";
    //     for (const auto& p : kv.second.ranges) {
    //         std::cerr << fmt::format(" [{}, {}]", p.first, p.second);
    //     } 
    //     std::cerr << std::endl;
    // }

    // std::cerr << "Lifetime analysis done" << std::endl << std::endl;
}

void CodeGen::linearScan() {
    using Reg = InstGen::Reg;
    using Addr = InstGen::Addr;
    // std::cerr << "Linear scan for function " << current_function->get_name() << std::endl;

    vreg_to_reg.clear(); vreg_to_stack_slot.clear(); reg_to_vreg.clear(); alloca_to_stack_slot.clear();

    int call_count = 0;
    std::set<std::string> call_inst_names;
    std::map<std::string, int> alloca_inst_to_bytes;
    for (auto bb : current_function->get_basic_blocks()) {
        for (auto inst : bb->get_instructions()) {
            if (dynamic_cast<CallInst*>(inst)) {
                if (inst->get_name() == "") {
                    call_inst_names.insert(fmt::format("call{}", call_count));
                    intervals[fmt::format("call{}", call_count)].addRange(inst_id[inst], inst_id[inst]);
                    call_count++;
                } else {
                    call_inst_names.insert(inst->get_name());
                    if (intervals.at(inst->get_name()).ranges.size() == 0) {
                        intervals.at(inst->get_name()).addRange(inst_id[inst], inst_id[inst]);
                    }
                }
            }
            if (dynamic_cast<AllocaInst*>(inst)) {
                alloca_inst_to_bytes.insert({inst->get_name(), getTypeSizeInBytes(((AllocaInst*)inst)->get_alloca_type())});
            }
        }
    }

    std::set<std::string> active, inactive;
    std::vector<std::string> unhandled;
    unhandled.reserve(intervals.size());
    for (const auto& kv : intervals) {
        if (kv.second.ranges.size() > 0)
            unhandled.push_back(kv.first);
    }
    std::sort(unhandled.begin(), unhandled.end(), [this](const std::string& a, const std::string& b) {
        return intervals[a].ranges.begin()->first < intervals[b].ranges.begin()->first;
    });

    const std::vector<Reg> regs_going_to_be_used = {
        Reg(9), Reg(18), Reg(19), Reg(20), Reg(21), Reg(22), Reg(23), Reg(24), Reg(25), Reg(26), Reg(27), // s2 - s11
        Reg(10), Reg(11), Reg(12), Reg(13), Reg(14), Reg(15), Reg(16), Reg(17), // a0 - a7
        Reg(28), Reg(29), Reg(30), Reg(31), // t3 - t6
    };
    std::set<Reg> regs_used;

    auto pair_vreg_reg = [this, &regs_used](const std::string& vreg, Reg reg) {
        vreg_to_reg.insert({vreg, reg});
        reg_to_vreg.insert({reg, vreg});
        regs_used.insert(reg);
    };
    int offset = 0;
    auto alloca_stack_slot = [this, &offset](int size) -> Addr {
        offset -= size;
        return Addr(Reg(8), offset);
    };
    auto assign_vreg_stack_slot = [this, &alloca_stack_slot](const std::string& vreg, int size = 4) {
        assert(!vreg_to_stack_slot.contains(vreg));
        vreg_to_stack_slot.insert({vreg, alloca_stack_slot(size)});
    };

    auto is_reg_conflict = [&active, &inactive, this](const Reg& reg, const Interval& interval) -> bool {
        if (reg_to_vreg.contains(reg)) {
            return true;
        }
        for (const auto& vreg : inactive) {
            if (vreg_to_reg.at(vreg) != reg || vreg_to_stack_slot.contains(vreg))
                continue; 
            const auto& i = intervals[vreg];
            if (interval.overlaps(i))
                return true;
        }
        return false;
    };
    auto move_conflict_vreg_to_stack = [this, &active, &inactive, &assign_vreg_stack_slot](const Reg& reg, const Interval& interval) -> void {
        if (reg_to_vreg.contains(reg)) {
            assign_vreg_stack_slot(reg_to_vreg.at(reg));
            reg_to_vreg.erase(reg);
        }
        for (const auto& vreg : inactive) {
            if (vreg_to_reg.at(vreg) != reg || vreg_to_stack_slot.contains(vreg))
                continue; 
            const auto& i = intervals[vreg];
            if (interval.overlaps(i)) {
                assign_vreg_stack_slot(vreg);
            }
        }
    };
    auto get_reg = [&](const Interval& interval) -> Reg {
        for (auto reg : regs_used) {
            if (is_reg_conflict(reg, interval)) continue;
            return reg;
        }
        for (auto reg : regs_going_to_be_used) {
            if (is_reg_conflict(reg, interval)) continue;
            return reg;
        }
        // randomly pick a reg
        auto reg = regs_going_to_be_used[rand() % regs_going_to_be_used.size()];
        move_conflict_vreg_to_stack(reg, interval);
        return reg;
    };

    if (call_inst_names.size() > 0) {
        vreg_to_reg.insert({"ra", Reg("ra")});
        assign_vreg_stack_slot("ra");
    }
    int args_nums = current_function->get_num_of_args();
    for (int i = 0; i < args_nums; i++) {
        const auto op = fmt::format("arg{}", i);
        pair_vreg_reg(op, Reg(10 + i));
    }

    for (int i = 8; i < args_nums; i++) {
        const auto op = fmt::format("arg{}", i);
        vreg_to_stack_slot.insert({op, Addr(Reg(8), (i - 8) * 4)});
    }

    for (const auto& op : unhandled) {
        // std::cerr << "handling interval " << op << std::endl;
        const auto& interval = intervals[op];
        int pos = interval.ranges.begin()->first;

        // active -> active, inactive
        for (auto vreg_iter = active.begin(); vreg_iter != active.end(); ) {
            if (vreg_to_stack_slot.contains(*vreg_iter)) {
                vreg_iter = active.erase(vreg_iter);
                continue;
            }
            const auto& i = intervals.at(*vreg_iter);
            auto it = i.ranges.lower_bound({pos, 0});
            if (it == i.ranges.begin()) {
                assert(i.ranges.begin()->first == pos);
                continue;
            }
            --it;
            if (it->second <= pos) { // inactive
                // std::cerr << "inactivate " << *vreg_iter << std::endl;
                reg_to_vreg.erase(vreg_to_reg.at(*vreg_iter));
                inactive.insert(*vreg_iter);
                vreg_iter = active.erase(vreg_iter);
            } else {
                ++vreg_iter;
            }
        }
        // inactive -> inactive, active
        for (auto vreg_iter = inactive.begin(); vreg_iter != inactive.end(); ) {
            if (vreg_to_stack_slot.contains(*vreg_iter)) {
                vreg_iter = active.erase(vreg_iter);
                continue;
            }
            const auto& i = intervals.at(*vreg_iter);
            auto it = i.ranges.lower_bound({pos, 0});
            assert(it != i.ranges.begin());
            --it;
            if (pos < it->second) { // active
                // std::cerr << "activate " << *vreg_iter << std::endl;
                auto reg = vreg_to_reg.at(*vreg_iter);
                assert(!reg_to_vreg.contains(reg));
                reg_to_vreg.insert({reg, *vreg_iter});
                active.insert(*vreg_iter);
                vreg_iter = inactive.erase(vreg_iter);
            } else {
                ++vreg_iter;
            }
        }
        auto remove_handled_element = [this, pos](std::set<std::string>& s) {
            for (auto it = s.begin(); it != s.end();) {
                if (intervals[*it].ranges.rbegin()->second <= pos) {
                    s.erase(it++);
                } else {
                    ++it;
                }
            }
        };
        remove_handled_element(active);
        remove_handled_element(inactive);

        // std::cerr << "active: "; for (const auto& vreg: active) std::cerr << vreg << ' '; std::cerr << std::endl;
        // std::cerr << "inactive: "; for (const auto& vreg: inactive) std::cerr << vreg << ' '; std::cerr << std::endl;
        // std::cerr << "registers: "; for (const auto& kv : reg_to_vreg) std::cerr << kv.first.get_name() << "->" << kv.second << ' '; std::cerr << std::endl;

        if (call_inst_names.contains(op)) {
            // std::cerr << "handling call inst " << op << std::endl;
            
            auto i = Interval(); i.addRange(pos, pos);
            for (const auto& reg : caller_save_regs) {
                move_conflict_vreg_to_stack(reg, i);
            }
            if (op.starts_with("op")) {
                pair_vreg_reg(op, Reg(10));
                active.insert(op);
            }
        } else if (op.starts_with("arg")) {
        } else {
            if (alloca_inst_to_bytes.contains(op)) {
                alloca_to_stack_slot.insert({op, alloca_stack_slot(alloca_inst_to_bytes.at(op))});
            } else {
                auto reg = get_reg(interval);
                // std::cerr << "assign " << reg.get_name() << " to " << op << std::endl;
                pair_vreg_reg(op, reg);
                active.insert(op);
            }
        }
    }

    for (auto reg : regs_used) {
        if (reg.getID() == 9 || (18 <= reg.getID() && reg.getID() <= 27)) {
            assign_vreg_stack_slot(reg.get_name());
        }
    }
    if (offset != 0) {
        assign_vreg_stack_slot("fp");
    }
    stack_size = -offset;
    stack_size = (stack_size + 15) & ~15;

    // std::cerr << "Linear scan result" << std::endl;
    // std::cerr << "stack size: " << stack_size << std::endl;
    // for (const auto& kv : vreg_to_reg) {
    //     std::cerr << kv.first << " -> " << kv.second.get_name() << std::endl;
    // }
    // for (const auto& kv : vreg_to_stack_slot) {
    //     std::cerr << kv.first << " -> " << kv.second.get_name() << std::endl;
    // }
    // for (const auto& kv : alloca_to_stack_slot) {
    //     std::cerr << kv.first << " -> " << kv.second.get_name() << std::endl;
    // }

    // std::cerr << "Linear scan done" << std::endl << std::endl;
}

string CodeGen::generateFunctionCode(Function *func) {
    using Reg = InstGen::Reg;
    using Addr = InstGen::Addr;
    current_function = func;

    // lifetime analysis
    lifetimeAnalysis();

    // register allocation
    linearScan();

    string asm_code;
    asm_code += fmt::format(".globl {}\n{}:\n", func->get_name(), func->get_name());

    phi_store.clear();
    for(auto b : func->get_basic_blocks()) {
        for (auto i : b->get_instructions()) {
            if (auto phi = dynamic_cast<PhiInst*>(i); phi) {
                if (!vreg_to_reg.contains(phi->get_name())) continue;
                auto ops = phi->get_operands();
                assert(phi->get_operands().size() == 4);
                assert(dynamic_cast<BasicBlock*>(ops[1]) && dynamic_cast<BasicBlock*>(ops[3]));
                auto v1 = ops[0];
                auto b1 = (BasicBlock*)ops[1];
                auto v2 = ops[2];
                auto b2 = (BasicBlock*)ops[3];
                phi_store[b1].push_back({v1, phi->get_name()});
                phi_store[b2].push_back({v2, phi->get_name()});
            }
        }
    }

    const Reg fp = Reg("fp");
    const Reg sp = Reg("sp");
    const Reg t0 = Reg("t0");
    if (stack_size != 0) {
        asm_code += regToStack(Reg("fp"), Addr(sp, vreg_to_stack_slot.at("fp").getOffset()));
        asm_code += backend->emit_mv(fp, sp);
        if (-2048 <= -stack_size) {
            asm_code += backend->emit_addi(sp, sp, -stack_size);
        } else {
            asm_code += backend->emit_li(t0, -stack_size);
            asm_code += backend->emit_add(sp, sp, t0);
        }
    } else {
        for (auto& kv : vreg_to_stack_slot) {
            kv.second.setReg(sp);
        }
    }
    const int callee_save_regs[] = {9, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27};
    for (auto reg : callee_save_regs) {
        if (vreg_to_stack_slot.contains(reg_name[reg])) {
            asm_code += regToStack(Reg(reg), vreg_to_stack_slot.at(reg_name[reg]));
        }
    }
    if (vreg_to_stack_slot.contains("ra")) {
        asm_code += regToStack("ra");
    }

    for (int i = 0; i < std::min(8u, func->get_num_of_args()); i++) {
        auto it = vreg_to_stack_slot.find(fmt::format("arg{}", i));
        if (it != vreg_to_stack_slot.end()) {
            asm_code += regToStack(Reg(10+i), it->second);
        }
    }
    for(auto b : func->get_basic_blocks()) {
        asm_code += fmt::format("{}:\n", getLabelName(b), b->get_name());
        asm_code += generateBasicBlockCode(b);
    }

    asm_code += fmt::format("{}$return:\n", func->get_name());
    if (vreg_to_stack_slot.contains("ra")) {
        asm_code += stackToReg(vreg_to_stack_slot.at("ra"), Reg("ra"));
    }
    for (auto reg : callee_save_regs) {
        if (vreg_to_stack_slot.contains(reg_name[reg])) {
            asm_code += stackToReg(vreg_to_stack_slot.at(reg_name[reg]), Reg(reg));
        }
    }
    if (stack_size != 0) {
        asm_code += stackToReg(vreg_to_stack_slot.at("fp"), fp);
        if (-2048 <= -stack_size) {
            asm_code += backend->emit_addi(sp, sp, stack_size);
        } else {
            asm_code += backend->emit_li(t0, stack_size);
            asm_code += backend->emit_add(sp, sp, t0);
        }
    }
    asm_code += fmt::format("  ret\n");
    return asm_code;
}

CodeGen::CodeGen(shared_ptr<Module> module) : module(move(module)), backend(new RiscVBackEnd()) {}

[[nodiscard]]  string CodeGen::generateFunctionExitCode() {
    std::string asm_code;
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
            auto src = kv.first;
            const auto& dst = kv.second;
            auto rd = getReg(dst);
            asm_code += vregToReg(src, rd);
            if (vreg_to_stack_slot.contains(dst)) {
                asm_code += regToStack(dst);
            }
        }
    }
    return asm_code;
}
string CodeGen::generateInstructionCode(Instruction *inst) {
    using Reg = InstGen::Reg;
    using Addr = InstGen::Addr;
    // std::cerr << inst->print() << std::endl;
    std::string asm_code;
    auto &ops = inst->get_operands();
    switch (inst->get_instr_type()) {
        case lightir::Instruction::Ret: {
            assert(ops.size() == 0 || ops.size() == 1);
            if (ops.size() == 1) {
                asm_code += vregToReg(ops[0], Reg(10));
            }
            asm_code += fmt::format("  j {}$return\n", current_function->get_name());
            break;
        }
        case lightir::Instruction::Br: {
            asm_code += generateBasicBlockPostCode(current_basic_block);
            if (ops.size() == 1) {
                // unconditional branch
                assert(dynamic_cast<BasicBlock*>(ops[0]));
                asm_code += fmt::format("  j {}\n", getLabelName((BasicBlock*)ops[0]));
            } else if (ops.size() == 3) {
                assert(dynamic_cast<BasicBlock*>(ops[1]));
                auto rs = getReg(ops[0]->get_name());
                asm_code += vregToReg(ops[0], rs);
                asm_code += backend->emit_beq(rs, Reg(0), Addr(getLabelName((BasicBlock*)ops[2])));
                asm_code += backend->emit_j(getLabelName((BasicBlock*)ops[1]));
            } else {
                assert(0);
            }
            break;
        }
        case lightir::Instruction::Neg:
        case lightir::Instruction::Not: {
            if (!vreg_to_reg.contains(inst->get_name())) break;
            Reg rd = vreg_to_reg.at(inst->get_name());
            assert(ops.size() == 1);
            auto rs = getReg(ops[0]->get_name());
            asm_code += vregToReg(ops[0], rs);
            if (inst->get_instr_type() == lightir::Instruction::Not)
                asm_code += backend->emit_xori(rd, rs, 1);
            else
                asm_code += backend->emit_neg(rd, rs);
            if (vreg_to_stack_slot.contains(inst->get_name())) {
                asm_code += regToStack(inst->get_name());
            }
            break;
        }
        case lightir::Instruction::Add:
        case lightir::Instruction::Sub:
        case lightir::Instruction::Mul:
        case lightir::Instruction::Div:
        case lightir::Instruction::Rem:
        case lightir::Instruction::And:
        case lightir::Instruction::Or: {
            if (!vreg_to_reg.contains(inst->get_name())) break;
            Reg rd = vreg_to_reg.at(inst->get_name());
            char const * asm_inst_name;
            switch (inst->get_instr_type()) {
                case lightir::Instruction::Add: asm_inst_name = "add"; break;
                case lightir::Instruction::Sub: asm_inst_name = "sub"; break;
                case lightir::Instruction::Mul: asm_inst_name = "mul"; break;
                case lightir::Instruction::Div: asm_inst_name = "div"; break;
                case lightir::Instruction::Rem: asm_inst_name = "rem"; break;
                case lightir::Instruction::And: asm_inst_name = "and"; break;
                case lightir::Instruction::Or: asm_inst_name = "or"; break;
                default: assert(0);
            }
            assert(ops.size() == 2);
            auto rs1 = getReg(ops[0]->get_name(), 5);
            auto rs2 = getReg(ops[1]->get_name(), 6);
            asm_code += vregToReg(ops[0], rs1);
            asm_code += vregToReg(ops[1], rs2);
            asm_code += fmt::format("  {} {}, {}, {}\n", asm_inst_name, rd.get_name(), rs1.get_name(), rs2.get_name());
            if (vreg_to_stack_slot.contains(inst->get_name())) {
                asm_code += regToStack(inst->get_name());
            }
            break;
        }
        case lightir::Instruction::Alloca: {
            break;
        }
        case lightir::Instruction::Load: {
            if (!vreg_to_reg.contains(inst->get_name())) break;
            Reg rd = vreg_to_reg.at(inst->get_name());
            assert(ops.size() == 1);
            string op = ops[0]->get_name();
            if (inst->get_type()->print() == "i8") {
                auto rs = getReg(op);
                asm_code += vregToReg(ops[0], rs);
                asm_code += backend->emit_lbu(rd, rs, 0);
                asm_code += backend->emit_andi(rd, rd, 255);
            } else {
                if (alloca_to_stack_slot.contains(op)) {
                    asm_code += stackToReg(alloca_to_stack_slot.at(op), rd);
                } else if (GOT.contains(op)) {
                    // asm_code += fmt::format("  lui t1, %hi({})\n  lw t1, %lo({})(t1)\n", op, op);
                    const auto rd_name = rd.get_name();
                    asm_code += fmt::format("  lui {}, %hi({})\n  lw {}, %lo({})({})\n", rd_name, op, rd_name, op, rd_name);
                } else {
                    auto rs = getReg(op);
                    asm_code += vregToReg(ops[0], rs);
                    asm_code += backend->emit_lw(rd, rs, 0);
                }
            }
            if (vreg_to_stack_slot.contains(inst->get_name())) {
                asm_code += regToStack(inst->get_name());
            }
            break;
        }
        case lightir::Instruction::Store: {
            auto vreg = ops[0];
            auto rs1 = Reg(5);

            if (dynamic_cast<ConstantNull*>(vreg)) {
                asm_code += backend->emit_li(rs1, 0);
            } else if (auto c = dynamic_cast<ConstantInt*>(vreg); c) {
                asm_code += backend->emit_li(rs1, c->get_value());
            } else if (auto a = dynamic_cast<AllocaInst*>(vreg)) {
                assert(stack_size != 0);
                asm_code += backend->emit_addi(rs1, InstGen::Reg("fp"), alloca_to_stack_slot.at(a->get_name()).getOffset());
            } else if (auto cls = dynamic_cast<Class*>(vreg); cls) {
                asm_code += backend->emit_la(rs1, InstGen::Addr(cls->prototype_label_));
            } else if (auto name = vreg->get_name(); GOT.contains(name)) {
                asm_code += backend->emit_la(rs1, InstGen::Addr(name));
            } else {
                rs1 = getReg(vreg->get_name());
                asm_code += vregToReg(vreg, rs1);
            }

            if (dynamic_cast<AllocaInst*>(ops[1])) {
                asm_code += regToStack(rs1, alloca_to_stack_slot.at(ops[1]->get_name()));
            } else {
                auto rs2 = getReg(ops[1]->get_name(), 6);
                asm_code += vregToReg(ops[1], rs2);
                asm_code += regToStack(rs1, InstGen::Addr(rs2, 0));
            }
            break;
        }
        case lightir::Instruction::ICmp: {
            if (!vreg_to_reg.contains(inst->get_name())) break;
            Reg rd = vreg_to_reg.at(inst->get_name());
            auto op = ((CmpInst*)inst)->get_cmp_op();
            auto rs1 = getReg(ops[0]->get_name(), 5);
            auto rs2 = getReg(ops[1]->get_name(), 6);
            asm_code += vregToReg(ops[0], rs1);
            asm_code += vregToReg(ops[1], rs2);
            if (op == CmpInst::EQ) {
                asm_code += backend->emit_xor(rd, rs1, rs2);
                asm_code += backend->emit_seqz(rd, rd);
            } else if (op == CmpInst::NE) {
                asm_code += backend->emit_xor(rd, rs1, rs2);
                asm_code += backend->emit_snez(rd, rd);
            } else if (op == CmpInst::LT) {
                asm_code += backend->emit_slt(rd, rs1, rs2);
            } else if (op == CmpInst::LE) {
                asm_code += backend->emit_slt(rd, rs2, rs1);
                asm_code += backend->emit_xori(rd, rd, 1);
            } else if (op == CmpInst::GT) {
                asm_code += backend->emit_slt(rd, rs2, rs1);
            } else if (op == CmpInst::GE) {
                asm_code += backend->emit_slt(rd, rs1, rs2);
                asm_code += backend->emit_xori(rd, rd, 1);
            } else {
                assert(0);
            }
            if (vreg_to_stack_slot.contains(inst->get_name())) {
                asm_code += regToStack(inst->get_name());
            }
            break;
        }
        case lightir::Instruction::PHI: {
            break;
        }
        case lightir::Instruction::Call: {
            if (dynamic_cast<Function*>(ops[0])) {
                auto func_name = ops[0]->get_name();
                asm_code += generateFunctionCall(inst, fmt::format("  call {}\n", func_name), ops);
            } else {
                assert(ops[0]->print() != "");
                asm_code += vregToReg(ops[0], Reg(6));
                asm_code += generateFunctionCall(inst, fmt::format("  jalr t1\n"), ops);
            }
            if (vreg_to_stack_slot.contains(inst->get_name())) {
                asm_code += regToStack(inst->get_name());
            }
            break;
        }
        case lightir::Instruction::GEP: {
            if (!vreg_to_reg.contains(inst->get_name())) break;
            Reg rd = vreg_to_reg.at(inst->get_name());
            auto gep = (GetElementPtrInst*)inst;
            auto ptr = ops[0];
            assert(dynamic_cast<ArrayType*>(ptr->get_type()) && ((ArrayType*)ptr->get_type())->get_num_of_elements() == -1);
            auto inner_type = ((ArrayType*)ptr->get_type())->get_element_type();
            if (dynamic_cast<Class*>(inner_type) || inner_type->print().ends_with("$dispatchTable_type")) {
                // it seems that every attribute is 4 bytes
                assert(dynamic_cast<ConstantInt*>(ops[1]));
                auto idx = ((ConstantInt*)ops[1])->get_value();
                auto rs = getReg(ptr->get_name());
                asm_code += vregToReg(ptr, rs);
                asm_code += backend->emit_addi(rd, rs, idx * 4);
            } else if (inner_type->print() == "%$union.conslist") {
                auto rs1 = getReg(ptr->get_name());
                asm_code += vregToReg(ptr, rs1);
                auto rs2 = getReg(ops[1]->get_name());
                asm_code += vregToReg(ops[1], rs2);
                auto t0 = Reg(5);
                asm_code += backend->emit_slli(t0, rs2, 2);
                asm_code += backend->emit_add(rd, rs1, t0);
            } else if (auto i = dynamic_cast<IntegerType*>(inner_type)) {
                auto rs1 = getReg(ptr->get_name());
                asm_code += vregToReg(ptr, rs1);
                auto rs2 = getReg(ops[1]->get_name());
                asm_code += vregToReg(ops[1], rs2);
                auto t0 = Reg(5);
                if (i->get_num_bits() == 32 || i->get_num_bits() == 1) {
                    asm_code += backend->emit_slli(t0, rs2, 2);
                    asm_code += "  slli t1, t1, 2\n";
                    asm_code += backend->emit_add(rd, rs1, t0);
                } else {
                    assert(i->get_num_bits() == 8);
                    asm_code += backend->emit_add(rd, rs1, rs2);
                }
            } else {
                std::cerr << inner_type->print() << std::endl;
                assert(0 && "not implemented");
            }
            if (vreg_to_stack_slot.contains(inst->get_name())) {
                asm_code += regToStack(inst->get_name());
            }
            break;
        }
        case lightir::Instruction::ZExt: {
            if (!vreg_to_reg.contains(inst->get_name())) break;
            Reg rd = vreg_to_reg.at(inst->get_name());
            assert(ops.size() == 1);
            auto rs = getReg(ops[0]->get_name());
            asm_code += vregToReg(ops[0], rs);
            if (rd != rs) {
                asm_code += backend->emit_mv(rd, rs);
            }
            if (vreg_to_stack_slot.contains(inst->get_name())) {
                asm_code += regToStack(inst->get_name());
            }
            break;
        }
        case lightir::Instruction::BitCast: {
            if (!vreg_to_reg.contains(inst->get_name())) break;
            Reg rd = vreg_to_reg.at(inst->get_name());
            asm_code += vregToReg(ops[0], rd);
            if (vreg_to_stack_slot.contains(inst->get_name())) {
                asm_code += regToStack(inst->get_name());
            }
            break;
        }
        case lightir::Instruction::ASM: {
            string asm_ = ((AsmInst*)inst)->get_asm();
            asm_ = std::regex_replace(asm_, std::regex("\\\\0A"), "\n  ");
            asm_code += "  " + asm_ + "\n";
            break;
        }
        case lightir::Instruction::InElem:
        case lightir::Instruction::ExElem:
        case lightir::Instruction::Trunc:
        case lightir::Instruction::VExt:
        case lightir::Instruction::Shl:
        case lightir::Instruction::AShr:
        case lightir::Instruction::LShr:
        case lightir::Instruction::ACCSTART:
        case lightir::Instruction::ACCEND: {
            assert(0);
            break;
        }
    }
    // std::cerr << asm_code << std::endl;
    // std::cerr << "---" << std::endl;
    return asm_code;
}
string CodeGen::getLabelName(BasicBlock *bb) { return "." + bb->get_parent()->get_name() + "_" + bb->get_name(); }
string CodeGen::getLabelName(Function *func, int type) {
    const std::vector<std::string> name_list = {"pre", "post"};
    return "." + func->get_name() + "_" + name_list.at(type);
}
string CodeGen::generateFunctionCall(Instruction *inst, const string &call_inst, vector<Value *> ops) {
    using Reg = InstGen::Reg;
    using Addr = InstGen::Addr;
    // ops[0] is the function
    std::string asm_code;
    int args = ops.size() - 1;
    int sp_delta = 0;
    auto t0 = InstGen::Reg(5);

    if (args > 8) {
        args -= 8;
        sp_delta = args * 4;
        asm_code += fmt::format("  addi sp, sp, {}\n", -sp_delta);
        for (int i = 0; i < args; i++) {
            auto arg = ops[i+9];
            asm_code += vregToReg(arg, t0);
            asm_code += fmt::format("  sw t0, {}(sp)\n", i * 4);
        }
    }

    // TODO: optimize
    for (int i = 0; i < std::min(8, args); i++) {
        auto rs = getReg(ops[i+1]->get_name());
        asm_code += vregToReg(ops[i+1], rs);
        asm_code += regToStack(rs, Addr(Reg("sp"), -(i+1)*4));
    }
    for (int i = 0; i < std::min(8, args); i++) {
        asm_code += stackToReg(Addr(Reg("sp"), -(i+1)*4), Reg(10+i));
    }

    asm_code += call_inst;
    
    if (sp_delta != 0) {
        asm_code += fmt::format("  addi sp, sp, {}\n", +sp_delta);
    }
    return asm_code;
}
string CodeGen::generateGlobalVarsCode() {
    GOT.clear();
    string asm_code;
    for (auto &global_var : this->module->get_global_variable()) {
        if (global_var->init_val_ == nullptr) continue;
        GOT[global_var->get_name()] = 1;
        asm_code += fmt::format(".globl {}\n", global_var->get_name());
        asm_code += ".p2align 2\n";
        asm_code += global_var->get_name() + ":\n";
        if (reinterpret_cast<Type *>(!global_var->get_type()->get_ptr_element_type()) ==
            global_var->get_operands().at(0)->get_type()) {
            asm_code += "  .zero " + std::to_string(global_var->get_type()->get_size()) + "\n";
        } else {
            asm_code +=
                CodeGen::generateInitializerCode(dynamic_cast<Constant *>(global_var->get_operands().at(0)));
        }
    }
    LOG(INFO) << asm_code;
    return asm_code;
}
string CodeGen::generateInitializerCode(Constant *init) {
    string asm_code;
    if (auto str_init = dynamic_cast<ConstantStr *>(init); str_init) {
        // guarantee the string is the last attribute of any class
        string str = str_init->get_value();
        string str_label = fmt::format(".Lstr.{}", init->get_name());
        asm_code += "  .word " + std::to_string(str_init->get_type()->get_type_id()) + "\n";
        size_t objsize = str.size() / 4 + 5;
        asm_code += "  .word " + std::to_string(objsize) += "\n";
        asm_code += "  .word $str$dispatchTable\n";
        asm_code += "  .word " + std::to_string(str.size()) += "\n";
        asm_code += "  .word " + str_label += "\n";
        asm_code += fmt::format("{}:\n", str_label);
        asm_code += "  .asciz \"";
        for (char c : str) {
            switch (c) {
                case 0: { asm_code += "\\0"; break; }
                case 8: { asm_code += "\\b"; break; }
                case 9: { asm_code += "\\t"; break; }
                case 10: { asm_code += "\\n"; break; }
                case 12: { asm_code += "\\f"; break; }
                case 13: { asm_code += "\\r"; break; }
                case '\"': { asm_code += "\\\""; break; }
                case '\\': { asm_code += "\\\\"; break; }
                default: {
                    if (c < ' ') asm_code += fmt::format("\\{0:03o}", (int)c);
                    else asm_code += c;
                }
            }
        }
        asm_code += "\"\n";
        asm_code += "\n";
    } else if (auto box_int = dynamic_cast<ConstantBoxInt*>(init); box_int) {
        asm_code += "  .word 1\n";
        asm_code += "  .word 4\n";
        asm_code += "  .word $int$dispatchTable\n";
        asm_code += fmt::format("  .word {}\n", box_int->get_value());
    } else if (auto box_bool = dynamic_cast<ConstantBoxBool*>(init); box_bool) {
        asm_code += "  .word 2\n";
        asm_code += "  .word 4\n";
        asm_code += "  .word $bool$dispatchTable\n";
        asm_code += fmt::format("  .word {}\n", (int)box_bool->get_value());
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
RiscVBackEnd::RiscVBackEnd(int vlen, int vlmax) : vlen(vlen), vlmax(64) {
    /** emplace_back vector reg */
    for (int i = 0; i < vlen / word_size / 4; i++)
        reg_name.emplace_back(fmt::format("x{}", i));
}

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
