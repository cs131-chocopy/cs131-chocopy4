//
// Created by yiwei yang on 2/16/21.
//

#ifndef CHOCOPY_COMPILER_VALUETYPE_HPP
#define CHOCOPY_COMPILER_VALUETYPE_HPP

#include "SymbolType.hpp"
#include <chocopy_ast.hpp>
#include <chocopy_logging.hpp>
#include <cjson/cJSON.h>
#include <string>
#include <utility>

using std::string;

namespace parser {
class TypeAnnotation;
class ClassType;
class ListType;
} // namespace parser

namespace semantic {
class ClassValueType;
class ListValueType;

/**
 * @brief A tempelate class for nonlocal and global. Will be replaced after DeclarationAnalyzer.
 */
class RefType : public SymbolType {
    public:
        RefType(const string& name) : name(name) {}

        const string get_name() const override { return name; }
        void set_name(string_view className) override { name = className; }
        string get_type() const override { return ""; };

        string name;
};
class NonlocalRefType : public RefType {
    public:
        NonlocalRefType(const string& name) : RefType(name) {}
};
class GlobalRefType : public RefType {
    public:
        GlobalRefType(const string& name) : RefType(name) {}
};

/**
 * A ValueType references types that are assigned to variables and
 * expressions.
 *
 * In particular, ValueType can be a {@link ClassValueType} (e.g. "int") or
 * a {@link ListValueType} (e.g. "[int]").
 */
class ValueType : public SymbolType {
public:
    ValueType() = default;

    bool is_value_type() const override { return true; }

    static ValueType *annotate_to_val(parser::TypeAnnotation *annotation);

    const string get_name() const override;
};

/**
 * A ListType references types that are assigned to variables and
 * expressions.  (e.g. "[int]").
 */
class ListValueType : public ValueType {
public:
    explicit ListValueType(ValueType *element) { this->element_type = element; }

    explicit ListValueType(parser::ListType *typeAnnotation);

    bool constexpr is_list_type() const override { return true; }

    const string get_name() const override { return "[" + this->element_type->get_name() + "]"; }

    void set_name(string_view className) override {}

    string get_type() const override {
        auto func_type = string(typeid(this).name()).substr(14);
        func_type.erase(remove(func_type.begin(), func_type.end(), 'E'), func_type.end());
        return func_type;
    }

    ValueType *element_type;
};
/**
 * A ClassValueType references types that are assigned to variables and
 * expressions.  (e.g. "int").
 */
class ClassValueType : public ValueType {
public:
    explicit ClassValueType(string className) { this->class_name = std::move(className); }

    explicit ClassValueType(parser::ClassType *classTypeAnnotation);

    ClassValueType() = default;

    bool is_special_type() const override {
        return this->class_name != "str" && this->class_name != "int" && this->class_name != "list" &&
               this->class_name != "bool";
    };
    const string get_name() const override { return class_name; }

    void set_name(string_view className) override { this->class_name = className; }

    string get_type() const override {
        auto func_type = string(typeid(this).name()).substr(14);
        func_type.erase(remove(func_type.begin(), func_type.end(), 'E'), func_type.end());
        return func_type;
    }

    string class_name;
};

} // namespace semantic
#endif // CHOCOPY_COMPILER_VALUETYPE_HPP
