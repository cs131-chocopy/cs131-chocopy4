//
// Created by yiwei yang on 2/18/21.
//

#include "FunctionDefType.hpp"
#include "SymbolType.hpp"
#include "ValueType.hpp"
#include "chocopy_parse.hpp"
#include <algorithm>
#include <chocopy_semant.hpp>
#include <fmt/core.h>
#include <fmt/format.h>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <set>
#include <utility>
#include <cassert>

using std::map;
using std::set;

bool is_lambda_pass = false;
bool is_rvalue = false;
vector<string> lambda_params;
template <class Key, class T> set<string> key_to_set(const std::map<Key, T> &map, std::set<Key> &set) {
    set.clear();
    auto itr = map.begin();
    while (map.end() != itr) {
        set.insert((itr++)->first);
    }
    return set;
}

namespace semantic {
cJSON *FunctionDefType::toJSON() const {
    cJSON *d = cJSON_CreateObject();

    cJSON_AddStringToObject(d, "kind", "FuncType");
    auto param = cJSON_CreateArray();

    if (!this->params->empty())
        for (auto &parameter : *this->params) {
            auto type_name = parameter->get_type().c_str();
            auto tmp_param = cJSON_CreateObject();

            if (dynamic_cast<ListValueType *>(parameter)) {
                cJSON_AddStringToObject(tmp_param, "kind", type_name);
                cJSON_AddItemToObject(tmp_param, "elementType",
                                      parser::add_inferred_type(((ListValueType *)parameter)->element_type));
                cJSON_AddItemToArray(param, tmp_param);
            } else {
                auto param_name = ((ClassValueType *)parameter)->class_name.c_str();
                cJSON_AddStringToObject(tmp_param, "kind", type_name);
                cJSON_AddStringToObject(tmp_param, "className", param_name);
                cJSON_AddItemToArray(param, tmp_param);
            }
        }
    cJSON_AddItemToObject(d, "parameters", param);

    auto class_name = ((ClassValueType *)this->return_type)->class_name.c_str();
    auto tmp_return = cJSON_CreateObject();
    cJSON_AddStringToObject(tmp_return, "kind", "ClassValueType");
    cJSON_AddStringToObject(tmp_return, "className", class_name);
    cJSON_AddItemToObject(d, "returnType", tmp_return);
    return d;
}
void FunctionDefType::set_name(string_view className) { ((ClassValueType *)this->return_type)->class_name = className; }
template <typename Ty> bool SymbolType::eq(const Ty &Value) const  {
    if (this->is_list_type()) {
        if (dynamic_cast<ListValueType const *>(Value))
            return ((ListValueType const *)this)->element_type->eq(((ListValueType const *)Value)->element_type);
        else
            return false;
    } else if (this->is_value_type()) {
        if (dynamic_cast<ClassValueType const *>(Value))
            return ((ClassValueType const *)this)->get_name() == ((ClassValueType const *)Value)->get_name();
        else
            return false;
    }
    return false;
}
template <typename Ty> bool SymbolType::neq(const Ty &Value) const { return !(this->eq(Value)); }

void SymbolTableGenerator::visit(parser::Program &program) {
    for(const auto decl : *program.declarations) {
        auto id = decl->get_id();
        const auto &name = id->name;
        ret = nullptr;
        decl->accept(*this);

        if (sym->declares(name)) {
            errors->emplace_back(new SemanticError(id, "Duplicate declaration of identifier in same scope: " + name));
            ignore.emplace_back(decl);
            delete ret;
        } else {
            sym->put(name, ret);
        }
    }
}
void SymbolTableGenerator::visit(parser::ClassDef &class_def) {
    const auto& super_name = class_def.superClass->name;
    const auto _super_class = globals->get<SymbolType*>(super_name);
    if (_super_class == nullptr) {
        errors->emplace_back(new SemanticError(class_def.superClass, "Super-class not defined: " + super_name));
    }
    const auto super_class = dynamic_cast<ClassDefType*>(_super_class);
    if (_super_class != nullptr && super_class == nullptr) {
        errors->emplace_back(new SemanticError(class_def.superClass, "Super-class must be a class: " + super_name));
    }
    if (super_name == "int" || super_name == "str" || super_name == "bool") {
        errors->emplace_back(new SemanticError(class_def.superClass, "Cannot extend special class: " + super_name));
    }
    SymbolTable const *const super_scope = super_class == nullptr ? nullptr : super_class->current_scope;

    const auto& class_name = class_def.name->name;
    const auto cur = new ClassDefType(super_name, class_name);
    cur->current_scope->parent = this->sym;

    this->sym = cur->current_scope;

    for(const auto decl : *class_def.declaration) {
        const auto id = decl->get_id();
        const auto& name = id->name;

        if (sym->declares(name)) {
            errors->emplace_back(new SemanticError(id, "Duplicate declaration of identifier in same scope: " + name));
            ignore.emplace_back(decl);
            continue;
        }

        decl->accept(*this);

        if (super_scope == nullptr) { sym->put(name, ret); continue; }

        if (dynamic_cast<ValueType*>(ret)) {
            if (super_scope->declares(name)) {
                errors->emplace_back(new SemanticError(id, "Cannot re-define attribute: " + name));
            }
            sym->put(name, ret);
        } else {
            const auto func = dynamic_cast<FunctionDefType*>(ret);
            assert(func);
            const auto params = func->params;
            if (params->size() < 1 || dynamic_cast<ClassValueType*>(params->front()) == nullptr || params->front()->get_name() != class_name) {
                errors->emplace_back(new SemanticError(id, "First parameter of the following method must be of the enclosing class: " + name));
            }
            if (name != "__init__" && super_scope->declares(name)) {
                auto super_func = super_class->current_scope->get<FunctionDefType*>(name);
                if (super_func) {
                    if (*super_func == *func) {
                        sym->put(name, func);
                    } else {
                        ignore.emplace_back(decl);
                        errors->emplace_back(new SemanticError(id, "Method overridden with different type signature: " + name));
                    }
                } else {
                    ignore.emplace_back(decl);
                    errors->emplace_back(new SemanticError(id, "Cannot re-define attribute: " + name));
                }
            } else {
                sym->put(name, func);
            }
        }
    }

    if (super_scope) {
        for(const auto& kv : *super_scope->tab) {
            if (sym->declares(kv.first)) continue;
            sym->put(kv.first, kv.second);
            cur->inherit_members.emplace_back(kv.first);
        }
    }

    ret = cur;
    this->sym = this->sym->parent;
}
void SymbolTableGenerator::visit(parser::FuncDef & func_def) {
    const auto id = func_def.get_id();
    const auto& name = id->name;

    const auto cur = new FunctionDefType;
    cur->current_scope->parent = this->sym;
    this->sym = cur->current_scope;

    cur->func_name = name;
    cur->return_type = ValueType::annotate_to_val(func_def.returnType);

    for (const auto param : *func_def.params) {
        const auto id = param->identifier;
        const auto& name = id->name;

        const auto type = ValueType::annotate_to_val(param->type);
        cur->params->emplace_back(type);

        if (sym->declares(name)) {
            errors->emplace_back(new SemanticError(id, "Duplicate declaration of identifier in same scope: " + name));
            continue;
        }

        sym->put(name, type);
    }


    for(const auto& decl : *func_def.declarations) {
        const auto id = decl->get_id();
        const auto& name = id->name;

        if (sym->declares(name)) {
            errors->emplace_back(new SemanticError(id, "Duplicate declaration of identifier in same scope: " + name));
            ignore.emplace_back(decl);
            continue;
        }

        ret = nullptr;
        decl->accept(*this);
        if (ret) sym->put(name, ret);
    }

    ret = cur;
    this->sym = this->sym->parent;
}
void SymbolTableGenerator::visit(parser::VarDef &var_def) {
    ret = ValueType::annotate_to_val(var_def.var->type);
}
void SymbolTableGenerator::visit(parser::NonlocalDecl &nonlocal_decl) {
    ret = new NonlocalRefType(nonlocal_decl.get_id()->name);
}
void SymbolTableGenerator::visit(parser::GlobalDecl &global_decl) {
    ret = new GlobalRefType(global_decl.get_id()->name);
}

void DeclarationAnalyzer::debug_sym() {
    for (const auto &x : *sym->tab) {
        if ((x.second)->is_list_type()) {
            LOG(INFO) << x.first << " : [" << ((ClassValueType *)((ListValueType *)x.second)->element_type)->get_name()
                      << "]\n";
        } else if ((x.second)->is_func_type()) {
            if (dynamic_cast<FunctionDefType *>(x.second))
                LOG(INFO) << "Function " << x.first << " : "
                          << dynamic_cast<FunctionDefType *>(x.second)->return_type->get_name() << "\nparam\n";
            for (const auto y : *dynamic_cast<FunctionDefType *>(x.second)->params)
                if (!y->is_list_type())
                    LOG(INFO) << "      : " << ((ClassValueType *)y)->get_name() << "\n";
                else
                    LOG(INFO) << "      : " << ((ClassValueType *)((ListValueType *)y)->element_type)->get_name()
                              << "\n";
        } else if ((x.second)->is_value_type()) {
            LOG(INFO) << x.first << " : " << ((ClassValueType *)x.second)->get_name() << "\n";
        }
    }
}
void DeclarationAnalyzer::visit(parser::Program &program) {
    for(const auto decl : *program.declarations) {
        if (ignore(decl)) continue;
        decl->accept(*this);
    }

    for(const auto stmt : *program.statements) {
        if (dynamic_cast<parser::ReturnStmt*>(stmt)) {
            errors->emplace_back(new SemanticError(stmt, "Return statement cannot appear at the top level"));
        }
    }
}
void DeclarationAnalyzer::visit(parser::VarDef &var_def) {
    const auto id = var_def.var->identifier;
    const auto& name = id->name;

    assert(sym->get<ValueType*>(name));

    checkVarName(name, id);
    checkValueType(sym->get<ValueType*>(name), var_def.var->type);
}
void DeclarationAnalyzer::visit(parser::ClassDef &class_def) {
    const auto& class_name = class_def.get_id()->name;
    const auto class_type = sym->get<ClassDefType*>(class_name);
    assert(class_type);
    current_class = class_type;
    sym = class_type->current_scope;

    for(const auto decl : *class_def.declaration) {
        if (ignore(decl)) continue;
        decl->accept(*this);
    }

    sym = sym->parent;
    current_class = nullptr;
}
void DeclarationAnalyzer::visit(parser::FuncDef &func_def) {
    const auto id = func_def.get_id();
    const auto& name = id->name;

    // const auto cur = new FunctionDefType;
    // cur->current_scope->parent = this->sym;
    FunctionDefType * const func_type = sym->declares<FunctionDefType*>(name);
    assert(func_type);
    sym = func_type->current_scope;

    for(int i = 0; i < func_type->params->size(); i++) {
        const auto annoation = func_def.params->at(i)->type;
        const auto value_type = dynamic_cast<ValueType*>(func_type->params->at(i));
        assert(value_type);
        checkValueType(value_type, annoation);
    }

    for(const auto& decl : *func_def.declarations) {
        if (ignore(decl)) {
            if (dynamic_cast<parser::VarDef*>(decl)) {
                const auto t = ValueType::annotate_to_val(((parser::VarDef*)decl)->var->type);
                checkValueType(t, ((parser::VarDef*)decl)->var->type);
                delete t;
            }
            continue;
        }
        decl->accept(*this);
    }

    if (name == "__init__" && current_class) {
        for (const auto stmt : *func_def.statements) {
            if (dynamic_cast<parser::ReturnStmt*>(stmt)) {
                errors->emplace_back(new SemanticError(stmt, "Return statements should not appear in method: __init__"));
            }
        }
    }

    this->sym = this->sym->parent;
}
void DeclarationAnalyzer::visit(parser::GlobalDecl &global_decl) {
    const auto id = global_decl.get_id();
    const auto& name = id->name;
    const auto type = sym->get<GlobalRefType*>(name);
    assert(type);

    const auto real_type = globals->declares<ValueType*>(name);
    if (!real_type) errors->emplace_back(new SemanticError(id, "Not a global variable: " + name));

    delete type;
    sym->put(name, real_type);
}
void DeclarationAnalyzer::visit(parser::NonlocalDecl &nonlocal_decl) {
    const auto id = nonlocal_decl.get_id();
    const auto& name = id->name;
    const auto type = sym->get<NonlocalRefType*>(name);
    assert(type);

    ValueType* real_type = nullptr;
    auto table = sym->parent;
    while (real_type == nullptr && table != globals.get()) {
        real_type = table->declares<ValueType*>(name);
        table = table->parent;
    }
    if (!real_type) errors->emplace_back(new SemanticError(id, "Not a nonlocal variable: " + name));

    delete type;
    sym->put(name, real_type);
}

template <typename... Args> void TypeChecker::typeError(parser::Node *node, const string &message, Args... rest) {
    node->semError(rest...);
    node->semError(message);
}
void TypeChecker::typeError(parser::Node *node, const string &message) {
    if (!node->has_type_err()) {
        node->typeError = message;
    } else {
        node->typeError += "\t" + message;
    }
}
void TypeChecker::visit(parser::Program &program) {

    for (auto decl : *program.declarations) {
        decl->accept(*this);
    }
    for (auto stmt : *program.statements) {
        stmt->accept(*this);
    }
}
void TypeChecker::visit(parser::BinaryExpr &node)
{
    node.left->accept(*this);
    node.right->accept(*this);
    auto lc=node.left->inferredType,rc=node.right->inferredType;
    if (node.operator_=="+") {
        if (lc->get_name()=="int" && rc->get_name()=="int") {
            node.inferredType=new ClassValueType("int");
            return;
        }
        if (lc->get_name()=="str" && rc->get_name()=="str") {
            node.inferredType=new ClassValueType("str");
            return;
        }
        auto lcl = dynamic_cast<ListValueType*>(lc);
        auto rcl = dynamic_cast<ListValueType*>(rc);
        if (lcl != nullptr && rcl != nullptr) {
            node.inferredType = new ListValueType((ValueType*)get_common_type_2(lcl->element_type, rcl->element_type));
            return;
        }
        typeError(&node,fmt::format("Cannot apply opreator + on types {} and {}",lc->get_name(),rc->get_name()));
        if (lc->get_name()=="int" || rc->get_name()=="int") {
            node.inferredType=new ClassValueType("int");
        } else {
            node.inferredType=new ClassValueType("object");
        }
    } else if (node.operator_=="-" || node.operator_=="*" || node.operator_=="//" || node.operator_=="%") {
        if (lc->get_name()=="int" && rc->get_name()=="int") {
            node.inferredType=new ClassValueType("int");
        } else {
            typeError(&node,fmt::format("Cannot apply opreator {} on types {} and {}",node.operator_,lc->get_name(),rc->get_name()));
        }
    } else if (node.operator_=="==" || node.operator_=="!=") {
        if ((lc->get_name()=="int" && rc->get_name()=="int") || (lc->get_name()=="str" && rc->get_name()=="str") || (lc->get_name()=="bool" && rc->get_name()=="bool")) {
            node.inferredType=new ClassValueType("bool");
        } else {
            typeError(&node,fmt::format("Cannot apply opreator {} on types {} and {}",node.operator_,lc->get_name(),rc->get_name()));
        }
    } else if (node.operator_=="<=" || node.operator_==">=" || node.operator_=="<" || node.operator_==">") {
        if (lc->get_name()=="int" && rc->get_name()=="int") {
            node.inferredType=new ClassValueType("bool");
        } else {
            typeError(&node,fmt::format("Cannot apply opreator {} on types {} and {}",node.operator_,lc->get_name(),rc->get_name()));
        }
    } else if (node.operator_=="and" || node.operator_=="or") {
        if (lc->get_name()=="bool" && rc->get_name()=="bool") {
            node.inferredType=new ClassValueType("bool");
        } else {
            typeError(&node,fmt::format("Cannot apply opreator {} on types {} and {}",node.operator_,lc->get_name(),rc->get_name()));
        }
    } else if (node.operator_=="is") {
        auto lcn=lc->get_name(),rcn=rc->get_name();
        if (lcn=="int" || lcn=="bool" || lcn=="str" || rcn=="int" || rcn=="bool" || rcn=="str") {
            typeError(&node,fmt::format("Cannot apply opreator {} on types {} and {}",node.operator_,lc->get_name(),rc->get_name()));
        }
        node.inferredType=new ClassValueType("bool");
    } else {
        assert(false);
    }
}
void TypeChecker::visit(parser::BoolLiteral &node)
{
    node.inferredType=new ClassValueType("bool");
}
void TypeChecker::visit(parser::CallExpr &node) {
    const auto func = sym->get<FunctionDefType*>(node.function->name);
    const auto class_ = global->get<ClassDefType*>(node.function->name);
    if (func == nullptr && class_ == nullptr) {
        typeError(&node, "Not a function or class: " + node.function->name);
        return;
    }
    if (func) {
        if (!global->declares<FunctionDefType*>(node.function->name) && curr_lambda_params) {
            curr_lambda_params->emplace_back(node.function->name);
        }
        node.function->inferredType = func;
    }

    std::vector<SymbolType*>::const_iterator param;
    if (func != nullptr) {
        if (node.args->size() != func->params->size()) {
            typeError(&node, fmt::format("Expected {} arguments; got {}", func->params->size(), node.args->size()));
            return;
        }
        param = func->params->cbegin();
    } else {
        const auto init_func = class_->current_scope->get<FunctionDefType*>("__init__");
        if (node.args->size() != init_func->params->size() - 1) {
            typeError(&node, fmt::format("Expected {} arguments; got {}", init_func->params->size() - 1, node.args->size()));
            return;
        }
        param = init_func->params->cbegin();
        param++;
    }

    for(int i = 0; i < node.args->size(); i++) {
        const auto arg = node.args->at(i);
        arg->accept(*this);
        const auto arg_type = arg->inferredType;
        const auto param_type = *(param++);
        if (!is_subtype(arg_type, param_type)) {
            typeError(&node, fmt::format("Expected type `{}`; got type `{}` in parameter {}", param_type->get_name(), arg_type->get_name(), i));
        }
    }
    
    if (func != nullptr) {
        //node.inferredType = new ClassValueType(func->return_type->get_name());
        node.inferredType = func->return_type;
    } else {
        node.inferredType = new ClassValueType(class_->get_name());
    }
}
void TypeChecker::visit(parser::ExprStmt &node) {
    node.expr->accept(*this);
}
void TypeChecker::visit(parser::ForStmt &node)
{
    node.iterable->accept(*this);
    for(auto s:*node.body) {
        s->accept(*this);
    }
    node.identifier->accept(*this);
    auto Q = sym->get<ClassValueType*>(node.identifier->name);
    if (Q==nullptr) {
        typeError(&node,fmt::format("id {} in for statement does not exists or is not a ClassDefType.",node.identifier->name));
        return;
    }
    auto T = global->get<ClassDefType*>(Q->class_name);
    if(T==nullptr) {
        throw("??");
    }

    if (node.iterable->inferredType->get_name()=="str") {
        if (!is_super_class(global,"str",T->get_name())) {
            typeError(&node,fmt::format("id {} in for statement is not a super class of str",node.identifier->name));
        }
    } else if (auto lType=dynamic_cast<ListValueType*>(node.iterable->inferredType)) {
        auto T1 = lType->element_type;
        if (!is_super_class(global,T1->get_name(),T->get_name())) {
            typeError(&node,fmt::format("The type of id {}:{} and iterator type {} do not match.",node.identifier->name,T->get_name(),node.iterable->inferredType->get_name()));
        }
    } else {
        typeError(&node, fmt::format("not iterable type {} in for_stmt",node.iterable->inferredType->get_name()));
    }
}
void TypeChecker::visit(parser::ClassDef &node)
{
    saved.push(sym);
    sym = sym->get<ClassDefType*>(node.name->name)->current_scope;
    for(auto s:*node.declaration) {
        s->accept(*this);
    }
    sym=saved.top();
    saved.pop();
}
void TypeChecker::visit(parser::FuncDef &node)
{
    saved.push(sym);
    saved_func.push(curr_func);
    curr_func = sym->get<FunctionDefType*>(node.name->name);
    assert(curr_func);
    sym = curr_func->current_scope;
    
    std::set<std::string> declared_local_vars;
    for(auto s:*node.declarations) {
        s->accept(*this);
        if (dynamic_cast<parser::VarDef*>(s) || dynamic_cast<parser::FuncDef*>(s)) {
            declared_local_vars.insert(s->get_id()->name);
        }
        if (dynamic_cast<parser::NonlocalDecl*>(s)) {
            node.lambda_params->emplace_back(s->get_id()->name);
        }
        if (dynamic_cast<parser::FuncDef*>(s)) {
            for (auto& x : *((parser::FuncDef*)s)->lambda_params)
                node.lambda_params->emplace_back(x);
        }
    }

    bool have_return = is_subtype("<None>", curr_func->return_type);
    curr_lambda_params = node.lambda_params;
    for(auto s:*node.statements) {
        s->accept(*this);
        if (s->is_return) {
            have_return=true;
        }
    }

    node.lambda_params = new std::vector<std::string>();
    std::copy_if(curr_lambda_params->begin(), curr_lambda_params->end(), std::back_inserter(*node.lambda_params), [&declared_local_vars](const std::string& x) {
        return declared_local_vars.find(x) == declared_local_vars.end();
    });
    delete curr_lambda_params;
    curr_lambda_params = nullptr;
    // std::cerr << node.name->name << " lambda_params: "; for (auto& x : *node.lambda_params) std::cerr << x << ' '; std::cerr << std::endl;

    if (!have_return && node.returnType!=nullptr) {
        this->errors->push_back(new SemanticError(node.name,fmt::format("All paths in this function/method must have a return statement: {}",node.name->name)));
        typeError(&node,fmt::format("All paths in this function/method must have a return statement: {}",node.name->name));
    }

    sym=saved.top();
    saved.pop();
    curr_func = saved_func.top();
    saved_func.pop();
}
void TypeChecker::visit(parser::Ident &node) {
    auto type = sym->declares<SymbolType*>(node.name);
    if (type == nullptr) {
        type = sym->get<SymbolType*>(node.name);
        if (type) {
            if (is_lvalue) {
                this->errors->push_back(new SemanticError(&node,fmt::format("Cannot assign to variable that is not explicitly declared in this scope: {}", node.name)));
            } else {
                if (curr_lambda_params && global->get<SymbolType*>(node.name) != type) {
                    curr_lambda_params->emplace_back(node.name);
                }
                node.inferredType = type;
            }
        } else {
            this->errors->push_back(new SemanticError(&node,fmt::format("Not a variable: {}", node.name)));
        }
    } else {
        if (type->is_value_type()) {
            node.inferredType = type;
        } else {
            this->errors->push_back(new SemanticError(&node,fmt::format("Not a variable: {}", node.name)));
            node.inferredType = new ClassValueType("object");
        }
    }
}
void TypeChecker::visit(parser::IfStmt &node)
{
    bool is_return_t=false,is_return_f=false;
    node.condition->accept(*this);
    if (node.thenBody!= nullptr) {
        for (auto s:*node.thenBody) {
            s->accept(*this);
            if (s->is_return) {
                is_return_t=true;
            }
        }
    }
    if (node.elseBody!=nullptr) {
        for (auto s:*node.elseBody) {
            s->accept(*this);
            if (s->is_return) {
                is_return_f=true;
            }
        }
    }
    if (node.elifBody!=nullptr) {
        node.elifBody->accept(*this);
        if (node.elifBody->is_return) {
            is_return_f=true;
        }
    }
    if (is_return_t && is_return_f) {
        node.is_return = true;
    }
    if (node.condition->inferredType->get_name()!="bool") {
        typeError(&node,fmt::format("Condition expression must be bool"));
    }
}
void TypeChecker::visit(parser::IfExpr &node) {
    node.condition->accept(*this);
    if (node.condition->inferredType->get_name()!="bool") {
        typeError(&node,fmt::format("Condition expression must be bool"));
    }
    node.thenExpr->accept(*this);
    node.elseExpr->accept(*this);
    auto lca = get_common_type_2(node.thenExpr->inferredType,node.elseExpr->inferredType);
    node.inferredType = lca;
}
void TypeChecker::visit(parser::IndexExpr &node)
{
    node.list->accept(*this);
    node.index->accept(*this);
    if (auto cType=dynamic_cast<ClassValueType*>(node.list->inferredType)) {
        if (cType->get_name()=="str") {
            node.inferredType=new ClassValueType("str");
        } else {
            typeError(&node,fmt::format("Indexing on a non-string type {}",cType->get_name()));
        }
    } else if (auto lType=dynamic_cast<ListValueType*>(node.list->inferredType)) {
        node.inferredType=lType->element_type;
    } else {
        throw("A Value type but neither ClassValueType or ListValueType?");
    }
    if (node.index->inferredType->get_name()!="int") {
        typeError(&node,fmt::format("Index is a non-int type {}",node.index->inferredType->get_name()));
    }
}
void TypeChecker::visit(parser::IntegerLiteral &node)
{
    node.inferredType = new ClassValueType("int");
}
void TypeChecker::visit(parser::ListExpr &node)
{
    if (node.elements->empty()) {
        node.inferredType = new ClassValueType("<Empty>");
        return;
    } 
    ValueType* v=nullptr;
    for(auto e:*node.elements) {
        e->accept(*this);
    }
    for(auto e:*node.elements) {
        if(auto T=dynamic_cast<ClassValueType*>(e->inferredType)) {
            if( v==nullptr) {
                v = new ClassValueType(T->get_name());
            }  else {
                if (v->is_list_type()) {
                    v = new ClassValueType("object");
                } else {
                    auto lca = get_common_type(v, T);
                    v = new ClassValueType(lca);
                }
            }
        } else {
            auto T1 = dynamic_cast<ListValueType*>(e->inferredType);
            if (v==nullptr) {
                v = T1;
            } else {
                if (T1->neq(v)) {
                    v= new ClassValueType("object");
                }
            }
        }
    }
    node.inferredType=new ListValueType(v);
}
void TypeChecker::visit(parser::MemberExpr &node)
{
    node.object->accept(*this);
    // member needn't be inferred
    if (auto cls=dynamic_cast<ClassValueType*>(node.object->inferredType)) {
        if (auto type=global->get<ClassDefType*>(cls->class_name)) {
            const auto memType = type->current_scope->declares<SymbolType*>(node.member->name);
            if (dynamic_cast<FunctionDefType*>(memType) || dynamic_cast<ValueType*>(memType)) {
                if (memType->is_func_type() && !node.is_function_call) {
                    typeError(&node,fmt::format("{}.{} is a function, which is not fisrt class citizen in Chocopy",cls->class_name,node.member->name));
                } else if(!memType->is_func_type() && node.is_function_call) {
                    typeError(&node,fmt::format("{}.{} is not a function, but be called",cls->class_name,node.member->name));
                }
                node.inferredType=memType;
            } else {
                typeError(&node,fmt::format("There is no attribute named `{}` in class `{}`",node.member->name,cls->class_name));
            }
        } else{
            typeError(&node,fmt::format("basic class {} do not have member",cls->class_name));
        }
    } else {
        typeError(&node,fmt::format("MemberExpr object is not a class"));
    }
}
void TypeChecker::visit(parser::MethodCallExpr &node) {
    node.method->is_function_call = true;
    node.method->accept(*this);

    const auto func = dynamic_cast<FunctionDefType*>(node.method->inferredType);
    // assert(func != nullptr);
    if (func == nullptr) {
        node.inferredType = new ClassValueType("<None>");
        return;
    }

    if (node.args->size() != func->params->size() - 1) {
        typeError(&node, fmt::format("Expected {} arguments; got {}", func->params->size() - 1, node.args->size()));
        return;
    }

    for(int i = 0; i < node.args->size(); i++) {
        const auto arg = node.args->at(i);
        arg->accept(*this);
        const auto arg_type = arg->inferredType;
        const auto param_type = func->params->at(i + 1);
        if (!is_subtype(arg_type, param_type)) {
            typeError(&node, fmt::format("Expected type `{}`; got type `{}` in parameter {}", param_type->get_name(), arg_type->get_name(), i));
        }
    }

    node.inferredType = func->return_type;
}
void TypeChecker::visit(parser::NoneLiteral &node)
{
    node.inferredType = new ClassValueType("<None>");
}
void TypeChecker::visit(parser::ReturnStmt &node) {
    auto R = curr_func->return_type;
    SymbolType *T;
    if (node.value == nullptr) {
        T = new ClassValueType("<None>");
    } else {
        node.value->accept(*this);
        T = node.value->inferredType;
    }
    if (!is_subtype(T, R)) {
        typeError(&node, fmt::format("Expected type `{}`; got type `{}`", R->get_name(), T->get_name()));
    }
    if (node.value == nullptr) {
        delete T;
    }
}
void TypeChecker::visit(parser::StringLiteral &node)
{
    node.inferredType = new ClassValueType("str");
}
void TypeChecker::visit(parser::UnaryExpr &node)
{
    node.operand->accept(*this);
    if (parser::UnaryExpr::hashcode(node.operator_)==parser::UnaryExpr::operator_code::Minus) {
        auto t = dynamic_cast<ClassValueType*>(node.operand->inferredType);
        if(t==nullptr || t->class_name!="int") {
            typeError(&node,fmt::format("Cannot apply opreator {} on type {}",node.operator_,node.operand->inferredType->get_name()));
            node.inferredType = new ClassValueType("object");
            return;
        }
        node.inferredType=new ClassValueType("int");
    } else {
        auto t = dynamic_cast<ClassValueType*>(node.operand->inferredType);
        if(t==nullptr || t->class_name!="bool") {
            typeError(&node,fmt::format("Cannot apply opreator {} on type {}",node.operator_,node.operand->inferredType->get_name()));
            node.inferredType = new ClassValueType("Object");
            return;
        }
        node.inferredType=new ClassValueType("bool");
    }
}
void TypeChecker::visit(parser::VarDef &node)
{
    node.value->accept(*this);
    auto listType = dynamic_cast<parser::ListType*>(node.var->type);
    if(listType!=nullptr) {
        if (node.value->inferredType->get_name()!="<None>") {
            typeError(&node, fmt::format("Cannot use literal value type {} to declare list value {}",node.value->inferredType->get_name(),listType->get_name()));
        }
    } else {
        auto classType = dynamic_cast<parser::ClassType*>(node.var->type);
        assert(classType!=nullptr);
        auto valueType = ClassValueType(classType);
        if (!(classType->className=="object" || classType->className == node.value->inferredType->get_name() || (valueType.is_special_type() && node.value->inferredType->get_name()=="<None>"))) {
            typeError(&node, fmt::format("Expected type `{}`; got type `{}`",valueType.get_name(),node.value->inferredType->get_name()));
        }
    }
}
void TypeChecker::visit(parser::WhileStmt &node) {
    node.condition->accept(*this);
    for(auto s:*node.body) {
        s->accept(*this);
    }
    if (node.condition->inferredType->get_name()!="bool") {
        typeError(&node,fmt::format("Type of condition of if must be bool"));
    }
}
void TypeChecker::visit(parser::VarAssignStmt &node) {
    // The function seems useless.
    /*
    auto varType = sym->get<SymbolType*>(node.var->name);
    if (varType==nullptr) {
        typeError(&node, "Undefined variable: " + node.var->name);
        return;
    }
    node.value->accept(*this);
    auto *valueType = dynamic_cast<ValueType *>(this->passing_type);
    if (valueType==nullptr) {
        typeError(&node, "TypeError: Right of assignment is not a value");
    }*/
}
void TypeChecker::visit(parser::MemberAssignStmt &) {
    // The function seems useless.
}
void TypeChecker::visit(parser::IndexAssignStmt &) {
    // The function seems useless.
}
void TypeChecker::visit(parser::VarAssignExpr &) {
    // The function seems useless.
}
void TypeChecker::visit(parser::IndexAssignExpr &) {
    // The function seems useless.
}
void TypeChecker::visit(parser::AssignStmt& node) {
    node.value->accept(*this);
    auto T0=node.value->inferredType;
    bool is_error=false;
    if (node.targets->size()>1 && T0->get_name()=="[<None>]") {
        typeError(&node,"Right-hand side of multiple assignment may not be [<None>]");
        is_error=true;
    }
    for(auto s:*node.targets) {
        is_lvalue = true; s->accept(*this); is_lvalue = false;
        auto T = s->inferredType;
        if (is_error) continue;
        if (auto index = dynamic_cast<parser::IndexExpr*>(s); index!=nullptr && index->list->inferredType->get_name()=="str") {
            typeError(&node,"Cannot assign to string index");
        } else if (!is_subtype(T0, T)) {
            typeError(&node, fmt::format("Expected type `{}`; got type `{}`", T->get_name(), T0->get_name()));
            is_error=true;
        }
    }
}


void TypeChecker::debug_sym() {
    for (const auto &x : *sym->tab) {
        if ((x.second)->is_list_type()) {
            LOG(INFO) << x.first << " : [" << ((ClassValueType *)((ListValueType *)x.second)->element_type)->get_name()
                      << "]\n";
        } else if ((x.second)->is_func_type()) {
            if (dynamic_cast<FunctionDefType *>(x.second)) {
                LOG(INFO) << "Function " << x.first << " : [ "
                          << dynamic_cast<FunctionDefType *>(x.second)->return_type->get_name() << " ]";
                debug_nested_func_sym(((FunctionDefType *)x.second)->current_scope, 1);
            }
            for (const auto y : *dynamic_cast<FunctionDefType *>(x.second)->params)
                if (!y->is_list_type())
                    LOG(INFO) << "      : " << ((ClassValueType *)y)->get_name() << "\n";
                else
                    LOG(INFO) << "      : " << ((ClassValueType *)((ListValueType *)y)->element_type)->get_name()
                              << "\n";
        } else if ((x.second)->is_value_type()) {
            LOG(INFO) << x.first << " : " << ((ClassValueType *)x.second)->get_name() << "\n";
        }
    }
}

void TypeChecker::debug_nested_func_sym(SymbolTable *func_sym, int tab) {
    for (auto x : *func_sym->tab) {
        if ((x.second)->is_list_type()) {
            LOG(INFO) << fmt::format("{:>{}} : [ {} ]", x.first, tab * 20,
                                     ((ClassValueType *)((ListValueType *)x.second)->element_type)->get_name());
        } else if ((x.second)->is_func_type()) {
            if (dynamic_cast<FunctionDefType *>(x.second)) {
                LOG(INFO) << fmt::format("{:>{}} : [ {} ]", "Function " + x.first, tab * 20,
                                         dynamic_cast<FunctionDefType *>(x.second)->return_type->get_name());
                debug_nested_func_sym(((FunctionDefType *)x.second)->current_scope, tab++);
            }
            for (const auto y : *dynamic_cast<FunctionDefType *>(x.second)->params)
                if (!y->is_list_type())
                    LOG(INFO) << fmt::format("{:>{}}       : [ {} ]", "", tab * 20, ((ClassValueType *)y)->get_name());
                else
                    LOG(INFO) << fmt::format("{:>{}}       : [ {} ]", "", tab * 20,
                                             ((ClassValueType *)((ListValueType *)y)->element_type)->get_name());
        } else if ((x.second)->is_value_type()) {
            LOG(INFO) << fmt::format("{:>{}} : [ {} ]", x.first, tab * 20, ((ClassValueType *)x.second)->get_name());
        }
    }
}
/** Get the right type */
const string TypeChecker::get_common_type(SymbolType const * const first, SymbolType const * const second) {
    string tmp = first->get_name();
    list<string> first_dfs = {tmp};
    while (tmp != "object") {
        for (const auto &x : super_classes) {
            if (tmp == x.first) {
                first_dfs.push_back(x.second);
                tmp = x.second;
                break;
            }
        }
    }
    tmp = second->get_name();
    while (tmp != "object") {
        if (std::find(first_dfs.begin(), first_dfs.end(), tmp) != first_dfs.end()) {
            return tmp;
        }
        for (const auto &x : super_classes) {
            if (tmp == x.first) {
                tmp = x.second;
                break;
            }
        }
    }
    return "object";
}
SymbolType* TypeChecker::get_common_type_2(SymbolType * const x, SymbolType * const y) {
    if (is_subtype(x, y)) return y;
    if (is_subtype(y, x)) return x;
    if (x->is_list_type() || y->is_list_type()) return new ClassValueType("object");
    return new ClassValueType(get_common_type(x, y));
}
bool TypeChecker::is_subtype(SymbolType const *sub, SymbolType const *super) {
    // rule 3
    // <Empty> <= [T]
    if (super->is_list_type() && sub->get_name() == "<Empty>") return true;

    if (sub->is_list_type() && super->is_list_type()) {
        // [T] <= [T]
        if (sub->eq(super)) return true;
        // rule 4
        // [<None>] <= [T]
        const auto sub_e = dynamic_cast<ListValueType const *>(sub)->element_type;
        const auto super_e = dynamic_cast<ListValueType const *>(super)->element_type;
        if (sub_e->get_name() == "<None>") {
            sub = sub_e;
            super = super_e;
        } else {
            return false;
        }
    }

    // rule 2
    // <None> <= T
    if (sub->get_name() == "<None>" && super->get_name() != "int" && super->get_name() != "bool" && super->get_name() != "str")
        return true;

    // rule 1
    if (super->get_name() == "object") return true;
    if (sub->is_list_type() || super->is_list_type()) return false;
    return is_super_class(global, sub->get_name(), super->get_name());
}
bool TypeChecker::is_subtype(const string& sub_class_name, SymbolType const *super) {
    const auto sub = new ClassValueType(sub_class_name);
    return is_subtype(sub, super);
}
void TypeChecker::setup_num_to_class() {
    for (const auto &x : *sym->tab) {
        if (dynamic_cast<ClassDefType *>(x.second)) {
            auto tmp_dad = sym->get<ClassDefType *>(((ClassDefType *)x.second)->get_dad());
            super_classes[x.second->get_name()] = ((ClassDefType *)x.second)->get_dad();
            add_edge(((ClassDefType *)x.second)->get_dad(), x.second->get_name());
            while (tmp_dad != nullptr && tmp_dad->get_name() != "object" &&
                   !super_classes.contains(tmp_dad->get_name())) {
                add_edge(tmp_dad->get_dad(), tmp_dad->get_name());
                tmp_dad = sym->get<ClassDefType *>(tmp_dad->get_dad());
            }
        }
    }
    dfs("object");
    sym->class_tag_["list"] = -1;
    sym->class_tag_["object"] = 0;
    sym->class_tag_["int"] = 1;
    sym->class_tag_["bool"] = 2;
    sym->class_tag_["str"] = 3;
}


    ValueType *ValueType::annotate_to_val(parser::TypeAnnotation *annotation) {
    if (dynamic_cast<parser::ClassType *>(annotation)) {
        return new ClassValueType((parser::ClassType *)annotation);
    } else {
        if (annotation != nullptr && annotation->kind == "<None>")
            return new ClassValueType("<None>");
        if (dynamic_cast<parser::ListType *>(annotation))
            return new ListValueType((parser::ListType *)annotation);
    }
    return nullptr;
}

ListValueType::ListValueType(parser::ListType *typeAnnotation)
    : element_type(ValueType::annotate_to_val(typeAnnotation->elementType)) {}

ClassValueType::ClassValueType(parser::ClassType *classTypeAnnotation) : class_name(classTypeAnnotation->className) {}

cJSON *SemanticError::toJSON() {
    cJSON *d = parser::Err::toJSON();
    cJSON_AddStringToObject(d, "message", this->message.c_str());
    return d;
}
const string ValueType::get_name() const { return ((ClassValueType *)this)->class_name; }
} // namespace semantic

#ifdef PA2
int main(int argc, char *argv[]) {
    //To debug
    //std::freopen("bad_unary_expr.py","r",stdin);
    std::unique_ptr<parser::Program> tree(parse(argv[1]));

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

    cJSON *a = tree->toJSON();
    char *out = cJSON_Print(a);
    cout << out;
}
#endif