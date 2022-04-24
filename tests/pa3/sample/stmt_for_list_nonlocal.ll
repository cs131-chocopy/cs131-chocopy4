; ModuleID = 'ChocoPy code'
source_filename = "../tests/pa3/sample/stmt_for_list_nonlocal.py"

%$union.type = type {
  %$int$dispatchTable_type*,
  %$bool$dispatchTable_type*,
  %$str$dispatchTable_type*,
  %$object$dispatchTable_type*
}

%$union.len = type {
  %$.list$prototype_type*,
  %$str$prototype_type*
}

%$union.put = type {
  %$int$prototype_type*,
  %$bool$prototype_type*,
  %$str$prototype_type*
}

%$union.conslist = type {
  i32,
  i1,
  %$str$prototype_type*,
  %$.list$prototype_type*
}

%$object$prototype_type  = type  {
  i32,
  i32,
  %$object$dispatchTable_type*
}
@$object$prototype  = global %$object$prototype_type{
  i32 0,
  i32 3,
  %$object$dispatchTable_type* @$object$dispatchTable
}
%$object$dispatchTable_type = type {
  %$object$dispatchTable_type(%$object$dispatchTable_type)*
}
@$object$dispatchTable = global %$object$dispatchTable_type {
  %$object$dispatchTable_type(%$object$dispatchTable_type)* @$object.__init__
}

%$int$prototype_type  = type  {
  i32,
  i32,
  %$int$dispatchTable_type*,
  i32 
}
@$int$prototype  = global %$int$prototype_type{
  i32 1,
  i32 4,
  %$int$dispatchTable_type* @$int$dispatchTable,
  i32 0
}
%$int$dispatchTable_type = type {
  %$object$dispatchTable_type(%$object$dispatchTable_type)*
}
@$int$dispatchTable = global %$int$dispatchTable_type {
  %$object$dispatchTable_type(%$object$dispatchTable_type)* @$object.__init__
}

%$bool$prototype_type  = type  {
  i32,
  i32,
  %$bool$dispatchTable_type*,
  i1 
}
@$bool$prototype  = global %$bool$prototype_type{
  i32 2,
  i32 4,
  %$bool$dispatchTable_type* @$bool$dispatchTable,
  i1 0
}
%$bool$dispatchTable_type = type {
  %$object$dispatchTable_type(%$object$dispatchTable_type)*
}
@$bool$dispatchTable = global %$bool$dispatchTable_type {
  %$object$dispatchTable_type(%$object$dispatchTable_type)* @$object.__init__
}

%$str$prototype_type  = type  {
  i32,
  i32,
  %$str$dispatchTable_type*,
  i32 ,
  i8* 
}
@$str$prototype  = global %$str$prototype_type{
  i32 3,
  i32 5,
  %$str$dispatchTable_type* @$str$dispatchTable,
  i32 0,
  i8* inttoptr (i32 0 to i8*)
}
%$str$dispatchTable_type = type {
  %$object$dispatchTable_type(%$object$dispatchTable_type)*
}
@$str$dispatchTable = global %$str$dispatchTable_type {
  %$object$dispatchTable_type(%$object$dispatchTable_type)* @$object.__init__
}

%$.list$prototype_type  = type  {
  i32,
  i32,
  %$union.type ,
  i32 ,
  %$union.conslist* 
}
@$.list$prototype  = global %$.list$prototype_type{
  i32 -1,
  i32 5,
  %$union.type {%$int$dispatchTable_type* undef,  %$bool$dispatchTable_type* undef,  %$str$dispatchTable_type* undef,  %$object$dispatchTable_type* undef},
  i32 0,
  %$union.conslist* inttoptr (i32 0 to %$union.conslist*)
}

%$class.anon_make_z = type {i32* ,
  %$.list$prototype_type* 
}

@const_0 = external global %$bool$prototype_type
@const_1 = external global %$bool$prototype_type
@const_2 = external global %$str$prototype_type
@const_3 = external global %$str$prototype_type
@const_4 = external global %$str$prototype_type
@const_5 = external global %$str$prototype_type
@const_6 = external global %$str$prototype_type
@const_7 = external global %$str$prototype_type
@x = global i32 0
declare %$object$dispatchTable_type @$object.__init__(%$object$dispatchTable_type)
declare void @heap.init()
declare %$str$dispatchTable_type* @initchars(i8*)
declare %$int$dispatchTable_type* @noconv()
declare void @error.OOB()
declare void @error.None()
declare void @error.Div()
declare %$.list$prototype_type* @concat(%$.list$prototype_type*, %$.list$prototype_type*)
declare %$.list$prototype_type* @conslist(i32, %$union.conslist, ...)
declare i32 @$len(%$union.len*)
declare void @print(%$union.put*)
declare %$bool$prototype_type* @makebool(i1)
declare %$int$prototype_type* @makeint(i32)
declare %$str$prototype_type* @makestr(%$str$prototype_type*)
declare %$str$prototype_type* @$input()
declare i1 @streql(%$str$prototype_type*)
declare i1 @strneql(%$str$prototype_type*)
declare %$str$prototype_type* @strcat(%$str$prototype_type*, %$str$prototype_type*)
@llvm.global_ctors = appending global [1 x { i32, void ()*, i8* }] [{ i32, void ()*, i8* } { i32 65535, void ()* @before_main, i8* null }]
define void @before_main() {

label0:
  tail call void asm sideeffect "lui a0, 8192\0A	add s11, zero, a0", ""()
  call void @heap.init()
  tail call void asm sideeffect "mv s10, gp\0A	add s11, s11, s10\0A	mv fp, zero\0A	lw ra, 12(sp)\0A	addi sp, sp, 16\0A	ret", ""()

unreachable
}
define void @main() {

label0:
  tail call void asm sideeffect "addi fp, sp, 0", ""()
  %op2 = alloca i32
  store i32 1, i32* %op2
  %op3 = bitcast i32* %op2 to %$union.conslist*
  %op4 = load %$union.conslist, %$union.conslist* %op3
  %op5 = alloca i32
  store i32 2, i32* %op5
  %op6 = bitcast i32* %op5 to %$union.conslist*
  %op7 = load %$union.conslist, %$union.conslist* %op6
  %op8 = call %$.list$prototype_type* (i32, %$union.conslist, ...) @conslist(i32 2, %$union.conslist %op4, %$union.conslist %op7)
  %op9 = bitcast %$.list$prototype_type* %op8 to %$union.conslist*
  %op10 = load %$union.conslist, %$union.conslist* %op9
  %op11 = alloca i32
  store i32 2, i32* %op11
  %op12 = bitcast i32* %op11 to %$union.conslist*
  %op13 = load %$union.conslist, %$union.conslist* %op12
  %op14 = alloca i32
  store i32 3, i32* %op14
  %op15 = bitcast i32* %op14 to %$union.conslist*
  %op16 = load %$union.conslist, %$union.conslist* %op15
  %op17 = call %$.list$prototype_type* (i32, %$union.conslist, ...) @conslist(i32 2, %$union.conslist %op13, %$union.conslist %op16)
  %op18 = bitcast %$.list$prototype_type* %op17 to %$union.conslist*
  %op19 = load %$union.conslist, %$union.conslist* %op18
  %op20 = alloca i32
  store i32 4, i32* %op20
  %op21 = bitcast i32* %op20 to %$union.conslist*
  %op22 = load %$union.conslist, %$union.conslist* %op21
  %op23 = alloca i32
  store i32 5, i32* %op23
  %op24 = bitcast i32* %op23 to %$union.conslist*
  %op25 = load %$union.conslist, %$union.conslist* %op24
  %op26 = call %$.list$prototype_type* (i32, %$union.conslist, ...) @conslist(i32 2, %$union.conslist %op22, %$union.conslist %op25)
  %op27 = bitcast %$.list$prototype_type* %op26 to %$union.conslist*
  %op28 = load %$union.conslist, %$union.conslist* %op27
  %op29 = alloca i32
  store i32 6, i32* %op29
  %op30 = bitcast i32* %op29 to %$union.conslist*
  %op31 = load %$union.conslist, %$union.conslist* %op30
  %op32 = alloca i32
  store i32 7, i32* %op32
  %op33 = bitcast i32* %op32 to %$union.conslist*
  %op34 = load %$union.conslist, %$union.conslist* %op33
  %op35 = call %$.list$prototype_type* (i32, %$union.conslist, ...) @conslist(i32 2, %$union.conslist %op31, %$union.conslist %op34)
  %op36 = bitcast %$.list$prototype_type* %op35 to %$union.conslist*
  %op37 = load %$union.conslist, %$union.conslist* %op36
  %op38 = call %$.list$prototype_type* (i32, %$union.conslist, ...) @conslist(i32 4, %$union.conslist %op10, %$union.conslist %op19, %$union.conslist %op28, %$union.conslist %op37)
  call void @$crunch(%$.list$prototype_type* %op38)
  %op39 = load i32, i32*@x
  %op40 = call %$int$prototype_type* @makeint(i32 %op39)
  %op41 = bitcast %$int$prototype_type* %op40 to %$union.put*
  call void @print(%$union.put* %op41)
  tail call void asm sideeffect "li a7, 93 #exit system call\0Aecall", ""()
  ret void
}
define void @$crunch(%$.list$prototype_type* %arg0) {

label1:
  %op2 = alloca i32
  %op3 = alloca %$class.anon_make_z, align 4
  %op4 = getelementptr %$class.anon_make_z, %$class.anon_make_z* %op3, i32 0, i32 0
  store i32* %op2, i32** %op4
  %op5 = getelementptr %$class.anon_make_z, %$class.anon_make_z* %op3, i32 0, i32 1
  store %$.list$prototype_type* %arg0, %$.list$prototype_type** %op5
  call void @$crunch.make_z(%$class.anon_make_z* %op3)
  %op6 = load i32, i32*@x
  br label %label7

label7:                                                ; preds = %label1, %label13
  %op8 = phi i32 [ %op6, %label1 ], [ %op11, %label13 ]
  %op9 = icmp ne i32 %op6, %op8
  %op10 = getelementptr i32, i32* %op2, i32 %op8
  %op11 = add i32 %op8, 1
  %op12 = load i32, i32* %op10
  br label %label13

label13:                                                ; preds = %label7
  br  i1 %op9, label %label7, label %label14

label14:                                                ; preds = %label13
  ret void
}
define void @$crunch.make_z(%$class.anon_make_z* %arg0) {

label1:
  %op2 = getelementptr %$class.anon_make_z, %$class.anon_make_z* %arg0, i32 0, i32 0
  %op3 = getelementptr %$class.anon_make_z, %$class.anon_make_z* %arg0, i32 0, i32 1
  %op4 = load %$.list$prototype_type*, %$.list$prototype_type** %op3
  %op5 = alloca i32
  store i32 0, i32* %op5
  %op6 = bitcast %$.list$prototype_type* %op4 to %$union.len*
  %op7 = call i32 @$len(%$union.len* %op6)
  br label %label8

label8:                                                ; preds = %label1, %label17
  %op9 = phi i32 [ %op7, %label1 ], [ %op12, %label17 ]
  %op10 = icmp ne i32 %op7, %op9
  %op11 = getelementptr %$.list$prototype_type, %$.list$prototype_type* %op4, i32 0, i32 4
  %op12 = add i32 %op9, 1
  %op13 = load %$union.conslist*, %$union.conslist** %op11
  %op14 = bitcast %$union.conslist* %op13 to i32*
  %op15 = getelementptr i32, i32* %op14, i32 %op9
  %op16 = load i32, i32* %op15
  br label %label17

label17:                                                ; preds = %label8
  br  i1 %op10, label %label8, label %label18

label18:                                                ; preds = %label17
  ret void
}
