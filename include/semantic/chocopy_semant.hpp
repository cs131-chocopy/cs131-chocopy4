//
// Created by yiwei yang on 2021/7/26.
//

#ifndef CHOCOPY_COMPILER_CHOCOPY_SEMANT_HPP
#define CHOCOPY_COMPILER_CHOCOPY_SEMANT_HPP

#include <cassert>
#include <chocopy_logging.hpp>
#include <map>
#include <memory>
#include <set>
#include <stack>
#include <string>

#include "ClassDefType.hpp"
#include "FunctionDefType.hpp"
#include "SymbolTable.hpp"
#include "SymbolType.hpp"
#include "ValueType.hpp"

using std::stack;
using std::string;

namespace parser {
class AssignStmt;
class BinaryExpr;
class BoolLiteral;
class CallExpr;
class ClassDef;
class ClassType;
class Decl;
class CompilerErr;
class Expr;
class ExprStmt;
class ForStmt;
class FuncDef;
class GlobalDecl;
class Ident;
class IfExpr;
class IndexExpr;
class IntegerLiteral;
class ListExpr;
class ListType;
class Literal;
class MemberExpr;
class MethodCallExpr;
class Node;
class NoneLiteral;
class NonlocalDecl;
class Program;
class ReturnStmt;
class Stmt;
class StringLiteral;
class TypeAnnotation;
class TypedVar;
class UnaryExpr;
class VarDef;
class PassStmt;
class IfStmt;
class Err;
class WhileStmt;
class VarAssignStmt;
class MemberAssignStmt;
class IndexAssignStmt;
class VarAssignExpr;
class MemberAssignExpr;
class IndexAssignExpr;

}  // namespace parser

using std::map;
using std::string;

namespace ast {
class Visitor;
class ASTAnalyzer : public Visitor {
   public:
    virtual void visit(parser::BinaryExpr &node){};
    virtual void visit(parser::Node &node){};
    virtual void visit(parser::Err &node){};
    virtual void visit(parser::PassStmt &node){};
    virtual void visit(parser::BoolLiteral &node){};
    virtual void visit(parser::CallExpr &node){};
    virtual void visit(parser::ClassDef &node){};
    virtual void visit(parser::ClassType &node){};
    virtual void visit(parser::ExprStmt &node){};
    virtual void visit(parser::ForStmt &node){};
    virtual void visit(parser::FuncDef &node){};
    virtual void visit(parser::GlobalDecl &node){};
    virtual void visit(parser::Ident &node){};
    virtual void visit(parser::IfExpr &node){};
    virtual void visit(parser::IfStmt &node){};
    virtual void visit(parser::IndexExpr &node){};
    virtual void visit(parser::IntegerLiteral &node){};
    virtual void visit(parser::ListExpr &node){};
    virtual void visit(parser::ListType &node){};
    virtual void visit(parser::MemberExpr &node){};
    virtual void visit(parser::MethodCallExpr &node){};
    virtual void visit(parser::NoneLiteral &node){};
    virtual void visit(parser::NonlocalDecl &node){};
    virtual void visit(parser::ReturnStmt &node){};
    virtual void visit(parser::StringLiteral &node){};
    virtual void visit(parser::TypedVar &node){};
    virtual void visit(parser::UnaryExpr &node){};
    virtual void visit(parser::VarDef &node){};
    virtual void visit(parser::WhileStmt &node){};
    virtual void visit(parser::VarAssignStmt &){};
    virtual void visit(parser::MemberAssignStmt &){};
    virtual void visit(parser::IndexAssignStmt &){};
    virtual void visit(parser::VarAssignExpr &){};
    virtual void visit(parser::MemberAssignExpr &){};
    virtual void visit(parser::IndexAssignExpr &){};
    virtual void visit(parser::TypeAnnotation &){};
    virtual void visit(parser::AssignStmt &node){};
    virtual void visit(parser::Program &node){};
};

}  // namespace ast

namespace semantic {

class SemanticError : public parser::Err {
   public:
    SemanticError(Node *node, const string &message)
        : Err(nullptr, false), message(message) {
        if (node->location != nullptr) {
            this->location = node->get_location();
        }
        ((parser::Node *)this)->error_msg = message;
        ((parser::Node *)this)->kind = "SemanticError";
    }

    cJSON *toJSON() override;
    string message;
};

/** Analyzer that performs ChocoPy type checks on all nodes.  Applied after
 *  collecting declarations. */
class TypeChecker : public ast::ASTAnalyzer {
   public:
    void visit(parser::BinaryExpr &node) override;
    void visit(parser::BoolLiteral &node) override;
    void visit(parser::CallExpr &node) override;
    void visit(parser::ClassDef &node) override;
    void visit(parser::ExprStmt &node) override;
    void visit(parser::ForStmt &node) override;
    void visit(parser::FuncDef &node) override;
    void visit(parser::Ident &node) override;
    void visit(parser::IfStmt &node) override;
    void visit(parser::IfExpr &node) override;
    void visit(parser::IndexExpr &node) override;
    void visit(parser::IntegerLiteral &node) override;
    void visit(parser::ListExpr &node) override;
    void visit(parser::MemberExpr &node) override;
    void visit(parser::MethodCallExpr &node) override;
    void visit(parser::NoneLiteral &node) override;
    void visit(parser::ReturnStmt &node) override;
    void visit(parser::StringLiteral &node) override;
    void visit(parser::UnaryExpr &node) override;
    void visit(parser::VarDef &node) override;
    void visit(parser::WhileStmt &node) override;
    void visit(parser::VarAssignStmt &) override;
    void visit(parser::MemberAssignStmt &) override;
    void visit(parser::IndexAssignStmt &) override;
    void visit(parser::VarAssignExpr &) override;
    void visit(parser::IndexAssignExpr &) override;
    void visit(parser::Program &node) override;
    void visit(parser::AssignStmt &node) override;

    /** type checker attributes and their chocopy typing judgement analogues:
     * O : symbolTable
     * M : classes
     * C : currentClass
     * R : expReturnType */
    TypeChecker(SymbolTable *globalSymbols, vector<parser::Err *> *errors0) {
        this->sym = globalSymbols;
        this->global = globalSymbols;
        this->errors = errors0;
        setup_num_to_class();
        debug_sym();
    }
    /** Inserts an error message in NODE if there isn"t one already.
     *  The message is constructed with MESSAGE and ARGS as for
     *  String.format. */
    template <typename... Args>
    void typeError(parser::Node *node, const string &message, Args... rest);
    static void typeError(parser::Node *node, const string &message);

    /** The current symbol table (changes depending on the function
     *  being analyzed). */
    SymbolTable *sym;
    stack<SymbolTable *> saved{};
    SymbolTable *global;

    /** For the nested function declaration */
    FunctionDefType *curr_func = nullptr;
    std::vector<std::string>* curr_lambda_params{nullptr};
    stack<FunctionDefType *> saved_func{};

    SymbolType *passing_type{};
    bool is_lvalue{false};

    /** Collector for errors. */
    vector<parser::Err *> *errors;

    /** set up default class hierarchy
     * <None> <= object
     * <Empty> <= object
     * <None> <= <None>
     * <Empty> <= <Empty>
     */
    map<string, string> super_classes = {
        {"int", "object"},   {"bool", "int"},      {"none", "object"},
        {"empty", "object"}, {"<None>", "object"}, {"<Empty>", "object"}};

    const string get_common_type(SymbolType const * const first, SymbolType const * const second);
    // The function can check both ClassType and ListType
    SymbolType* get_common_type_2(SymbolType * const, SymbolType * const);
    bool is_subtype(SymbolType const *, SymbolType const *);
    bool is_subtype(const string&, SymbolType const *);
    void setup_num_to_class();

    /** linear-list-stored graph for the object graph
     *
     * OBJECT 0
     * INT1 BOOL2 STR3 LIST4 A5   F10 ....
     *                       | \
     *                      B6 E9
     *                      | \
     *                      C7 D8
     */

    /** initialize the edge */
    void add_edge(const string &a, const string &b) {
        if (!sym->head.count(a)) sym->head[a] = -1;
        if (!sym->head.count(b)) sym->head[b] = -1;
        sym->way.emplace_back(SymbolTable::edge{b, sym->head[a]});
        sym->head[a] = sym->way.size() - 1;
    }
    int dfn = -1;
    /** depth first search */
    void dfs(const string &x) {
        sym->lhs[x] = ++dfn;
        sym->nametable.emplace_back(x);
        for (int y = sym->head[x]; y != -1; y = sym->way[y].pre) {
            dfs(sym->way[y].target);
        }
        sym->rhs[x] = dfn;
        if (x != "object")
            sym->class_tag_[x] = sym->lhs[x] + 3;
        else
            sym->class_tag_[x] = sym->lhs[x];
    }

    void debug_sym();
    void debug_nested_func_sym(SymbolTable *func_sym, int tab);
};

class SymbolTableGenerator : public ast::ASTAnalyzer {
   public:
    SymbolTableGenerator(vector<parser::Err *> *e) : errors(e) {

        auto *foo = new ClassDefType("", "object");
        auto *init = new FunctionDefType();
        init->func_name = "__init__";
        init->return_type = new ClassValueType("<None>");
        init->params = new std::vector<SymbolType *>();
        init->params->emplace_back(new ClassValueType("object"));
        foo->current_scope->tab->insert({"__init__", init});
        sym->tab->insert({"object", foo});

        foo = new ClassDefType("object", "str");
        init = new FunctionDefType();
        init->func_name = "__init__";
        init->return_type = new ClassValueType("<None>");
        init->params = new std::vector<SymbolType *>();
        init->params->emplace_back(new ClassValueType("str"));
        foo->current_scope->tab->insert({"__init__", init});
        sym->tab->insert({"str", foo});

        foo = new ClassDefType("object", "int");
        init = new FunctionDefType();
        init->func_name = "__init__";
        init->return_type = new ClassValueType("<None>");
        init->params = new std::vector<SymbolType *>();
        init->params->emplace_back(new ClassValueType("int"));
        foo->current_scope->tab->insert({"__init__", init});
        sym->tab->insert({"int", foo});

        foo = new ClassDefType("object", "bool");
        init = new FunctionDefType();
        init->func_name = "__init__";
        init->return_type = new ClassValueType("<None>");
        init->params = new std::vector<SymbolType *>();
        init->params->emplace_back(new ClassValueType("bool"));
        foo->current_scope->tab->insert({"__init__", init});
        sym->tab->insert({"bool", foo});

        auto bar = new FunctionDefType();
        bar->func_name = "len";
        bar->return_type = new ClassValueType("int");
        bar->params = new std::vector<SymbolType *>();
        bar->params->emplace_back(new ClassValueType("object"));
        sym->tab->insert({"len", bar});

        bar = new FunctionDefType();
        bar->func_name = "print";
        bar->return_type = new ClassValueType("<None>");
        bar->params = new std::vector<SymbolType *>();
        bar->params->emplace_back(new ClassValueType("object"));
        sym->tab->insert({"print", bar});

        bar = new FunctionDefType();
        bar->func_name = "input";
        bar->return_type = new ClassValueType("str");
        bar->params = new std::vector<SymbolType *>();
        sym->tab->insert({"input", bar});
    }
    void visit(parser::Program &) override;
    void visit(parser::ClassDef &) override;
    void visit(parser::VarDef &) override;
    void visit(parser::FuncDef &) override;
    void visit(parser::NonlocalDecl &) override;
    void visit(parser::GlobalDecl &) override;

    SymbolType *ret = nullptr;
    std::unique_ptr<SymbolTable> globals = std::make_unique<SymbolTable>();
    SymbolTable *sym = globals.get();
    vector<parser::Err *> *errors;
    vector<parser::Node *> ignore;
};

/**
 * Analyzes declarations to create a top-level symbol table
 */
class DeclarationAnalyzer : public ast::ASTAnalyzer {
   public:
    void visit(parser::ClassDef &node) override;
    void visit(parser::FuncDef &node) override;
    void visit(parser::GlobalDecl &node) override;
    void visit(parser::NonlocalDecl &node) override;
    void visit(parser::VarDef &varDef) override;
    void visit(parser::Program &program) override;

    explicit DeclarationAnalyzer(vector<parser::Err *> *errors,
                                 const std::vector<parser::Node *> &ignore,
                                 std::unique_ptr<SymbolTable> globals) {
        this->errors = errors;
        for (const auto x : ignore) ignore_set.insert(x);
        this->globals = std::move(globals);
        sym = this->globals.get();
    }

    /** Collector for errors. */
    vector<parser::Err *> *errors;

    std::unique_ptr<SymbolTable> globals;

   private:
    bool ignore(parser::Node *node) { return ignore_set.contains(node); }
    std::set<parser::Node *> ignore_set{};
    void debug_sym();
    ClassDefType *current_class = nullptr;
    SymbolTable *sym;
    ClassDefType *getClass(const string &name) {
        return globals->declares<ClassDefType *>(name);
    }
    void checkShadowClass(const string &name, parser::Ident *id) {
        if (getClass(name)) {
            errors->emplace_back(
                new SemanticError(id, "Cannot shadow class name: " + name));
        }
    }
    void checkValueType(ValueType *type, parser::TypeAnnotation *annoation) {
        while (dynamic_cast<ListValueType *>(type)) {
            type = ((ListValueType *)type)->element_type;
        }
        assert(dynamic_cast<ClassValueType *>(type));
        const auto &class_name = ((ClassValueType *)type)->class_name;

        if (getClass(class_name) == nullptr) {
            errors->emplace_back(new SemanticError(
                annoation,
                "Invalid type annotation; there is no class named: " +
                    class_name));
        }
    }
};

// check if b is super class of a
inline bool is_super_class(SymbolTable* sym,string a,string b) {
    if (b=="object") {
        return true;
    }
    auto p=a;
    while(p!="object" && p!=b) {
        if (auto t=sym->get<ClassDefType*>(p); t!=nullptr) {
            p=t->super_class;
        } else {
            //throw(string("Error: super class is not a class!"));
            return false;
        }
    }
    return (p==b);
}

}  // namespace semantic
#endif  // CHOCOPY_COMPILER_CHOCOPY_SEMANT_HPP
