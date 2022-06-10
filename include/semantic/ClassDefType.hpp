//
// Created by yiwei yang on 2021/7/26.
//

#ifndef CHOCOPY_COMPILER_CLASSDEFTYPE_HPP
#define CHOCOPY_COMPILER_CLASSDEFTYPE_HPP

#include <iostream>
#include <utility>

#include "SymbolTable.hpp"
#include "ValueType.hpp"

using std::map;

namespace semantic {
class ClassDefType : public SymbolType {
   public:
    ClassDefType(string parent, string self)
        : super_class(std::move(parent)), class_name(std::move(self)) {
        this->current_scope = new SymbolTable;
    }
    ~ClassDefType() {
        for (const auto& e : inherit_members) {
            current_scope->tab->erase(e);
        }
        delete this->current_scope;
    }

    const string get_name() const override { return class_name; }
    void set_name(string_view class_name_) override {
        this->class_name = class_name_;
    }
    string get_type() const override {
        auto func_type = string(typeid(this).name()).substr(14);
        func_type.erase(remove(func_type.begin(), func_type.end(), 'E'),
                        func_type.end());
        return func_type;
    }
    string get_dad() const { return super_class; }
    std::vector<string> inherit_members;
    string super_class;
    string class_name;
    SymbolTable* current_scope;
};
}  // namespace semantic
#endif  // CHOCOPY_COMPILER_CLASSDEFTYPE_HPP
