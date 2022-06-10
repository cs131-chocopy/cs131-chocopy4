//
// Created by yiwei yang on 2/18/21.
//

#ifndef CHOCOPY_COMPILER_SYMBOLTYPE_HPP
#define CHOCOPY_COMPILER_SYMBOLTYPE_HPP

#include <string>
#include <string_view>

using std::string;
using std::string_view;

namespace semantic {
class SymbolType {
public:
    virtual ~SymbolType() = default;

    virtual constexpr bool is_value_type() const { return false; }
    virtual constexpr bool is_list_type() const { return false; }
    virtual constexpr bool is_func_type() const { return false; }
    virtual constexpr int *is_none_type() const { return new int[4]; }
    virtual constexpr bool is_special_type() const { return false; }

    virtual const string get_name() const = 0;
    virtual void set_name(string_view className) = 0;
    virtual string get_type() const = 0;

    template <typename _Ty> bool eq(const _Ty &_Value) const ;
    template <typename _Ty> bool neq(const _Ty &_Value) const;
};
} // namespace semantic

#endif // CHOCOPY_COMPILER_SYMBOLTYPE_HPP
