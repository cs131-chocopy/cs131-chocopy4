# ChocoPy PA4

student1: huab@shanghaitech.edu.cn
student2: zhangych6@shanghaitech.edu.cn

Compile the LightIR code to RiscV Assembly. [Start from using docker](./doc/common/build.md) and see the [doc](./doc/PA4/README.md).

## writeup

这次作业的难点主要在于实现符合 RISC-V Calling Convention 的函数和寄存器分配。

在实现二元运算和其他一些指令时，我们参考了 The RISC-V Instruction Set Manual 和 RISC-V rv32gc clang 生成的代码。

#### 遇到的一些困难和解决方案

1. 项目开始时，因为没有学过 CA1，不知道汇编怎么写，不知道每个函数的预期作用，没有一个简单的实例指导怎么写代码。解决方案：通过看 chocopy.org 产生的 RISC-V 汇编，大致理解任务。
2. stdlib 和 PA3 不同。
3. stdlib 不符合 RISC-V Calling Convention.
4. 不了解 memory align 的作用，导致 myprintf.c 不正常工作。解决方案：使用 p2align 指令。
5. PA3 代码只保证了输出 LLVM-IR 的效果，导致必须使用 std::string 来做索引，开销比较大。
6. RiscVBackEnd 框架一些指令生成有问题。

#### 实现了什么

在 commit #fb2c2003 我们实现了一个简单的 stack machine，只使用了 t0, t1, t2 寄存器，全部变量都分配在栈上。

在 commit #45fc1a9d 我们实现了 liveness analysis 和 linear scan 寄存器分配。
实现的难点在于函数调用需要按需保存 caller save 寄存器，函数需要按需保存 callee save 寄存器。

####  代码质量

1. 删除无用代码。
2. 使用断言 assert，减少查错时间。
3. 合并类似代码，比如 add, sub, mul, div, rem 实现都很类似，放到一个 switch case 中。
4. 使用 `[[nodiscard]]` attribute 增加编译器的警告信息，防止犯错。
5. 写注释和调试输出，帮助队友了解工作。
6. 使用 lambda function，使代码更容易理解。

#### 内存管理

本次作业没有显示使用指针，全部 RAII.

#### 如何 Debug

使用 `/llvm/bin/riscv64-unknown-elf-gdb` 和 `qemu-riscv32 -g`，配合程序中间输出的信息可以高效 debug.
