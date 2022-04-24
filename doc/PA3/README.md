# Programing Assignment III 文档

<!-- TOC -->

- [Lab2 实验文档](#lab3-实验文档)
    - [0. 基础知识](#0-基础知识)
        - [0.1 LightIR ScopeAnalyzer](#01-LightIR-ScopeAnalyzer)
        - [0.1 LightIR Predefined part](#01-LightIR-Predefined-part)
        - [0.2 LightIR class print](#02-LightIR-class-print)
        - [0.3 LightIR emission](#03-LightIR-emission)
    
        - [1. 实验要求](#1-实验要求)
            - [1.1 目录结构](#11-目录结构)
            - [1.2 实验目的](#12-实验目的)
            - [1.3 编译、运行和验证](#13-编译运行和验证)
            - [1.4 提交要求和评分标准](#14-提交要求和评分标准)

<!-- /TOC -->

# lab3-实验文档

本次实验是组队实验，请仔细阅读组队要求，并合理进行分工合作。 本次实验和 Lab3 一样，需要使用 `LightIR` 框架自动产生 `ChocoPy` 语言的LLVM IR。 建议在实验前队伍内进行讨论，以确保成员都理解 Lab3
中的问题。

相信大家已经掌握了 LightIR 的使用，并且对于 LLVM IR 也有了一定理解。在本次实验中，我们要使用访问者模式来实现 IR 自动生成。 对于产生的IR， 我们可以调用 clang 生成可执行文件，这样一个初级的 ChocoPy
编译器就完成啦！

### 主要工作

1. 阅读[LightIR 核心类介绍](../common/LightIR.md)
2. 阅读[实验框架](#1-实验框架)，理解如何使用框架以及注意事项
3. 修改 `src/cminusfc/cminusf_builder.cpp` 来实现自动 IR 产生的算法，使得它能正确编译任何合法的 cminus-f 程序
4. 在 `report.md` 中解释你们的设计，遇到的困难和解决方案
5. 由**队长**在 `contribution.md` 中解释每位队员的贡献，并说明贡献比例

注意：组队实验意味着合作，但是小组间的交流是受限的，且严格**禁止**代码的共享。除此之外，如果小组和其它组进行了交流，必须在 `report.md` 中记录下来交流的小组和你们之间交流内容。

# 0. 基础知识

## Light IR class print

## Light IR class print

Class代码框架打印部分需要大家来完成。

### 1.1 目录结构

### 1.2 实验目的

#### 几点说明
由于 LLVM IR 是强类型的
有很多部分和伯克利生成的代码不太一样
1. char* 还是 .string
2. 不使用 frame pointer 回滚 function local 变量。