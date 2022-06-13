#include "BasicBlock.hpp"
#include "Class.hpp"
#include "ClassDefType.hpp"
#include "Constant.hpp"
#include "Function.hpp"
#include "FunctionDefType.hpp"
#include "GlobalVariable.hpp"
#include "Module.hpp"
#include "Type.hpp"
#include "Value.hpp"
#include <cassert>
#include <chocopy_lightir.hpp>
#include <chocopy_parse.hpp>
#include <chocopy_semant.hpp>
#include <fstream>
#include <string>
#include <utility>
#if __cplusplus > 202000L && !defined(__clang__)
#include <ranges>
#endif
#include <regex>

namespace semantic {
class SemanticError;
class SymbolTable;
class DeclarationAnalyzer;
class TypeChecker;
} // namespace semantic
namespace lightir {
Type *VOID_T;
Type *BOOL_T;
Type *INT_T;
Type *ARR_T;
Type *STR_T;
auto OBJ_T = new vector<Type *>();
const std::regex to_replace("\\$(.+?)+\\.");
const std::regex to_replace_prev(".+?\\.");
const std::regex to_replace_post("\\..+?$");
#define CONST(num) ConstantInt::get(num, &*module)
vector<Constant *> new_array;
vector<Value *> init_val;
vector<BasicBlock *> base_layer;
vector<BasicBlock *> for_layer_stack;
vector<string> nonlocal_symbol;
vector<string> iterable_symbol;
bool is_local_global = false;
/** Store the tmp value */
Value *tmp_value;
int tmp_int = 0;
/** Check whether is rval */
bool is_rval = true;
string tmp_string;
/** Check whether is the int variable */
bool use_int = false;
/** Check whether use the conslist to initialize a list */
bool is_conslist = false;
/** If there's no function or class or doubly list or above
 * just skip all the global conslist */
bool is_global_conslist = false;
/** Function that is being built */
Function *curr_func = nullptr;

bool enable_str_for = true;

/** Use the symbol table to generate the type id */
int LightWalker::get_next_type_id() { return next_type_id++; }

int LightWalker::get_const_type_id() { return next_const_id++; }

int LightWalker::get_class_id(const string &name) const { return sym->class_tag_[name]; }

string LightWalker::get_nested_func_name(semantic::FunctionDefType const * const func) {
    const auto parent_sym = func->current_scope->parent;
    const auto grand_parent_sym = parent_sym->parent;
    if (grand_parent_sym) {
        for(auto& x : *grand_parent_sym->tab) {
            auto parent_func = dynamic_cast<semantic::FunctionDefType*>(x.second);
            if (parent_func) {
                if (parent_func->current_scope == parent_sym) {
                    return get_nested_func_name(parent_func) + "." + func->get_name();
                }
            }
            auto parent_class = dynamic_cast<semantic::ClassDefType*>(x.second);
            if (parent_class) {
                if (parent_class->current_scope == parent_sym) {
                    return "$" + parent_class->get_name() + "." + func->get_name();
                }
            }
        }
        assert(0 && "No parent function found");
    } else {
        return func->get_name();
    }
}

Type* LightWalker::semantic_type_to_llvm_type(semantic::SymbolType *type) {
    if (type->is_list_type()) {
        return ArrayType::get(list_class);
    } else if (dynamic_cast<semantic::ClassValueType*>(type)) {
        if (type->get_name() == "int") {
            return IntegerType::get(32, module.get());
        } else if (type->get_name() == "bool") {
            return IntegerType::get(1, module.get());
        } else if (type->get_name() == "str") {
            return ptr_vstr_type;
        } else if (type->get_name() == "<None>") {
            // ! FIXME: actually I don't know what I should do here.
            return ArrayType::get(object_class);
        } else {
            const auto class_ = dynamic_cast<Class*>(scope.find_in_global(type->get_name()));
            assert(class_);
            return ArrayType::get(class_);
        }
    } else if (type->is_func_type()) {
        // only support global function
        const auto func_def_type = dynamic_cast<semantic::FunctionDefType*>(type);
        std::vector<Type*> arg_types;
        for(auto param : *func_def_type->params) {
            arg_types.emplace_back(semantic_type_to_llvm_type(param));
        }
        auto func_return_type = semantic_type_to_llvm_type(func_def_type->return_type);
        auto func_type = FunctionType::get(func_return_type, arg_types);
        return func_type;
    }
    assert(0);
}

LightWalker::LightWalker(semantic::SymbolTable *sym) : sym(sym) {
    module = std::make_unique<Module>("ChocoPy code");
    builder = new IRBuilder(nullptr, module.get());

    auto TyVoid = Type::get_void_type(&*module);
    auto TyI32 = Type::get_int32_type(&*module);
    auto TyI1 = Type::get_int1_type(&*module);
    auto TyString = Type::get_str_type(&*module);
    auto I8 = new IntegerType(8, &*module);
    auto TyArrI32 = Type::get_array_type(new IntegerType(32, &*module));
    auto TyArrI1 = Type::get_array_type(new IntegerType(1, &*module));
    auto TyArrStr = Type::get_array_type(new StringType("", &*module));

    /** Get the class ready. */
    std::vector<Type *> object_params;

    object_class = new Class(&*module, "object", get_next_type_id(), nullptr, true, true);
    auto TyObject = object_class->get_type();
    auto TyPtrObject = ArrayType::get(object_class);
    object_params.emplace_back(TyPtrObject);
    auto object_init = new Function(FunctionType::get(TyPtrObject, object_params), "$object.__init__", &*module);
    object_class->add_method(object_init);
    int_class = new Class(&*module, "int", get_next_type_id(), nullptr, true, true);
    auto TyIntClass = int_class->get_type();
    int_class->add_method(object_init);
    int_class->add_attribute(new AttrInfo(TyI32, "__int__"));
    bool_class = new Class(&*module, "bool", get_next_type_id(), nullptr, true, true);
    auto TyBoolClass = bool_class->get_type();
    bool_class->add_method(object_init);
    bool_class->add_attribute(new AttrInfo(TyI1, "__bool__"));
    str_class = new Class(&*module, "str", get_next_type_id(), nullptr, true, true);
    auto TyStrClass = str_class->get_type();
    str_class->add_method(object_init);
    str_class->add_attribute(new AttrInfo(TyI32, "__len__", 0));
    str_class->add_attribute(new AttrInfo(TyString, "__str__"));

    Class* virtual_str = new Class(module.get(), 
                                   "str", 
                                   get_next_type_id(), 
                                   object_class, 
                                   true, 
                                   false, 
                                   false);
    virtual_str->add_method(object_init);
    virtual_str->add_attribute(new AttrInfo(TyI32, "__len__", 0));
    virtual_str->add_attribute(new AttrInfo(TyString, "__str__"));

    vector<Type *> union_vec;
    auto TyPtrInt = new ArrayType(TyIntClass);
    auto TyPtrBool = new ArrayType(TyBoolClass);
    auto TyPtrStr = new ArrayType(TyStrClass);
    union_vec.emplace_back(TyPtrInt);
    union_vec.emplace_back(TyPtrBool);
    union_vec.emplace_back(TyPtrStr);
    union_vec.emplace_back(TyPtrObject);
    auto union_list = new Union(union_vec, "type");
    list_class = new Class(&*module, ".list", -1, nullptr, false);
    list_class->add_method(object_init);
    list_class->add_attribute(new AttrInfo(ArrayType::get(union_list), "", union_list));
    list_class->add_attribute(new AttrInfo(TyI32, "__len__", 0));

    auto TyListClass = list_class->get_type();
    auto TyPtrList = new ArrayType(TyListClass);

    /** Predefined functions. */
    std::vector<Type *> heat_init_params;
    auto heat_init_type = FunctionType::get(TyVoid, heat_init_params);
    auto heat_init_fun = Function::create(heat_init_type, "heap.init", module.get());

    std::vector<Type *> initchars_params;
    initchars_params.emplace_back(I8);
    auto initchars_type = FunctionType::get(TyPtrStr, initchars_params);
    auto initchars_fun = Function::create(initchars_type, "initchars", module.get());

    std::vector<Type *> noconv_params;
    auto noconv_type = FunctionType::get(TyPtrInt, noconv_params);
    auto noconv_fun = Function::create(noconv_type, "noconv", module.get());

    std::vector<Type *> nonlist_params;
    auto nonlist_type = FunctionType::get(TyPtrList, nonlist_params);
    auto nonlist_fun = Function::create(nonlist_type, "nonlist", module.get());

    std::vector<Type *> error_oob_params;
    auto error_oob_type = FunctionType::get(TyVoid, error_oob_params);
    auto error_oob_fun = Function::create(error_oob_type, "error.OOB", module.get());
    std::vector<Type *> error_none_params;
    auto error_none_type = FunctionType::get(TyVoid, error_none_params);
    auto error_none_fun = Function::create(error_none_type, "error.None", module.get());
    std::vector<Type *> error_div_params;
    auto error_div_type = FunctionType::get(TyVoid, error_div_params);
    auto error_div_fun = Function::create(error_none_type, "error.Div", module.get());

    scope.enter();
    scope.push(union_list->get_name(), union_list);
    scope.push("object.__init__", object_init);
    scope.push("heap.init", heat_init_fun);
    scope.push("initchars", initchars_fun);
    scope.push("noconv", noconv_fun);
    scope.push("nonlist", nonlist_fun);

    scope.push("error.OOB", error_oob_fun);
    scope.push("error.None", error_none_fun);
    scope.push("error.Div", error_div_fun);

    scope.push("object", object_class);
    scope.push("int", int_class);
    scope.push("bool", bool_class);
    scope.push("str", str_class);
    scope.push(".list", list_class);

    module->add_union(union_list);

    //scope.push_in_global("str", str_class);
    scope.push_in_global("virtual_str_1145141919810", virtual_str);
    vstr_type = virtual_str;
    i32_type=IntegerType::get(32,module.get());
    i1_type=IntegerType::get(1,module.get());
    ptr_vstr_type = ArrayType::get(vstr_type);
}

/**
 * Analyze PROGRAM, creating Info objects for all symbols.
 * Populate the global symbol table.
 */
void LightWalker::visit(parser::Program &node) {
    VOID_T = Type::get_void_type(module.get());
    BOOL_T = Type::get_int1_type(module.get());
    INT_T = Type::get_int32_type(module.get());
    STR_T = Type::get_str_type(module.get());
    ARR_T = Type::get_array_type(module.get());

    for (const auto &obj_type : sym->class_tag_) {
        if (obj_type.first == "list")
            module->add_class_type(new Class(&*module, ".list", obj_type.second, nullptr));
        else if (obj_type.first == "str" || obj_type.first == "int" || obj_type.first == "bool")
            module->add_class_type(new Class(&*module, obj_type.first, obj_type.second, nullptr));
    }
    for (int i = 0; i < sym->class_tag_.size(); i++) {
        OBJ_T->emplace_back(Type::get_class_type(module.get(), i));
    }

    /** Some function that requires the OBJ_T to define */
    auto TyPtrList = ArrayType::get(list_class->get_type());
    auto TyPtrObj = ArrayType::get(module->get_class().front());
    // FIXME: auto TyPtrObj = LabelType::get("$object$prototype_type",get_module()->get_class().front(), module.get());
    auto TyPtrInt = ArrayType::get(OBJ_T->at(1));
    auto TyPtrBool = ArrayType::get(OBJ_T->at(2));
    auto TyPtrStr = ArrayType::get(OBJ_T->at(3));
    vector<Type *> union_len_vec;
    vector<Type *> union_put_vec;
    vector<Type *> union_conslist_vec;
    union_len_vec.emplace_back(TyPtrList);
    union_len_vec.emplace_back(TyPtrStr);
    union_put_vec.emplace_back(TyPtrInt);
    union_put_vec.emplace_back(TyPtrBool);
    union_put_vec.emplace_back(TyPtrStr);
    union_conslist_vec.emplace_back(INT_T);
    union_conslist_vec.emplace_back(BOOL_T);
    union_conslist_vec.emplace_back(TyPtrStr);
    union_conslist_vec.emplace_back(TyPtrList);
    union_len = new Union(union_len_vec, "len");
    union_put = new Union(union_put_vec, "put");
    union_conslist = new Union(union_conslist_vec, "conslist");
    list_class->add_attribute(new AttrInfo(ArrayType::get(union_conslist), "__list__", 0));
    std::vector<Type *> concat_params;
    std::vector<Type *> conslist_params;
    concat_params.emplace_back(TyPtrList);
    concat_params.emplace_back(TyPtrList);
    conslist_params.emplace_back(INT_T);
    conslist_params.emplace_back(union_conslist);
    auto concat_type = FunctionType::get(TyPtrList, concat_params);
    auto conslist_type = FunctionType::get(TyPtrList, conslist_params, true);
    auto concat_fun = Function::create(concat_type, "concat", module.get());
    auto conslist_fun = Function::create(conslist_type, "conslist", module.get());

    std::vector<Type *> len_params;
    len_params.emplace_back(ArrayType::get(union_len));
    auto len_type = FunctionType::get(INT_T, len_params);
    auto len_fun = Function::create(len_type, "$len", module.get());

    std::vector<Type *> put_params;
    put_params.emplace_back(ArrayType::get(union_put));
    auto put_type = FunctionType::get(VOID_T, put_params);
    auto put_fun = Function::create(put_type, "print", module.get());

    std::vector<Type *> makebool_params;
    std::vector<Type *> makeint_params;
    std::vector<Type *> makestr_params;
    makebool_params.emplace_back(BOOL_T);
    makeint_params.emplace_back(INT_T);
    makestr_params.emplace_back(TyPtrStr);
    auto makebool_type = FunctionType::get(TyPtrBool, makebool_params);
    auto makeint_type = FunctionType::get(TyPtrInt, makeint_params);
    auto makestr_type = FunctionType::get(TyPtrStr, makestr_params);
    auto makebool_fun = Function::create(makebool_type, "makebool", module.get());
    auto makeint_fun = Function::create(makeint_type, "makeint", module.get());
    auto makestr_fun = Function::create(makestr_type, "makestr", module.get());

    auto input_type = FunctionType::get(ptr_vstr_type, {});
    auto input_fun = Function::create(input_type, "$input", module.get());

    auto alloc_type = FunctionType::get(TyPtrObj, {TyPtrObj});
    auto alloc_fun = Function::create(alloc_type, "alloc", module.get());

    std::vector<Type *> strcat_params;
    std::vector<Type *> str_params;
    str_params.emplace_back(TyPtrStr);
    strcat_params.emplace_back(TyPtrStr);
    strcat_params.emplace_back(TyPtrStr);
    std::vector<Type*> vstr_params;
    vstr_params.emplace_back(ptr_vstr_type);
    vstr_params.emplace_back(ptr_vstr_type);
    auto str_type = FunctionType::get(BOOL_T, vstr_params);
    auto strcat_type = FunctionType::get(ptr_vstr_type, vstr_params);
    auto streql_fun = Function::create(str_type, "streql", module.get());
    auto strneql_fun = Function::create(str_type, "strneql", module.get());
    auto strcat_fun = Function::create(strcat_type, "strcat", module.get());

    scope.push("$input", input_fun);
    scope.push("strcat", strcat_fun);
    scope.push("streql", streql_fun);
    scope.push("strneql", strneql_fun);
    scope.push("makebool", makebool_fun);
    scope.push("makeint", makeint_fun);
    scope.push("makestr", makestr_fun);
    scope.push("$len", len_fun);
    scope.push("print", put_fun);
    scope.push("concat", concat_fun);
    scope.push("alloc", alloc_fun);
    scope.push("conslist", conslist_fun);
    module->add_union(union_len);
    module->add_union(union_put);
    module->add_union(union_conslist);

    /** Global variable in the stdlib */
    GlobalVariable *const_;
    auto const_vect = {0, 1};
    for (auto &&num : const_vect) {
        const_ = GlobalVariable::create(fmt::format("const_{}", num), &*this->module, OBJ_T->at(2), false, nullptr);
        scope.push(fmt::format("const_{}", num), const_);
    }
    auto const_vect_1 = {2, 3, 4, 5, 6, 7};
    for (auto &&num : const_vect_1) {
        const_ = GlobalVariable::create(fmt::format("const_{}", num), &*this->module, OBJ_T->at(3), false, nullptr);
        scope.push(fmt::format("const_{}", num), const_);
    }

    /** First set the function to before_main. */
    std::vector<Type *> param_types;
    auto func_type = FunctionType::get(VOID_T, param_types);

    curr_func = Function::create(true, func_type, "before_main", module.get());
    auto beforeBB = BasicBlock::create(&*module, "before_main", curr_func);
    builder->set_insert_point(beforeBB);
#ifdef RV64
    builder->create_asm("lui a0, 8192\\0A\tadd s11, zero, a0");
    vector<Value *> arg_params;
    Value *called = scope.find("heap.init");
    builder->create_call(called, arg_params);
    builder->create_asm("mv s10, gp\\0A\tadd s11, s11, s10\\0A\tmv fp, zero\\0A\tlw ra, 8(sp)\\0A\taddi "
                        "sp, sp, 16\\0A\tret");
#else
    builder->create_asm("lui a0, 8192\\0A\tadd s11, zero, a0");
    vector<Value *> arg_params;
    Value *called = scope.find("heap.init");
    builder->create_call(called, arg_params);
    builder->create_asm("mv s10, gp\\0A\tadd s11, s11, s10\\0A\tmv fp, zero\\0A\tlw ra, 12(sp)\\0A\taddi "
                        "sp, sp, 16\\0A\tret");
#endif
    /** Proceed in phases:
     * 1. Analyze all global variable declarations.
     *    Do this first so that global variables are in the symbol
     *    table before we encounter `global x` declarations.
     * 2. Analyze classes and global functions now that global variables
     *    are in the symbol table.
     */




    curr_func = Function::create(func_type, "main", module.get());
    auto baseBB = BasicBlock::create(&*module, "", curr_func);
    base_layer.emplace_back(baseBB);
    builder->set_insert_point(baseBB);
    scope.push("main", curr_func);

    auto ptr_list_type = ArrayType::get(list_class);
    if (enable_str_for) {
        for(int i=0; i<=126; i++) {
                string s("1");
                s[0]=i;
                int id=get_const_type_id();
                auto t=GlobalVariable::create(
                    "const_"+std::to_string(id),
                    module.get(),
                    ConstantStr::get(s,id,module.get())
                );
                //scope.push_in_global("!char_set_"+std::to_string(i), t);
                auto ptr_type = ArrayType::get(dynamic_cast<Class*>(
                    scope.find_in_global("virtual_str_1145141919810")));
                t->set_type(ptr_type);
                auto p = GlobalVariable::create(
                    "char_list_const"+std::to_string(id),
                    module.get(),
                    ptr_type,
                    false,
                    ConstantNull::get(ptr_type)
                );
                //std::cerr<<p->get_type()->print()<<"\n";
                builder->create_store(t, p);
                p->set_type(ArrayType::get(p->get_type()));
                char_list.push_back(p);
                //scope.push_in_global(node.var->identifier->name,p);
        }
        if (true) {
            Instruction *char_table;
            vector<Value*> conslist_para;
            int tt=126+1;
            conslist_para.push_back(CONST(tt));
            for(int i=0; i<tt; i++) {
                //auto t3=builder.create_
                auto p=char_list[i];
                //p->set_type(ArrayType::get(p->get_type()));
                auto t = builder->create_bitcast(p, ArrayType::get(union_conslist));
                auto t2 = builder->create_load(t);
                conslist_para.push_back(t2);
                //p->set_type(dynamic_cast<ArrayType*>(p->get_type())->get_element_type());
            }
            char_table=builder->create_call(conslist_fun,conslist_para);
            char_table=builder->create_bitcast(char_table, ArrayType::get(ArrayType::get(list_class)));
            char_table = builder->create_load(char_table);

            auto g_char_table=GlobalVariable::create(
                "global_char_table",
                module.get(),
                ptr_list_type,
                false,
                ConstantNull::get(ptr_list_type)
            );
            builder->create_store(char_table, g_char_table);
            scope.push_in_global("global_char_table", g_char_table);
        }
    }

    invalid_value=GlobalVariable::create(
        "invalid_list_pointer",
        module.get(),
        ptr_list_type,
        false,
        ConstantNull::get(ptr_list_type)
    );

    /*auto temp_conslist=GlobalVariable::create(
        "temp_conslist",
        module.get(),
        union_conslist,
        false,
        nullptr
    );*/
    Instruction* temp_conslist=builder->create_alloca(union_conslist);
    scope.push("temp_conslist", temp_conslist);

    null = builder->create_call(scope.find_in_global("noconv"),vector<Value*>());

    for (auto decl : *node.declarations) {
        if (auto node = dynamic_cast<parser::ClassDef*>(decl); node) {
            Class* super_class = nullptr;
            for (const auto e : module->get_class()) {
                if (e->get_name() == node->superClass->name) {
                    super_class = e;
                    break;
                }
            }
            assert(super_class);

            auto class_ = new Class(module.get(), node->name->name, get_next_type_id(), super_class, true, false, true);
            scope.push(node->name->name, class_);
        }
    }

    for (auto decl : *node.declarations) {
        if (auto node = dynamic_cast<parser::FuncDef*>(decl); node) {
            auto& func_name = node->get_id()->name;
            auto func_def_type = sym->declares<semantic::FunctionDefType*>(func_name);
            assert(func_def_type);
            auto unique_func_name = get_nested_func_name(func_def_type);

            auto func_type = dynamic_cast<FunctionType*>(semantic_type_to_llvm_type(func_def_type));
            assert(func_type);

            auto func = Function::create(func_type, unique_func_name, module.get());
            scope.push(func_name, func);
        }
    }

    for (auto decl : *node.declarations) {
        decl->accept(*this);
    }
    for (auto stmt : *node.statements) {
        stmt->accept(*this);
    }
    /** For Optimization Debug */
    for (auto &func : this->module->get_functions()) {
        func->set_instr_name();
    }
    /*auto C = new ConstantInt(new IntegerType(32,module.get()),1919810);
    auto t=builder->create_call(makeint_fun, {C});
    t->set_name("0");
    BitCastInst* t1 = builder->create_bitcast(t,ArrayType::get(union_put));
    t1->set_name("1");
    builder->create_call(put_fun, {t1});*/

    builder->create_asm("li a0, 0 \\0A"
        "li a7, 93 #exit system call\\0A"
                        "ecall");
    builder->create_void_ret();
}

/* The Light IR Your Code Here */

Value *LightWalker::get_conslist(vector<Value *> &object_args, Value *called_initial_object) {
    is_conslist = dynamic_cast<ArrayType *>(tmp_value->get_type()) &&
                  dynamic_cast<Class *>(dynamic_cast<ArrayType *>(tmp_value->get_type())->get_element_type());

    if (!(is_conslist && is_global_conslist)) {
        if (dynamic_cast<ConstantArray *>(tmp_value)) {
            auto array_ = dynamic_cast<ConstantArray *>(tmp_value)->const_array;
            Value *first_num = CONST(int(array_.size()));
            object_args.emplace_back(first_num);
            for (auto item : array_) {
                object_args.emplace_back(item);
            }
        } else {
            auto array_ =
                dynamic_cast<ConstantArray *>(dynamic_cast<GlobalVariable *>(tmp_value)->init_val_)->const_array;
            Value *first_num = CONST(int(array_.size()));
            object_args.emplace_back(first_num);
            Value *array_item;
            for (int i = 0; i < array_.size(); i++) {
                array_item = builder->create_gep(tmp_value, CONST(i));
                array_item = builder->create_bitcast(array_item, ArrayType::get(union_conslist));
                array_item = builder->create_load(array_item);
                object_args.emplace_back(array_item);
            }
        }
        return builder->create_call(called_initial_object, object_args);

    } else {
        return builder->create_call(called_initial_object, {tmp_value}); // here tmp_value is the list literal
    }
}

void LightWalker::transfer_conslist(const string &name) {
    auto val_ = scope.find(name);
    /** Check whether the list is empty, if empty : z:[int]=None (not initialized)*/
    auto renamed_ = new List(list_class, {nullptr}, name);
    auto type_ = dynamic_cast<ConstantArray *>(val_)->get_type();
    if (type_->is_array_type()) {
    }
    scope.remove(name);
    scope.push(name, renamed_);
}

void LightWalker::visit(parser::AssignStmt &node) {
    node.value->accept(*this);
    auto v = visitor_return_value;
    for(auto &i: *node.targets) {
        get_lvalue=true;
        i->accept(*this);
        get_lvalue=false;
        auto address = visitor_return_value;
        if (node.value->inferredType->get_name()=="int") {
            if (i->inferredType->get_name()=="int") {
                builder->create_store(v,address);
            } else {
                //TODO: object assignment
            }
        } else if (node.value->inferredType->get_name()=="bool") {
            if (i->inferredType->get_name()=="bool") {
                builder->create_store(v,address);
            } else {
                //TODO: object assignment
            }
        } else if (node.value->inferredType->get_name()=="str") {
            if (i->inferredType->get_name()=="str") {
                builder->create_store(v, address);
            } else {
                //TODO: object assignment
            }  
        } else {
            // class assignment
            auto ty = (dynamic_cast<GlobalVariable*>(address))? address->get_type():address->get_type()->get_ptr_element_type();
            auto t = builder->create_bitcast(v,ty);
            builder->create_store(t,address);
        }
    }
}
void LightWalker::visit(parser::PassStmt &) {}
void LightWalker::visit(parser::BinaryExpr &node)
{
    node.left->accept(*this);
    auto v1 = this->visitor_return_value;
    Instruction* res;
    if (node.operator_=="and" || node.operator_=="or") { //short circuit
        auto b = builder->get_insert_block();
        auto b_run = BasicBlock::create(module.get(), "", b->get_parent());   
        auto b_norun = BasicBlock::create(module.get(), "", b->get_parent());   
        auto b_end = BasicBlock::create(module.get(), "", b->get_parent());   
        if (node.operator_=="and") {
            builder->create_cond_br(v1, b_run, b_norun);
        } else {
            builder->create_cond_br(v1, b_norun, b_run);
        }
        builder->set_insert_point(b_run);
        node.right->accept(*this);
        auto v2 = this->visitor_return_value;
        auto b_run_end=builder->get_insert_block();
        Instruction* run_res=nullptr;
        if (node.operator_=="and") {
            run_res = builder->create_iand(v1, v2);
            run_res->set_type(i1_type);
        } else {
            run_res = builder->create_ior(v1, v2);
            run_res->set_type(i1_type);
        }
        builder->create_br(b_end);
        builder->set_insert_point(b_norun);
        Value* norun_res=nullptr;
        if (node.operator_=="and") {
            norun_res = CONST(false);
        } else {
            norun_res = CONST(true);
        }
        builder->create_br(b_end);
        builder->set_insert_point(b_end);
        auto phi=builder->create_phi(i1_type);
        phi->set_lval(run_res);
        phi->add_phi_pair_operand(run_res, b_run_end);
        phi->add_phi_pair_operand(norun_res, b_norun);
        b_end->add_instr_begin(phi);
        res = phi;
    }  else {
        node.right->accept(*this);
        auto v2 = this->visitor_return_value;
        if (node.operator_=="+") {
            if (node.left->inferredType->get_name()=="int") {
                res = builder->create_iadd(v1, v2);
            } else if (node.left->inferredType->get_name()=="str") {
                auto strcat_func=scope.find_in_global("strcat");
                vector<Value*> params;
                params.push_back(v1);
                params.push_back(v2);
                res = builder->create_call(strcat_func,params);
                res->set_type(ptr_vstr_type);
            } else {
                auto concat_func = scope.find_in_global("concat");
                vector<Value*> params;
                params.push_back(v1);
                params.push_back(v2);
                res = builder->create_call(concat_func,params);
            }
        } else if (node.operator_=="-") {
            res = builder->create_isub(v1, v2);
        } else if (node.operator_=="*") {
            res = builder->create_imul(v1, v2);
        } else if (node.operator_=="//") {
            auto div_0_fun = scope.find_in_global("error.Div");
            auto t1 = builder->create_icmp_eq(v2, CONST(0));
            auto b = builder->get_insert_block();
            auto b1 = BasicBlock::create(module.get(),"",b->get_parent());
            auto b2 = BasicBlock::create(module.get(),"",b->get_parent());
            /*b->add_succ_basic_block(b1);
            b->add_succ_basic_block(b2);
            b1->add_pre_basic_block(b);
            b2->add_pre_basic_block(b);
            // To make LLVM happy, b1 will jump to b2. However this won't happen. So b1 -> b2 is not nessary.*/
            auto t2 = builder->create_cond_br(t1, b1,b2);
            builder->set_insert_point(b1);
            builder->create_call(div_0_fun, {});
            builder->create_br(b2);
            builder->set_insert_point(b2);
            auto t3=builder->create_isdiv(v1, v2);
            res = t3;
        } else if (node.operator_=="%") {
            auto div_0_fun = scope.find_in_global("error.Div");
            auto t1 = builder->create_icmp_eq(v2, CONST(0));
            auto b = builder->get_insert_block();
            auto b1 = BasicBlock::create(module.get(),"",b->get_parent());
            auto b2 = BasicBlock::create(module.get(),"",b->get_parent());
            /*b->add_succ_basic_block(b1);
            b->add_succ_basic_block(b2);
            b1->add_pre_basic_block(b);
            b2->add_pre_basic_block(b);
            // To make LLVM happy, b1 will jump to b2. However this won't happen. So b1 -> b2 is not nessary.*/
            auto t2 = builder->create_cond_br(t1, b1,b2);
            builder->set_insert_point(b1);
            builder->create_call(div_0_fun, {});
            builder->create_br(b2);
            builder->set_insert_point(b2);
            auto t3=builder->create_irem(v1, v2);
            res = t3;
        } else if (node.operator_=="<") {
            res = builder->create_icmp_lt(v1, v2);
        } else if (node.operator_=="<=") {
            res = builder->create_icmp_le(v1, v2);
        } else if (node.operator_=="==") {
            if (node.left->inferredType->get_name()=="bool") {
                auto v1_32 = builder->create_zext(v1, IntegerType::get(32, module.get()));
                auto v2_32 = builder->create_zext(v2, IntegerType::get(32, module.get()));
                res = builder->create_icmp_eq(v1_32, v2_32);
            } else if (node.left->inferredType->get_name()=="int") {
                res = builder->create_icmp_eq(v1, v2);
            } else {
                auto strcmp_func=scope.find_in_global("streql");
                vector<Value*> params;
                params.push_back(v1);
                params.push_back(v2);
                res = builder->create_call(strcmp_func,params);
            }
        } else if (node.operator_=="!=") {
            if (node.left->inferredType->get_name()=="bool") {
                auto v1_32 = builder->create_zext(v1, IntegerType::get(32, module.get()));
                auto v2_32 = builder->create_zext(v2, IntegerType::get(32, module.get()));
                res = builder->create_icmp_ne(v1_32, v2_32);
            } else if (node.left->inferredType->get_name()=="int") {
                res = builder->create_icmp_ne(v1, v2);
            } else {
                auto strcmp_func=scope.find_in_global("strneql");
                vector<Value*> params;
                params.push_back(v1);
                params.push_back(v2);
                res = builder->create_call(strcmp_func,params);
            }
        } else if (node.operator_==">") {
            res = builder->create_icmp_gt(v1, v2);
        } else if (node.operator_==">=") {
            res = builder->create_icmp_ge(v1, v2);
        } else if (node.operator_=="is"){
            get_lvalue = false; node.left->accept(*this);
            auto l = visitor_return_value;
            get_lvalue = false; node.right->accept(*this);
            auto r = visitor_return_value;
            res = builder->create_icmp_eq(l, r);
        } else {
            assert(false);
        }
    }
    visitor_return_value = res;
}
void LightWalker::visit(parser::BoolLiteral &node)
{
    auto C = CONST(node.bin_value);
    visitor_return_value = C;
}
void LightWalker::visit(parser::CallExpr &node)
{
    const auto& func_name = node.function->name;
    if (func_name=="print") { //FIXME: problem here.
        node.args->at(0)->accept(*this);
        auto v1 = this->visitor_return_value;
        auto put_fun = scope.find_in_global("print");
        if (node.args->at(0)->inferredType->get_name()=="int") {
            auto makeint = scope.find_in_global("makeint");
            auto t=builder->create_call(makeint, {v1});
            BitCastInst* t1 = builder->create_bitcast(t,ArrayType::get(union_put));
            builder->create_call(put_fun, {t1});
        } else if (node.args->at(0)->inferredType->get_name()=="bool") {
            auto makebool = scope.find_in_global("makebool");
            v1->set_type(IntegerType::get(1,module.get()));
            auto t1 = builder->create_call(makebool, {v1});
            auto t2 = builder->create_bitcast(t1, ArrayType::get(union_put));
            builder->create_call(put_fun, {t2});
        } else if (node.args->at(0)->inferredType->get_name()=="str") {
            auto t2 = builder->create_bitcast(v1, ArrayType::get(union_put));
            builder->create_call(put_fun, {t2});
        } else {
            int id=get_const_type_id();
            auto C=ConstantStr::get("Invalid argument", id, module.get());
            auto t=GlobalVariable::create("const_"+std::to_string(id),module.get(),C);
            auto t2 = builder->create_bitcast(t, ArrayType::get(union_put));
            builder->create_call(put_fun, {t2});
        }
    } else if (func_name=="input") {
        auto input_fun=scope.find_in_global("$input");
        visitor_return_value = builder->create_call(input_fun,vector<Value*>());

    } else if (func_name=="len") { 
        auto len_func = scope.find_in_global("$len");
        node.args->at(0)->accept(*this);
        auto v1 = this->visitor_return_value;
        auto arg_type=node.args->at(0)->inferredType;
        if (dynamic_cast<semantic::ListValueType*>(arg_type)==nullptr && arg_type->get_name()!="str") {
            v1=builder->create_load(invalid_value);
        }
        auto t1 = builder->create_bitcast(v1, ArrayType::get(union_len));
        auto v_len = builder->create_call(len_func,{t1});
        visitor_return_value=v_len;
    } else if (func_name == "int") {
        visitor_return_value = CONST(0);
    } else if (func_name == "bool") {
        visitor_return_value = ConstantInt::get(false, module.get());
    } else {
        Function* func;
        std::vector<Value*> args;
        auto is_object_init = sym->get<semantic::ClassDefType*>(func_name);
        if (is_object_init) {
            auto class_type = dynamic_cast<Class*>(scope.find_in_global(func_name));
            assert(class_type);
            auto pointer_class_type = ArrayType::get(class_type);

            auto alloc_fun = dynamic_cast<Function*>(scope.find_in_global("alloc"));
            assert(alloc_fun);
            auto alloc_type = alloc_fun->get_args().front()->type_;
            
            auto op = builder->create_bitcast(class_type, alloc_type);
            auto alloc_call = builder->create_call(alloc_fun, {op});

            func = dynamic_cast<Function*>(class_type->get_method()->at(0));
            assert(func);            
            visitor_return_value = builder->create_bitcast(alloc_call, pointer_class_type);
            args.emplace_back(builder->create_bitcast(alloc_call, func->get_function_type()->get_arg_type(0)));
        } else {
            func = dynamic_cast<Function*>(scope.find_in_global(func_name));
            if (func == nullptr) {
                // lambda function
                func = dynamic_cast<Function*>(scope.find(func_name));
                auto class_instance = scope.find(func_name + "$anon");
                assert(func); assert(class_instance);
                args.emplace_back(class_instance);
            }
        }
        for (auto arg : *node.args) {
            visitor_return_value = nullptr;
            arg->accept(*this);
            assert(visitor_return_value);
            args.push_back(this->visitor_return_value);
        }
        auto v = builder->create_call(func, std::move(args));
        if (!is_object_init) visitor_return_value = v;
    }
}
void LightWalker::visit(parser::ClassDef &node) {
    const auto class_type = sym->get<semantic::ClassDefType*>(node.name->name);
    const auto super_class_type = sym->get<semantic::ClassDefType*>(node.superClass->name);
    assert(super_class_type);
    auto saved_sym = sym;
    sym = class_type->current_scope;

    auto class_ = dynamic_cast<Class*>(scope.find_in_global(node.name->name));
    assert(class_);
    auto super_class = class_->super_class_info_;

    for (const auto attr : *super_class->get_attribute()) {
        class_->add_attribute(attr);
    }
    for (const auto method: *super_class->get_method()) {
        class_->add_method(method);
    }
    for (const auto decl : *node.declaration) {
        const auto& name = decl->get_id()->name;
        if (const auto var_def = dynamic_cast<parser::VarDef*>(decl); var_def) {
            if (!super_class_type->current_scope->declares(name)) {
                var_def->var->type->accept(*this);
                assert(visitor_return_type);
                const auto attr_type = visitor_return_type;

                if (attr_type->is_integer_type()) {
                    class_->add_attribute(new AttrInfo(attr_type, name, static_cast<parser::IntegerLiteral*>(var_def->value)->value));
                } else if (attr_type->is_bool_type()) {
                    class_->add_attribute(new AttrInfo(attr_type, name, static_cast<parser::BoolLiteral*>(var_def->value)->bin_value));
                } else if (attr_type == ptr_vstr_type) {
                    auto str_literal = dynamic_cast<parser::StringLiteral*>(var_def->value);
                    assert(str_literal);

                    int id = get_const_type_id();
                    auto C = ConstantStr::get(str_literal->value, id, module.get());
                    auto t = GlobalVariable::create("const_" + std::to_string(id),module.get(),C);
                    t->set_type(ptr_vstr_type);

                    class_->add_attribute(new AttrInfo(attr_type, name, t));
                } else {
                    class_->add_attribute(new AttrInfo(attr_type, name));
                }
            }
        }
    }
    for (const auto decl : *node.declaration) {
        if (const auto func_def = dynamic_cast<parser::FuncDef*>(decl); func_def) {
            auto& func_name = func_def->get_id()->name;
            auto func_def_type = sym->declares<semantic::FunctionDefType*>(func_name);
            assert(func_def_type);
            auto unique_func_name = get_nested_func_name(func_def_type);

            auto func_type = dynamic_cast<FunctionType*>(semantic_type_to_llvm_type(func_def_type));
            assert(func_type);

            auto func = Function::create(func_type, unique_func_name, module.get());
            scope.push(unique_func_name, func);
            class_->add_method(func);
        }
    }
    for (const auto decl : *node.declaration) {
        if (const auto func_def = dynamic_cast<parser::FuncDef*>(decl); func_def) {
            func_def->accept(*this);
        }
    }
    sym = saved_sym;
}
void LightWalker::visit(parser::ClassType &node) {
    if (node.className == "<None>") {
        // ! FIXME: actually I don't know what I should do here.
        visitor_return_type = ArrayType::get(object_class);
        return; 
    }
    const auto class_ = dynamic_cast<Class*>(scope.find_in_global(node.className));
    assert(class_);
    if (node.get_name() == "int") {
        visitor_return_type = IntegerType::get(32, module.get());
    } else if (node.get_name() == "bool") {
        visitor_return_type = IntegerType::get(1, module.get());
    } else if (node.get_name() == "str") {
        visitor_return_type = ptr_vstr_type;
    } else {
        visitor_return_type = ArrayType::get(class_);
    }
}
void LightWalker::visit(parser::ExprStmt &node) 
{
    node.expr->accept(*this);
}
void LightWalker::visit(parser::ForStmt &node) 
{
    Instruction* char_table=nullptr;
    if(node.iterable->inferredType->get_name()=="str") {
        auto g_char_table=scope.find_in_global("global_char_table");
        char_table = builder->create_load(g_char_table);
    }
    node.iterable->accept(*this);
    auto list = visitor_return_value;

    auto noconv_fun = scope.find_in_global("noconv");
    auto null= builder->create_call(noconv_fun,vector<Value*>());
    auto cond_null=builder->create_icmp_eq(list, null);
    auto b_before_null = builder->get_insert_block();
    auto b_null_true= BasicBlock::create(module.get(), "", b_before_null->get_parent());
    auto b_null_false = BasicBlock::create(module.get(), "", b_before_null->get_parent());
    builder->create_cond_br(cond_null, b_null_true, b_null_false);
    builder->set_insert_point(b_null_true);
    auto none_func=scope.find_in_global("error.None");
    builder->create_call(none_func,vector<Value*>());
    builder->create_br(b_null_false);
    builder->set_insert_point(b_null_false);

    auto it = scope.find(node.identifier->name);
    //auto i = builder->create_alloca(IntegerType::get(32, module.get()));
    auto i_begin = CONST(0);
    //builder->create_store(CONST(0), i);
    auto len_func = scope.find_in_global("$len");
    auto t1 = builder->create_bitcast(list, ArrayType::get(union_len));
    auto v_len = builder->create_call(len_func,{t1});
    auto b= builder->get_insert_block();
    auto b_cond = BasicBlock::create(module.get(), "", b->get_parent());
    auto b_body = BasicBlock::create(module.get(), "", b->get_parent());
    auto b_end = BasicBlock::create(module.get(), "", b->get_parent());
    builder->create_br(b_cond);
    builder->set_insert_point(b_cond);
    //auto ii =builder->create_load(i);
    auto i = builder->create_phi(i32_type);
    i->set_lval(i_begin);
    i->add_phi_pair_operand(i_begin, b);
    b_cond->add_instr_begin(i);
    auto cond=builder->create_icmp_lt(i, v_len);
    builder->create_cond_br(cond, b_body, b_end);
    builder->set_insert_point(b_body);
    if (node.iterable->inferredType->get_name()=="str") {
        auto p_str = builder->create_gep(list, CONST(4));
        auto str = builder->create_load(p_str);
        str->set_type(ArrayType::get(IntegerType::get(8, module.get())));
        auto p_char = builder->create_gep(str, i);
        auto i_char = builder->create_load(p_char);
        auto i32_char = builder->create_zext(i_char, IntegerType::get(32,module.get()));
        auto idx = builder->create_isub(i32_char, CONST(0));

        auto t2 = builder->create_gep(char_table, CONST(4));
        t2->set_type(ArrayType::get(ArrayType::get(this->union_conslist)));
        auto t3=builder->create_load(t2);
        auto t5 = builder->create_gep(t3, idx);
        auto t6 = builder->create_bitcast(t5, ArrayType::get(ArrayType::get(dynamic_cast<Class*>(scope.find_in_global("virtual_str_1145141919810")))));
        auto t7 = builder->create_load(t6);
        builder->create_store(t7, it);

        for(auto s:*node.body) {
            s->accept(*this);
        }
    } else {
        auto list_type=dynamic_cast<semantic::ListValueType*>(node.iterable->inferredType);
        if (list_type!=nullptr) {
            auto element_type=list_type->element_type;
            auto p_actual_list = builder->create_gep(list, CONST(4));
            p_actual_list->set_type(ArrayType::get(ArrayType::get(this->union_conslist)));
            auto actual_list = builder->create_load(p_actual_list);
            auto p_element = builder->create_gep(actual_list, i);
            Type* type=nullptr;
            if (element_type->get_name()=="int") {
                type=i32_type;
            } else if (element_type->get_name()=="bool") {
                type=i1_type;
            } else if (element_type->get_name()=="str") {
                type = ArrayType::get(vstr_type);
            } else {
                if (element_type->is_list_type()) {
                    type=ArrayType::get(list_class);
                } else {
                    type=dynamic_cast<Type*>(scope.find_in_global(element_type->get_name()));
                    assert(type!=nullptr);
                    type=ArrayType::get(type);
                }
            }
            auto t1 = builder->create_bitcast(p_element, ArrayType::get(type));
            auto t2= builder->create_load(t1);
            builder->create_store(t2, it);
            for(auto s:*node.body) {
                s->accept(*this);
            }
        }
    }
    auto i_next =builder->create_iadd(i,CONST(1));
    i->add_phi_pair_operand(i_next, builder->get_insert_block());
    //builder->create_store(ni, i);
    builder->create_br(b_cond);
    builder->set_insert_point(b_end);
}
void LightWalker::visit(parser::FuncDef &node) {
    auto& func_name = node.get_id()->name;
    auto func_def_type = sym->declares<semantic::FunctionDefType*>(func_name);
    assert(func_def_type);
    auto unique_func_name = get_nested_func_name(func_def_type);
    // std::cerr << "enter " << unique_func_name << std::endl;

    Function* func;
    Class* anon = nullptr;
    if (!scope.in_global()) {
        func = dynamic_cast<Function*>(scope.find(func_name));
        anon = dynamic_cast<Class*>(func->get_function_type()->get_arg_type(0)->get_ptr_element_type());
        assert(anon);
        assert(func);
    } else {
        auto is_method = unique_func_name.find("$") != std::string::npos;
        func = dynamic_cast<Function*>(scope.find_in_global(is_method ? unique_func_name : func_name));
        assert(func);
    }
    auto& arg_types = func->get_function_type()->args_;

    auto saved_b = builder->get_insert_block();
    auto b = BasicBlock::create(module.get(), "", func);
    builder->set_insert_point(b);
    scope.enter();
    auto saved_sym = sym;
    sym = func_def_type->current_scope;
    for (auto decl : *node.declarations) {
        if (auto node = dynamic_cast<parser::FuncDef*>(decl); node) {
            // lambda function
            auto& func_name = node->get_id()->name;
            auto func_def_type = sym->declares<semantic::FunctionDefType*>(func_name);
            assert(func_def_type);
            auto unique_func_name = get_nested_func_name(func_def_type);

            auto func_type = dynamic_cast<FunctionType*>(semantic_type_to_llvm_type(func_def_type));
            assert(func_type);
            auto anon = new Class(module.get(), unique_func_name, true);
            func_type->args_.insert(func_type->args_.begin(), ArrayType::get(anon));

            auto func = Function::create(func_type, unique_func_name, module.get());
            scope.push(func_name, func);

            auto class_instance = builder->create_alloca(anon);
            scope.push(func_name + "$anon", class_instance);
        }
    }

    if (anon) {
        builder->set_insert_point(saved_b);
        auto arg = new Value(arg_types[0], "arg0");
        auto class_instance = scope.find(func_name + "$anon");
        for(int i = 0; i < node.lambda_params->size(); i++) {
            auto& capture_name = node.lambda_params->at(i);
            auto capture_value = scope.find(capture_name);
            assert(capture_value);

            auto capture_type = capture_value->get_type();
            if (dynamic_cast<Function*>(capture_value)) {
                auto capture_func = (Function*)capture_value;
                auto capture_value = scope.find(capture_name + "$anon");
                assert(capture_value);
                capture_type = capture_value->get_type();
            }
            assert(capture_type);

            // std::cerr << unique_func_name << " add attr " << capture_name << " of type " << capture_type->print() << ' ' << std::endl;
            anon->add_attribute(new AttrInfo(capture_type, capture_name, ConstantNull::get(capture_type)));
            
            auto addr = builder->create_gep(class_instance, CONST(i));
            auto v = scope.find(capture_name);
            if (dynamic_cast<Function*>(v)) {
                v = scope.find(capture_name + "$anon");
            }
            builder->create_store(v, addr);
        }

        builder->set_insert_point(b);
        for(int i = 0; i < node.lambda_params->size(); i++) {
            auto& capture_name = node.lambda_params->at(i);
            auto t = builder->create_gep(arg, CONST(i));
            auto v = builder->create_load(t);
            if (v->get_type()->get_ptr_element_type()->is_class_anon()) {
                scope.push(capture_name + "$anon", v);
            } else {
                scope.push(capture_name, v);
            }
        }
    }

    for (int i = 0, arg_num = anon ? 1 : 0; i < node.params->size(); i++, arg_num++) {
        auto arg_type = arg_types[arg_num];
        auto arg = node.params->at(i);
        auto alloca = builder->create_alloca(arg_type);
        builder->create_store(new Value(arg_type, "arg" + std::to_string(arg_num)),alloca);
        scope.push(arg->identifier->name, alloca);
    }
    Instruction* temp_conslist = builder->create_alloca(union_conslist);
    scope.push("temp_conslist", temp_conslist);

    for (auto decl : *node.declarations) {
        decl->accept(*this);
    }
    for (auto stmt : *node.statements) {
        stmt->accept(*this);
    }

    if (node.returnType->get_name() != "int" && node.returnType->get_name() != "bool" && node.returnType->get_name() != "str") {
        builder->create_ret(new ConstantNull(func->get_function_type()->get_return_type()));
    }
    builder->set_insert_point(saved_b);
    scope.exit();
    sym = saved_sym;
}
void LightWalker::visit(parser::GlobalDecl &) { }
void LightWalker::visit(parser::Ident &node) 
{
    //TODO: LightWalker for Ident
    //NOTE: List Or Class is not implemented.

    // Local var
    auto v = scope.find(node.name);
    if (v) {
        if (get_lvalue) {
            visitor_return_value = v;
        } else {
            auto t = builder->create_load(v);
            visitor_return_value = t;
        }
        return;
    }

    // Global var
    if (get_lvalue) {
        auto v = scope.find_in_global(node.name);
        visitor_return_value = v;
    } else {
        auto v = scope.find_in_global(node.name);
        auto t = builder->create_load(v);
        visitor_return_value = t;        
    }
}
void LightWalker::visit(parser::IfExpr &node) 
{
    node.condition->accept(*this);
    auto cond = visitor_return_value;
    Instruction* res=nullptr;
    auto b = builder->get_insert_block();
    auto b_true = BasicBlock::create(module.get(),"",b->get_parent());
    auto b_false = BasicBlock::create(module.get(),"",b->get_parent());
    auto b_end = BasicBlock::create(module.get(),"",b->get_parent());
    builder->create_cond_br(cond, b_true, b_false);
    builder->set_insert_point(b_true);
    node.thenExpr->accept(*this);
    auto b_true_end = builder->get_insert_block();
    auto v_true =  visitor_return_value;
    //TODO: 此处可能产生一个到LCA的类型转换
    builder->create_br(b_end);
    builder->set_insert_point(b_false);
    node.elseExpr->accept(*this);
    auto b_false_end = builder->get_insert_block();
    auto v_false = visitor_return_value;
    builder->create_br(b_end);
    builder->set_insert_point(b_end);
    auto phi=builder->create_phi(v_true->get_type());
    phi->set_lval(v_true);
    phi->add_phi_pair_operand(v_true, b_true_end);
    phi->add_phi_pair_operand(v_false, b_false_end);
    b_end->add_instr_begin(phi);
    visitor_return_value=phi;
}
void LightWalker::visit(parser::IntegerLiteral &node)
{
    auto C = CONST(node.value);
    visitor_return_value = C;
}
void LightWalker::visit(parser::ListExpr &node) 
{
    auto type=dynamic_cast<semantic::ListValueType*>(node.inferredType);
    vector<Value*> para;
    if (type==nullptr) {
        para.push_back(CONST(0));
        //Instruction* temp_conslist=builder->create_alloca(union_conslist);
        auto temp_conslist= scope.find("temp_conslist");
        Instruction* t=builder->create_bitcast(temp_conslist, ArrayType::get(IntegerType::get(32,module.get())));
        builder->create_store(CONST(0),t);
        t=builder->create_bitcast(t, ArrayType::get(union_conslist));
        t=builder->create_load(t);
        para.push_back(t);
    } else {
        auto element_type=type->element_type;
        para.push_back(CONST(int(node.elements->size())));
        //Instruction* temp_conslist=builder->create_alloca(union_conslist);
        auto temp_conslist= scope.find("temp_conslist");
        for(auto i:*node.elements) {
            i->accept(*this);
            auto v=visitor_return_value;
            if (element_type->get_name()=="int") {
                Instruction* t=builder->create_bitcast(temp_conslist, ArrayType::get(IntegerType::get(32,module.get())));
                builder->create_store(v,t);
                t=builder->create_bitcast(t, ArrayType::get(union_conslist));
                t=builder->create_load(t);
                para.push_back(t);
            } else if (element_type->get_name()=="bool") {
                Instruction* t=builder->create_bitcast(temp_conslist, ArrayType::get(IntegerType::get(32,module.get())));
                auto v_32 = builder->create_zext(v, IntegerType::get(32,module.get()));
                builder->create_store(v_32,t);
                t=builder->create_bitcast(t, ArrayType::get(union_conslist));
                t=builder->create_load(t);
                para.push_back(t);
            } else {
                //TODO: dynamic type cast 
                Instruction* t=builder->create_bitcast(temp_conslist, ArrayType::get(ArrayType::get(IntegerType::get(32,module.get()))));
                auto v2=builder->create_bitcast(v, ArrayType::get(IntegerType::get(32,module.get())));
                builder->create_store(v2, t);
                t=builder->create_bitcast(t, ArrayType::get(union_conslist));
                t=builder->create_load(t);
                para.push_back(t);
            }
        }
    }
    auto conslist_fun = scope.find_in_global("conslist");
    Instruction* list=builder->create_call(conslist_fun,para);
    list=builder->create_bitcast(list, ArrayType::get(ArrayType::get(list_class)));
    list = builder->create_load(list);
    visitor_return_value = list;

}
void LightWalker::visit(parser::ListType &node) {
    visitor_return_type = ArrayType::get(list_class);
}
void LightWalker::visit(parser::MemberExpr &node) {
    const auto param_get_lvalue = get_lvalue;    
    get_lvalue = false, visitor_return_value = nullptr;
    node.object->accept(*this);
    auto obj = visitor_return_value;
    assert(obj);
    auto class_type = dynamic_cast<Class*>(scope.find_in_global(node.object->inferredType->get_name()));
    assert(class_type);

    auto is_none = builder->create_icmp_eq(obj, ConstantNull::get(obj->get_type()));
    auto b = builder->get_insert_block();
    auto b_true = BasicBlock::create(module.get(),"",b->get_parent());
    auto b_end = BasicBlock::create(module.get(),"",b->get_parent());
    builder->create_cond_br(is_none, b_true, b_end);
    builder->set_insert_point(b_true);
    builder->create_call(scope.find_in_global("error.None"), {});
    builder->create_br(b_end);
    builder->set_insert_point(b_end);

    if (class_type->get_attr_offset(node.member->name) < class_type->get_attribute()->size()) {
        // member is a attribute
        auto v = builder->create_gep(obj, CONST(3 + class_type->get_attr_offset(node.member->name)));
        if (param_get_lvalue) {
            visitor_return_value = v;
        } else {
            auto t = builder->create_load(v);
            visitor_return_value = t;
        }
    } else {
        // member is a method
        auto dispatch_table_ptr = builder->create_gep(obj, CONST(2));
        auto dispatch_table = builder->create_load(dispatch_table_ptr);
        auto offset = class_type->get_method_offset(node.member->name);
        assert(offset < class_type->get_method()->size());
        auto func_ptr = builder->create_gep(dispatch_table, CONST(offset));
        auto func_type = class_type->get_method()->at(offset)->get_type();
        func_ptr->set_type(ArrayType::get(ArrayType::get(func_type)));
        auto func = builder->create_load(func_ptr);
        visitor_return_object = obj;
        visitor_return_value = func;
        visitor_return_type = func_type;
    }
}
void LightWalker::visit(parser::IfStmt &node)
{
    node.condition->accept(*this);
    auto cond = visitor_return_value;
    auto b = builder->get_insert_block();
    auto b_true = BasicBlock::create(module.get(),"",b->get_parent());
    auto b_end = BasicBlock::create(module.get(),"",b->get_parent());
    if (node.elseBody==nullptr && node.elifBody==nullptr) {
        builder->create_cond_br(cond, b_true, b_end);
        builder->set_insert_point(b_true);
        for(const auto& stmt : *node.thenBody) {
            stmt->accept(*this);
        }
        builder->create_br(b_end);
    } else {
        auto b_false = BasicBlock::create(module.get(),"",b->get_parent());
        builder->create_cond_br(cond, b_true, b_false);
        builder->set_insert_point(b_true);
        for(const auto& stmt : *node.thenBody) {
            stmt->accept(*this);
        }
        builder->create_br(b_end);
        builder->set_insert_point(b_false);
        if (node.elseBody!=nullptr) {
            for(const auto& stmt : *node.elseBody) {
                stmt->accept(*this);
            }
        } else {
            node.elifBody->accept(*this);
        }
        builder->create_br(b_end);
    } 
    builder->set_insert_point(b_end);
}
void LightWalker::visit(parser::MethodCallExpr &node) {
    visitor_return_object = nullptr; visitor_return_value = nullptr; visitor_return_type = nullptr; node.method->accept(*this);
    assert(visitor_return_value);
    auto func = visitor_return_value;
    auto func_type = visitor_return_type;

    std::vector<Value*> args{visitor_return_object};
    for (auto arg : *node.args) {
        visitor_return_value = nullptr;
        arg->accept(*this);
        assert(visitor_return_value);
        args.push_back(this->visitor_return_value);
    }
    visitor_return_value = builder->create_call(func, func_type, std::move(args));
}
void LightWalker::visit(parser::NoneLiteral &node) 
{
    //TODO: LightWalker for NoneLiteral
    auto nonconv_fun = scope.find_in_global("noconv");
    auto t1 = builder->create_call(nonconv_fun,vector<Value*>());
    visitor_return_value = t1;
}
void LightWalker::visit(parser::NonlocalDecl &) { }
void LightWalker::visit(parser::ReturnStmt &node) {
    if (node.value == nullptr) {
        builder->create_ret(new ConstantNull(ArrayType::get((Class*)scope.find_in_global("object"))));
        return;
    }
    visitor_return_value = nullptr;
    node.value->accept(*this);
    assert(visitor_return_value);
    builder->create_ret(visitor_return_value);
}
void LightWalker::visit(parser::StringLiteral &node)
{
    int id=get_const_type_id();
    auto C=ConstantStr::get(node.value, id, module.get());
    auto t=GlobalVariable::create("const_"+std::to_string(id),module.get(),C);
    auto ptr_type = ArrayType::get(dynamic_cast<Class*>(
        scope.find_in_global("virtual_str_1145141919810")));
    t->set_type(ptr_type);
    auto p = GlobalVariable::create(
        "ptr_const_"+std::to_string(id),
        module.get(),
        ptr_type,
        false,
        ConstantNull::get(ptr_type)
    );
    //std::cerr<<p->get_type()->print()<<"\n";
    builder->create_store(t, p);
    auto p2=builder->create_load(p);
    
    visitor_return_value = p2;
}
void LightWalker::visit(parser::TypeAnnotation &){}
void LightWalker::visit(parser::TypedVar &) {}
void LightWalker::visit(parser::UnaryExpr &node) 
{
    node.operand->accept(*this);
    auto v=visitor_return_value;
    Instruction *res=nullptr;
    if (node.operator_=="-") {
        res = builder->create_ineg(v);
    } else if (node.operator_=="not") {
        res = builder->create_not(v);
        res->set_type(i1_type);
    } else {
        assert(false);
    }
    visitor_return_value = res;
}
void LightWalker::visit(parser::VarDef & node) 
{
    if (scope.in_global()) {
        if (node.var->type->get_name()=="int") {
            auto t=GlobalVariable::create(
                node.var->identifier->name,
                module.get(),
                IntegerType::get(32, module.get()),
                false,
                ConstantInt::get(node.value->int_value,module.get())
                );
            scope.push_in_global(node.var->identifier->name,t);
        } else if (node.var->type->get_name()=="bool") {
            auto b = dynamic_cast<parser::BoolLiteral*>(node.value);
            assert(b);
            auto t=GlobalVariable::create(
                node.var->identifier->name,
                module.get(),
                IntegerType::get(1, module.get()),
                false,
                ConstantInt::get(b->bin_value,module.get())
                );
            scope.push_in_global(node.var->identifier->name,t);
        } else if (node.var->type->get_name()=="str") {
            auto s = dynamic_cast<parser::StringLiteral*>(node.value);
            int id=get_const_type_id();
            auto t=GlobalVariable::create(
                "const_"+std::to_string(id),
                module.get(),
                ConstantStr::get(s->value,id,module.get())
            );
            auto ptr_type = ArrayType::get(dynamic_cast<Class*>(
                scope.find_in_global("virtual_str_1145141919810")));
            t->set_type(ptr_type);
            auto p = GlobalVariable::create(
                node.var->identifier->name,
                module.get(),
                ptr_type,
                false,
                ConstantNull::get(ptr_type)
            );
            //std::cerr<<p->get_type()->print()<<"\n";
            builder->create_store(t, p);
            scope.push_in_global(node.var->identifier->name,p);
        } else if (node.var->type->kind == "ClassType") {
            const auto& class_name = node.var->type->get_name();
            const auto class_type = dynamic_cast<Class*>(scope.find_in_global(class_name));
            assert(class_type); // ! will failed in forward declaration
            const auto var_type = ArrayType::get(class_type);
            const auto init_val = ConstantNull::get(var_type);
            auto t = GlobalVariable::create(
                node.var->identifier->name,
                module.get(),
                var_type,
                false,
                init_val
            );
            builder->create_store(null, t);
            scope.push_in_global(node.var->identifier->name, t);
        } else if (auto list_type=dynamic_cast<parser::ListType*>(node.var->type)) {
            auto ptr_list_type = ArrayType::get(list_class);
            auto t=GlobalVariable::create(
                node.var->identifier->name,
                module.get(),
                ptr_list_type,
                false,
                ConstantNull::get(ptr_list_type)
            );
            builder->create_store(null, t);
            scope.push_in_global(node.var->identifier->name, t);
        }
    } else {
        if (node.var->type->get_name()=="int") {
            auto t=builder->create_alloca(IntegerType::get(32, module.get()));
            builder->create_store(
                ConstantInt::get(node.value->int_value,module.get()),
                t
            );
            scope.push(node.var->identifier->name,t);
        } else if (node.var->type->get_name()=="bool") {
            auto b = dynamic_cast<parser::BoolLiteral*>(node.value);
            assert(b);
            auto t=builder->create_alloca(IntegerType::get(1, module.get()));
            builder->create_store(
                ConstantInt::get(b->bin_value,module.get()),
                t
            );
            scope.push(node.var->identifier->name,t);
        } else if (node.var->type->get_name()=="str") {
            auto s = dynamic_cast<parser::StringLiteral*>(node.value);
            int id=get_const_type_id();
            auto t=GlobalVariable::create(
                "const_"+std::to_string(id),
                module.get(),
                ConstantStr::get(s->value,id,module.get())
            );
            t->set_type(ptr_vstr_type);
            auto p = builder->create_alloca(ptr_vstr_type);
            builder->create_store(t, p);
            scope.push(node.var->identifier->name,p);
        } else if (node.var->type->kind == "ClassType") {
            const auto& class_name = node.var->type->get_name();
            const auto class_type = dynamic_cast<Class*>(scope.find_in_global(class_name));
            assert(class_type); // ! will failed in forward declaration
            const auto var_type = ArrayType::get(class_type);
            const auto init_val = ConstantNull::get(var_type);
            auto t = builder->create_alloca(var_type);
            builder->create_store(init_val, t);
            scope.push(node.var->identifier->name, t);
        } else if (auto list_type=dynamic_cast<parser::ListType*>(node.var->type)) {
            auto ptr_list_type = ArrayType::get(list_class);
            auto t = builder->create_alloca(ptr_list_type);
            builder->create_store(ConstantNull::get(ptr_list_type), t);
            scope.push(node.var->identifier->name, t);
        }
    }
}
void LightWalker::visit(parser::WhileStmt &node) 
{
    auto before=builder->get_insert_block();
    auto b_cond=BasicBlock::create(module.get(),"",before->get_parent());
    auto b_body=BasicBlock::create(module.get(),"",before->get_parent());
    auto b_end=BasicBlock::create(module.get(),"",before->get_parent());
    builder->create_br(b_cond);
    builder->set_insert_point(b_cond);
    node.condition->accept(*this);
    auto cond=visitor_return_value;
    builder->create_cond_br(cond,b_body,b_end);
    builder->set_insert_point(b_body);
    for(const auto& stmt : *node.body) {
        stmt->accept(*this);
    }
    builder->create_br(b_cond);
    builder->set_insert_point(b_end);
}
void LightWalker::visit(parser::VarAssignStmt &) {}
void LightWalker::visit(parser::MemberAssignStmt &) {}
void LightWalker::visit(parser::IndexAssignStmt &) {}
void LightWalker::visit(parser::VarAssignExpr &) {}
void LightWalker::visit(parser::MemberAssignExpr &) {}
void LightWalker::visit(parser::IndexAssignExpr &){}
void LightWalker::visit(parser::Err &) {};
void LightWalker::visit(parser::Node &) {};
void LightWalker::visit(parser::IndexExpr &node)
{
    bool is_get_lvalue=get_lvalue;
    get_lvalue=false;
    node.list->accept(*this);
    auto list=visitor_return_value;
    node.index->accept(*this);
    auto idx=visitor_return_value;

    auto noconv_fun = scope.find_in_global("noconv");
    auto null= builder->create_call(noconv_fun,vector<Value*>());
    auto cond_null=builder->create_icmp_eq(list, null);
    auto b_before_null = builder->get_insert_block();
    auto b_null_true= BasicBlock::create(module.get(), "", b_before_null->get_parent());
    auto b_null_false = BasicBlock::create(module.get(), "", b_before_null->get_parent());
    builder->create_cond_br(cond_null, b_null_true, b_null_false);
    builder->set_insert_point(b_null_true);
    auto none_func=scope.find_in_global("error.None");
    builder->create_call(none_func,vector<Value*>());
    builder->create_br(b_null_false);
    builder->set_insert_point(b_null_false);

    auto len_func = scope.find_in_global("$len");
    auto t1 = builder->create_bitcast(list, ArrayType::get(union_len));
    auto v_len = builder->create_call(len_func,{t1});
    auto b= builder->get_insert_block();
    auto b_oob = BasicBlock::create(module.get(), "", b->get_parent());
    auto b_1 = BasicBlock::create(module.get(), "", b->get_parent());
    auto cond_1= builder->create_icmp_ge(idx, v_len);
    auto cond_2 = builder->create_icmp_lt(idx, CONST(0));
    auto cond = builder->create_ior(cond_1, cond_2);
    cond->set_type(i1_type);
    builder->create_cond_br(cond, b_oob, b_1);
    builder->set_insert_point(b_oob);
    auto oob_func=scope.find_in_global("error.OOB");
    builder->create_call(oob_func,vector<Value*>());
    builder->create_br(b_1);
    builder->set_insert_point(b_1);
    if (node.list->inferredType->get_name()=="str") {
        assert(!is_get_lvalue);
        auto g_char_table=scope.find_in_global("global_char_table");
        auto char_table = builder->create_load(g_char_table);
        auto p_str = builder->create_gep(list, CONST(4));
        auto str = builder->create_load(p_str);
        str->set_type(ArrayType::get(IntegerType::get(8, module.get())));
        auto p_char = builder->create_gep(str, idx);
        auto i_char = builder->create_load(p_char);
        auto i32_char = builder->create_zext(i_char, IntegerType::get(32,module.get()));
        auto idx = builder->create_isub(i32_char, CONST(0));

        auto t2 = builder->create_gep(char_table, CONST(4));
        t2->set_type(ArrayType::get(ArrayType::get(this->union_conslist)));
        auto t3=builder->create_load(t2);
        auto t5 = builder->create_gep(t3, idx);
        auto t6 = builder->create_bitcast(t5, ArrayType::get(ArrayType::get(dynamic_cast<Class*>(scope.find_in_global("virtual_str_1145141919810")))));
        auto t7 = builder->create_load(t6);
        visitor_return_value=t7;
    } else {
        Instruction *res=nullptr;
        if (node.inferredType->get_name()=="int") {
            auto t2 = builder->create_gep(list, CONST(4));
            t2->set_type(ArrayType::get(ArrayType::get(this->union_conslist)));
            auto t3=builder->create_load(t2);
            auto t5 = builder->create_gep(t3, idx);
            auto t6 = builder->create_bitcast(t5, ArrayType::get(IntegerType::get(32,module.get())));
            res=t6;
        } else if (node.inferredType->get_name()=="bool") {
            auto t2 = builder->create_gep(list, CONST(4));
            t2->set_type(ArrayType::get(ArrayType::get(this->union_conslist)));
            auto t3=builder->create_load(t2);
            auto t5 = builder->create_gep(t3, idx);
            auto t6 = builder->create_bitcast(t5, ArrayType::get(IntegerType::get(1,module.get())));
            res=t6;
        } else {
            auto t2 = builder->create_gep(list, CONST(4));
            t2->set_type(ArrayType::get(ArrayType::get(this->union_conslist)));
            auto t3=builder->create_load(t2);
            auto t5 = builder->create_gep(t3, idx);
            auto type_name=node.inferredType->get_name();
            if (type_name=="str") {
                type_name="virtual_str_1145141919810";
            }
            Type *type=nullptr;
            if (type_name[0]=='[') {
                type=list_class->get_type();
            } else {
                type = dynamic_cast<Class*>(scope.find_in_global(type_name));
            }
            auto t6 = builder->create_bitcast(t5, ArrayType::get(ArrayType::get(type)));
            res=t6;
        }
        if (!is_get_lvalue) {
            res=builder->create_load(res);
        }
        get_lvalue=is_get_lvalue;
        visitor_return_value=res;
    }
}
} // namespace lightir

void print_help_all(const string_view &exe_name) {
    std::cout << fmt::format(
                     "Usage: {} [ -h | --help ] [ -O3 ] [ -O0 ] <input-file>",
                     exe_name)
              << std::endl;
}

void print_help(const string_view &exe_name) {
    std::cout << fmt::format(
                     "Usage: {} [ -h | --help ] [ -o <target-file> ] [ -emit ] [ -run ] [ -assem ] <input-file>",
                     exe_name)
              << std::endl;
}


#ifdef PA3
int main(int argc, char *argv[]) {
    string target_path;
    string input_path;
    string IR;
    vector<string> passes;

    bool emit = false;
    bool run = false;
    bool assem = false;

    lightir::enable_str_for=true;

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
        } else if (argv[i] == "-strfor"s) {
            if (argv[i+1]=="true") {
                lightir::enable_str_for=true;
            } else {
                lightir::enable_str_for=false;
            }
            i+=1;
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

        if (emit) {
            cout << "\nLLVM IR:\n; ModuleID = 'chocopy'\nsource_filename = \"\"" << input_path << "\"\"\n\n" << IR;
        }

        if (run) {
            /*auto command_string_llc = fmt::format("/llvm/bin/llc {}.ll", target_path);
            int re_code_llc = std::system(command_string_llc.c_str());
            if (re_code_llc!=0) {
                return 1;
            }*/
#ifdef RV64
            auto command_string = "clang -mno-relax  -no-integrated-as -O1 -w --target=riscv64-unknown-linux-gnu "s +
                                  target_path + ".ll -o " + target_path + ".s -S";
            int re_code = std::system(command_string.c_str());
            if (target_path.ends_with("str_cat") || target_path.ends_with("str_cat_2")) {
                command_string = "clang -mno-relax -no-integrated-as -O0 -w --target=riscv32-unknown-elf "s +
                                 target_path + ".ll -o " + target_path + ".s -S " +
                                 R"(&& /usr/bin/sed -i '' 's/.*addrsig.*//g' )" + target_path + ".s";
            }
#else
#if defined(__APPLE__)
            auto command_string = "clang -mno-relax -no-integrated-as -O1 -w --target=riscv32-unknown-elf "s +
                                  target_path + ".ll -o " + target_path + ".s -S " +
                                  R"(&& /usr/bin/sed -i '' 's/.*addrsig.*//g' )" + target_path + ".s";
#else
            auto command_string = "/llvm/bin/llc -opaque-pointers -O0 -verify-machineinstrs -mtriple=riscv32-unknown-elf "s +
                                  target_path + ".ll -o " + target_path + ".s" +
                                  R"(&& /usr/bin/sed -i  's/.*addrsig.*//g' )" + target_path + ".s";
#endif
            int re_code = std::system(command_string.c_str());
#endif
            LOG(INFO) << command_string << re_code;
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
        if (assem) {
#ifdef RV64
            auto command_string = "clang -mno-relax --target=riscv64 -O1 -w -S --target=riscv64-unknown-linux-gnu "s +
                                  target_path + ".ll -o " + target_path + ".s -L. -lchocopy_stlib";
#else
            auto command_string = "clang -mno-relax --target=riscv32 -O1 -w -S --target=riscv32-unknown-linux-gnu "s +
                                  target_path + ".ll -o " + target_path + ".s -L. -lchocopy_stdlib && /bin/cat " +
                                  target_path + ".s";
#endif
            int re_code = std::system(command_string.c_str());
            LOG(INFO) << command_string;
            if (re_code == 0)
                goto result;
            else
                return 1;
        }
    }

result:
    return 0;
}
#endif
