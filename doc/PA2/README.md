# Programing Assignment II 文档

<!-- TOC -->

- [Programing Assignment II 文档](#programing-assignment-ii-文档)
    - [0. 基础知识](#0-基础知识)
        - [0.1 Declaration 检查](#01-declaration-检查)
            - [0.1.1 全局变量检查](#011-全局变量检查)
            - [0.1.2 for/while变量检查](#012-forwhile变量检查)
            - [0.1.3 函数内变量检查](#013-函数内变量检查)
            - [0.1.4 Class变量检查](#014-class变量检查)
    - [0.2 Type Checker 检查](#02-type-checker-检查)
        - [0.2.1 类型语义](#021-类型语义)
    - [1. 实验要求](#1-实验要求)
        - [主要工作](#主要工作)
        - [提示](#提示)
        - [1.1 目录结构](#11-目录结构)
        - [1.2 Bonus](#12-bonus)
        - [1.2 编译、运行和验证](#12-编译运行和验证)

<!-- /TOC -->

在本次实验是个人项目。本实验中需要使用`Visitor Pattern`完成对程序的 Declaration Analysis 和 Type Checker Analysis。 Declaration的结果以 Symbol table 的形式传给Type
Checker继续检查。从而使`Chocopy`的LSP没有语义错误。

## 0. 基础知识

### 0.1 Declaration 检查

Declaration 检查是一个申明作用域环境的检查，在python中的变量有四个作用域

| 作用域            | 英文解释                      | 英文简写 |
|----------------|---------------------------|------|
| 局部作用域（函数内）     | Local(function)           | L    |
| 外部嵌套函数作用域      | Enclosing function locals | E    |
| 函数定义所在模块作用域    | Global(module)            | G    |
| python内置模块的作用域 | Builtin                   | B    |

在访问变量时，先查找本地变量，然后是包裹此函数外部的函数内的变量，之后是全局变量 最后是內建作用域内的变量 即： L –> E -> G -> B

#### 0.1.1 全局变量检查

函数内定义的变量，在函数内部可以访问，但是不能访问函数外部的变量。global 关键字可以用来声明一个全局变量。

```python
x:int = 0
def f():
    x = 1
    print(x)
```
#### 0.1.2 for/while变量检查
对于for/while循环，在循环体内部定义的变量，在循环体内部可以访问，但是不能访问循环体外部的变量。in 关键字可以用来声明一个循环变量。in 前的循环变量可以与符号表的变量重名，在循环内是c++ rewrite的语义。

```python
x:str = ""
y:str = "123"
z:str = "abc"

for x in z:
    print(x)
    for x in y:
        print(x)
```

#### 0.1.3 函数内变量检查

Python 支持嵌套定义函数，每次进入函数时需要进入函数本地变量的scope，同时对外部的E/G/B所在定义aware，不能重名，如有调用需要指向外部的symbol。

```python
x:int = 0
def crunch(zz:[[int]]) -> object:
    z:[int] = None
    global x
    def make_z() -> object: # 嵌套函数 z为左值未定义，zz为调用右值，直接访问
        # 需要添加 nonlocal z 若没有此行需要报错
        for z in zz:
            pass # Set z to last element in zz
    make_z()
    for x in z:
        pass # Set x to last element in z
crunch([[1,2],[2,3],[4,5],[6,7]])
print(x) # 嵌套内部的变量修改在外部能看到 类似 c++ [&] lambda 语义
```

#### 0.1.4 Class变量检查

Python 会检查成员变量

```python
class A(object):
    x:int = 1

    def foo(self:"A") -> int:
        return 0

class B(A):
    x:int = 2  # Bad
    foo:str = "" # Bad

A()
```

## 0.2 Type Checker 检查

### 0.2.1 类型语义



## 1. 实验要求

本实验的输出遵循的是类[lsp](https://github.com/MaskRay/ccls/blob/master/src/lsp.hh)
protocol，如你们在web上测试的情况可知，这种json的传输协议很适合作为前后端现实的交互接口，VSCode等IDE也使用相关protocol进行前端高亮。

本次实验需要各位同学根据`ChocoPy`的语义补全[chocopy_semant.y](./src/semantic/chocopy_semant.cpp)
文件，完成完整的语法分析器，能够输出识别出的`token`，`type` ,`line_start(刚出现的行数)`，`pos_start(该行开始位置)`，`line_end(结束的行数)`, `pos_end(结束的位置,不包含)`
。如：

### 主要工作

1. 了解 `visitor` 模式基础知识和理解 ChocoPy 语义（重在了解如何在 `visitor` 模式下）
2. 阅读 `./src/parser/chocopy_ast.cpp`以及 `./src/semantic/chocopy_semant.cpp`（重在理解分析树的使用）
3. 了解 `./src/semantic/chocopy_semant.cpp` 中的语义检查，并完成语义检查
4. 补全 `./src/semantic/chocopy_semant.cpp` 文件，以及如果你需要其他改写的代码可以自行改写。

### 提示

文本输入：

```c
 a: int = 1
```

则对应语义检查结果应为：

```shell

```

对语义的抽象语法树输出文件如下，注意Type Error与Declaration Error输出位置的区别。

```json

```

如果有Declaration错误，对应的errors为`SemanticError`。如果有Type错误，对应在语法树节点上有`TypeError`。 检测程序不会检查错误信息与个数，所以不需要考虑错误的贪心信息，可以找到一个不可接收的程序直接报错返回。

**具体的需输出的Error和细节的要求参考[chocopy.y](.//doc/berkeley/PA2.pdf)

**特别说明：对于数字上的UnaryExpr -，输出负数和Expr+一个数都可以

### 1.1 目录结构

详见[common/structure.md](./doc/common/structure.md)

### 1.2 Bonus

错误信息能与tests完全一致[10pts]

### 1.2 编译、运行和验证

* 编译

  若编译成功，则将在 `./[build_dir]/` 下生成 `semantic` 命令。

* 运行

  本次实验的 `semantic` 命令使用 shell 的输入重定向功能，即程序本身使用标准输入输出（stdin 和 stdout），但在 shell 运行命令时可以使用 `<` `>` 和 `>>` 灵活地自定义输出和输入从哪里来。

  ```shell
  $ cd chocopy
  $ ./build/semantic               # 交互式使用（不进行输入重定向）
  <在这里输入 ChocoPy代码，如果遇到了错误，将程序将报错并退出。>
  <输入完成后按 ^D 结束输入，此时程序将输出解析json。>
  $ ./build/semantic < test.py # 重定向标准输入
  <此时程序从 test.py 文件中读取输入，因此不需要输入任何内容。>
  <如果遇到了错误，将程序将报错并输出错误json；否则，将输出解析json。>
  $ ./build/semantic test.py  # 不使用重定向，直接从 test.py 中读入
  $ ./build/semantic < test.py > out
  <此时程序从 test.py 文件中读取输入，因此不需要输入任何内容。>
  ```

  通过灵活使用重定向，可以比较方便地完成各种各样的需求，请同学们务必掌握这个 shell 功能。

* 验证

  本次试验测试案例较多，为此我们将这些测试分为两类：

    1. sample: 这部分测试均比较简单且单纯，适合开发时调试。
    2. fuzz: 由fuzzer生成的正确的python文件，此项不予开源。

  我们使用python中的 `json.load()` 命令进行验证。将自己的生成结果和助教提供的 `xxx.typed` 进行比较。

  ```shell
  $ python3 ./duipai.py --pa 2
  # 如果结果完全正确，则全 PASS，且有分数提示，一个正确的case 1 pts，此项评分按比例算入总评。选择chocopy的同学会在project部分分数上*1.2计入总评。
  # 如果有不一致，则会汇报具体哪个文件哪部分不一致，且有详细输出。
  ```

  **请注意助教提供的`testcase`并不能涵盖全部的测试情况，完成此部分仅能拿到基础分，请自行设计自己的`testcase`进行测试。**
  
