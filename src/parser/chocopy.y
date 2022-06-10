%locations

%{
#include <cstdio>
#include <cstdarg>
#include <vector>
#include <string>
#include <iostream>
#include <chocopy_parse.hpp>
#include <chocopy_ast.hpp>

#define yylloc2array (new int[4]{yylloc.first_line, yylloc.first_column, yylloc.last_line, yylloc.last_column-1})

int* mergelocation(int *a, int *b) { return new int[4]{a[0],a[1],b[2],b[3]}; }
int* mergelocation(int *a, parser::Node *b) { return new int[4]{a[0],a[1],b->location[2],b->location[3]}; }
int* mergelocation(parser::Node *a, int *b) { return new int[4]{a->location[0],a->location[1],b[2],b[3]}; }
int* mergelocation(parser::Node *a, parser::Node *b) { return new int[4]{a->location[0],a->location[1],b->location[2],b->location[3]}; }

int* set_backlocation(int *loc, int *b) { loc[2] = b[2], loc[3] = b[3]; return loc; }
int* set_backlocation(int *loc, parser::Node *b) { loc[2] = b->location[2], loc[3] = b->location[3]; return loc; }
int* set_frontlocation(int *loc, parser::Node *a) { loc[0] = a->location[0]; loc[1] = a->location[1]; return loc; }
int* set_frontlocation(int *loc, int *a) { loc[0] = a[0]; loc[1] = a[1]; return loc; }

int* reuse_location(int *loc, int* a, int *b) { loc[0] = a[0]; loc[1] = a[1]; loc[2] = b[2], loc[3] = b[3]; return loc; }
int* reuse_location(int *loc, parser::Node* a, parser::Node *b) { loc[0] = a->location[0]; loc[1] = a->location[1]; loc[2] = b->location[2], loc[3] = b->location[3]; return loc; }


/** external functions from lex */
extern void yyrestart(FILE*);
extern int yylex();
extern int yyparse();
extern FILE* yyin;
parser::Program *ROOT = new parser::Program(new int[4]{0, 0, 0, 0});

typedef struct yyltype {
    uint32_t first_line;
    uint32_t first_column;
    uint32_t last_line;
    uint32_t last_column;
} yyltype;

/** Error reporting */
void yyerror(const char *s);


/** Return a mutable list initially containing the single value ITEM. */
template<typename T>
std::vector<T *>* single(T *item) {
    std::vector<T *> *list=new std::vector<T *>();
    list->push_back(item);
    return list;
}

/* If ITEM is non-null, appends it to the end of LIST.  Then returns LIST. Generic is deprecated */
template<typename T>
std::vector<parser::Decl *>* combine(std::vector<parser::Decl *>* list, T *item) {
    list->push_back(item);
    return list;
}


std::vector<parser::Stmt *>* combine(std::vector<parser::Stmt *>* list, parser::Stmt *item) {
    list->push_back(item);
    return list;
}

parser::ListExpr* combine(parser::ListExpr *list, parser::Expr *item) {
    list->elements->push_back(item);
    return list;
}

std::vector<parser::Expr *>* combine(std::vector<parser::Expr *>* list, parser::Expr *item) {
    list->push_back(item);
    return list;
}

std::vector<parser::TypedVar *>* combine(std::vector<parser::TypedVar *>* list, parser::TypedVar *item) {
    list->push_back(item);
    return list;
}

std::vector<parser::IfStmt *>* combine(std::vector<parser::IfStmt *>* list, parser::IfStmt *item) {
    list->push_back(item);
    return list;
}

template<typename T>
T* get_right (std::vector<T*> *item){
    return item->at(0);
}
%}


%union {
  char * raw_str;
  int * location;
  int raw_int;
  const char *error_msg;
  ::parser::Program *PtrProgram;
  ::parser::Stmt *PtrStmt;
  ::parser::Decl *PtrDecl;
  ::parser::AssignStmt *PtrAssignStmt;
  ::parser::BinaryExpr *PtrBinaryExpr;
  ::parser::BoolLiteral *PtrBoolLiteral;
  ::parser::CallExpr *PtrCallExpr;
  ::parser::ClassDef *PtrClassDef;
  ::parser::ClassType *PtrClassType;
  ::parser::CompilerErr *PtrCompilerErr;
  ::parser::Err *PtrErr;
  ::parser::Expr *PtrExpr;
  ::parser::ExprStmt *PtrExprStmt;
  ::parser::ForStmt *PtrForStmt;
  ::parser::FuncDef *PtrFuncDef;
  ::parser::GlobalDecl *PtrGlobalDecl;
  ::parser::Ident *PtrIdent;
  ::parser::IfExpr *PtrIfExpr;
  ::parser::IndexExpr *PtrIndexExpr;
  ::parser::IntegerLiteral *PtrIntegerLiteral;
  ::parser::ListExpr *PtrListExpr;
  ::parser::ListType *PtrListType;
  ::parser::Literal *PtrLiteral;
  ::parser::MemberExpr *PtrMemberExpr;
  ::parser::MethodCallExpr *PtrMethodCallExpr;
  ::parser::Node *PtrNode;
  ::parser::NoneLiteral *PtrNoneLiteral;
  ::parser::NonlocalDecl *PtrNonlocalDecl;
  ::parser::ReturnStmt *PtrReturnStmt;
  ::parser::StringLiteral *PtrStringLiteral;
  ::parser::TypeAnnotation *PtrTypeAnnotation;
  ::parser::TypedVar *PtrTypedVar;
  ::parser::UnaryExpr *PtrUnaryExpr;
  ::parser::VarDef *PtrVarDef;
  ::parser::WhileStmt *PtrWhileStmt;
  ::parser::IfStmt *PtrIfStmt;
  ::parser::AccOptions *PtrAccOptions;
  std::vector<::parser::Decl *> *PtrListDecl;
  std::vector<::parser::Stmt *> *PtrListStmt;
  std::vector<::parser::TypedVar *> *PtrListTypedVar;
  std::vector<::parser::Expr *> *PtrExprList;
}


/* declare tokens */
%token <raw_int> TOKEN_INTEGER
%token <raw_str> TOKEN_IDENTIFIER
%token <raw_str> TOKEN_STRING
%token <location> TOKEN_TRUE
%token <location> TOKEN_FALSE
%token <location> TOKEN_AND
%token <location> TOKEN_BREAK
%token <location> TOKEN_DEF
%token <location> TOKEN_ELIF
%token <location> TOKEN_ELSE
%token <location> TOKEN_FOR
%token <location> TOKEN_IF
%token <location> TOKEN_NOT
%token <location> TOKEN_OR
%token <location> TOKEN_RETURN
%token <location> TOKEN_WHILE
%token <location> TOKEN_NONE
%token <location> TOKEN_AS
%token <location> TOKEN_CLASS
%token <location> TOKEN_GLOBAL
%token <location> TOKEN_IN
%token <location> TOKEN_IS
%token <location> TOKEN_NONLOCAL
%token <location> TOKEN_PASS
/* For ACC support */
/* %token <raw_str> TOKEN_DECORATOR
%token <raw_str> TOKEN_ACC
%token <raw_str> TOKEN_PARALLEL
%token <raw_str> TOKEN_LOOP
%token <raw_str> TOKEN_VECTOR
%token <raw_str> TOKEN_WORKER */

%token <location> TOKEN_plus
%token <location> TOKEN_minus
%token <location> TOKEN_star
%token <location> TOKEN_slash
%token <location> TOKEN_percent
%token <location> TOKEN_less
%token <location> TOKEN_greater
%token <location> TOKEN_lessequal
%token <location> TOKEN_greaterequal
%token <location> TOKEN_equalequal
%token <location> TOKEN_exclaimequal
%token <location> TOKEN_equal
%token <location> TOKEN_l_paren
%token <location> TOKEN_r_paren
%token <location> TOKEN_l_square
%token <location> TOKEN_r_square
%token <location> TOKEN_comma
%token <location> TOKEN_colon
%token <location> TOKEN_period
%token <location> TOKEN_rarrow
%token TOKEN_INDENT
%token TOKEN_DEDENT
%token TOKEN_NEWLINE

%type <PtrProgram> program
%type <PtrStmt> stmt simple_stmt
%type <PtrIfStmt> elif_list
%type <PtrListDecl> top_level_decl class_body func_decls
%type <PtrListStmt> stmt_list block
%type <PtrListTypedVar> typed_var_list
%type <PtrClassDef> class_def
%type <PtrGlobalDecl> global_decl
%type <PtrTypedVar> typed_var
%type <PtrTypeAnnotation> type func_return_type
%type <PtrExpr> expr expr_no_if cexpr target
%type <PtrExprList> expr_list
%type <PtrIndexExpr> index_expr
%type <PtrMemberExpr> member_expr
%type <PtrLiteral> literal
%type <PtrIdent> identifier
%type <PtrVarDef> var_def
%type <PtrFuncDef> func_def
%type <PtrNonlocalDecl> nonlocal_decl
%type <PtrAssignStmt> assign_stmt
/* %type <PtrAccOptions> acc_options */


%left TOKEN_OR
%left TOKEN_AND
%left TOKEN_NOT
%nonassoc TOKEN_equalequal TOKEN_exclaimequal TOKEN_greater TOKEN_greaterequal TOKEN_less TOKEN_lessequal TOKEN_IS
%left TOKEN_plus TOKEN_minus
%left TOKEN_star TOKEN_slash TOKEN_percent
%left UMINUS
%right TOKEN_period TOKEN_comma TOKEN_l_square TOKEN_r_square

%start program

%%
/* The grammar rules Your Code Here */

program : top_level_decl stmt_list {
        delete[] ROOT->location;
        ROOT->location = mergelocation($1->size() ? $1->front()->location : $2->front()->location, $2->back());
        ROOT->location[3]++;
        ROOT->declarations = $1;
        ROOT->statements = $2;
    }
    | top_level_decl {
        if ($1->size() > 0) {
            delete[] ROOT->location;
            ROOT->location = mergelocation($1->front(), $1->back());
            ROOT->location[3]++;
        }
        ROOT->declarations = $1;
    }
top_level_decl : { $$ = new std::vector<parser::Decl *>(); }
    | top_level_decl var_def { $$ = combine($1, $2); }
    | top_level_decl func_def { $$ = combine($1, $2); }
    | top_level_decl class_def { $$ = combine($1, $2); }

stmt_list : stmt { $$ = single($1); }
    | stmt_list stmt { $$ = combine($1, $2); }

class_def : TOKEN_CLASS identifier TOKEN_l_paren identifier TOKEN_r_paren TOKEN_colon TOKEN_NEWLINE TOKEN_INDENT class_body TOKEN_DEDENT {
        $$ = new parser::ClassDef(set_backlocation($1, $9->back()), $2, $4, $9);
        delete[] $3; delete[] $5; delete[] $6;
    }
    | TOKEN_CLASS identifier TOKEN_l_paren identifier TOKEN_r_paren TOKEN_colon TOKEN_NEWLINE TOKEN_INDENT TOKEN_PASS TOKEN_NEWLINE TOKEN_DEDENT {
        $$ = new parser::ClassDef(set_backlocation($1, $9), $2, $4, new std::vector<parser::Decl *>());
        delete[] $3; delete[] $5; delete[] $6; delete[] $9;
    }
class_body : var_def { $$ = single((parser::Decl *)$1); }
    | func_def { $$ = single((parser::Decl *)$1); }
    | class_body var_def { $$ = combine($$, (parser::Decl *)$2); }
    | class_body func_def { $$ = combine($$, (parser::Decl *)$2); }

func_def : TOKEN_DEF identifier TOKEN_l_paren typed_var_list TOKEN_r_paren func_return_type TOKEN_colon TOKEN_NEWLINE TOKEN_INDENT func_decls stmt_list TOKEN_DEDENT {
    $$ = new parser::FuncDef(set_backlocation($1, $11->back()), $2, $4, $6, $10, $11);
    delete[] $3; delete[] $5; delete[] $7;
}
typed_var_list:  { $$ = new std::vector<parser::TypedVar *>(); }
    | typed_var { $$ = single($1); }
    | typed_var_list TOKEN_comma typed_var { $$ = combine($1, $3); delete[] $2; }
func_return_type: { $$ = new parser::ClassType(yylloc2array, "<None>"); }
    | TOKEN_rarrow type { $$ = $2; delete[] $1; }
func_decls: { $$ = new std::vector<parser::Decl *>(); }
    | func_decls global_decl { $$ = combine($1, $2); }
    | func_decls nonlocal_decl { $$ = combine($1, $2); }
    | func_decls var_def { $$ = combine($1, $2); }
    | func_decls func_def { $$ = combine($1, $2); }

typed_var : identifier TOKEN_colon type { $$ = new parser::TypedVar(reuse_location($2, $1, $3), $1, $3); }
type : TOKEN_IDENTIFIER { $$ = new parser::ClassType(yylloc2array, string($1)); free($1); }
    | TOKEN_STRING { $$ = new parser::ClassType(yylloc2array, string($1)); delete[] $1; }
    | TOKEN_l_square type TOKEN_r_square { $$ = new parser::ListType(set_backlocation($1, $3), $2); delete[] $3; }

global_decl : TOKEN_GLOBAL identifier TOKEN_NEWLINE { $$ = new parser::GlobalDecl(set_backlocation($1, $2), $2); }
nonlocal_decl : TOKEN_NONLOCAL identifier TOKEN_NEWLINE { $$ = new parser::NonlocalDecl(set_backlocation($1, $2), $2); }
var_def : typed_var TOKEN_equal literal TOKEN_NEWLINE { $$ = new parser::VarDef(reuse_location($2, $1, $3), $1, $3); }

stmt : simple_stmt TOKEN_NEWLINE { $$ = $1; }
    | TOKEN_WHILE expr TOKEN_colon block {
        $1[2] = yylloc.last_line;
        $1[3] = yylloc.last_column - 1;
        $$ = new parser::WhileStmt($1, $2, $4); delete[] $3;
        // LOG(ERROR) << yylloc.first_line << ' ' << yylloc.first_column << ' ' << yylloc.last_line << ' ' << yylloc.last_column;
    }
    | TOKEN_FOR identifier TOKEN_IN expr TOKEN_colon block { $$ = new parser::ForStmt(set_backlocation($1, $6->back()), $2, $4, $6); delete $3; delete $5; }
    | TOKEN_IF expr TOKEN_colon block { $$ = new parser::IfStmt(set_backlocation($1, $4->back()), $2, $4); delete[] $3; }
    | TOKEN_IF expr TOKEN_colon block elif_list { $$ = new parser::IfStmt(set_backlocation($1, $5), $2, $4, $5); delete[] $3; }
    | TOKEN_IF expr TOKEN_colon block TOKEN_ELSE TOKEN_colon block { $$ = new parser::IfStmt(set_backlocation($1, $7->back()), $2, $4, $7); delete[] $3; delete[] $5; delete[] $6; }
    | assign_stmt { std::reverse($1->targets->begin(), $1->targets->end()); $$ = $1; }
elif_list : TOKEN_ELIF expr TOKEN_colon block elif_list { $$ = new parser::IfStmt(set_backlocation($1, $5), $2, $4, $5); delete[] $3; }
    | TOKEN_ELIF expr TOKEN_colon block TOKEN_ELSE TOKEN_colon block { $$ = new parser::IfStmt(set_backlocation($1, $7->back()), $2, $4, $7); delete[] $3; delete[] $5; delete[] $6; }
    | TOKEN_ELIF expr TOKEN_colon block { $$ = new parser::IfStmt(set_backlocation($1, $4->back()), $2, $4); delete $3; }

simple_stmt : TOKEN_PASS { $$ = new parser::PassStmt($1); }
    | expr { $$ = new parser::ExprStmt(mergelocation($1, $1), $1); }
    | TOKEN_RETURN { $$ = new parser::ReturnStmt($1); }
    | TOKEN_RETURN expr { $$ = new parser::ReturnStmt(set_backlocation($1, $2), $2); }

block : TOKEN_NEWLINE TOKEN_INDENT stmt_list TOKEN_DEDENT  { $$ = $3; }

literal : TOKEN_INTEGER { $$ = new parser::IntegerLiteral(yylloc2array, $1); }
    | TOKEN_TRUE { $$ = new parser::BoolLiteral($1, true); }
    | TOKEN_FALSE { $$ = new parser::BoolLiteral($1, false); }
    | TOKEN_NONE { $$ = new parser::NoneLiteral($1); }
    | TOKEN_STRING { $$ = new parser::StringLiteral(yylloc2array, string($1)); delete[] $1; }

expr : expr_no_if { $$ = $1; }
    | expr_no_if TOKEN_IF expr TOKEN_ELSE expr { $$ = new parser::IfExpr(reuse_location($2, $1, $5), $3, $1, $5); delete[] $4; }

expr_no_if : cexpr { $$ = $1; }
    | TOKEN_NOT expr_no_if { $$ = new parser::UnaryExpr(set_backlocation($1, $2), std::string("not"), $2); }
    | expr_no_if TOKEN_AND expr_no_if { $$ = new parser::BinaryExpr(reuse_location($2, $1, $3), $1, string("and"), $3); }
    | expr_no_if TOKEN_OR expr_no_if { $$ = new parser::BinaryExpr(reuse_location($2, $1, $3), $1, string("or"), $3); }

expr_list : { $$ = new std::vector<::parser::Expr *>(); }
    | expr { $$ = single($1); }
    | expr_list TOKEN_comma expr { $$ = combine($1, $3); delete[] $2; }

cexpr : identifier { $$ = $1; }
    | literal { $$ = $1; }
    | TOKEN_l_square expr_list TOKEN_r_square { $$ = new parser::ListExpr(set_backlocation($1, $3), $2); delete[] $3; }
    | TOKEN_l_paren expr TOKEN_r_paren { $$ = $2; delete[] $1; delete[] $3; }
    | member_expr { $$ = $1; }
    | index_expr { $$ = $1; }
    | member_expr TOKEN_l_paren expr_list TOKEN_r_paren { $$ = new parser::MethodCallExpr(set_frontlocation($4, $1), $1, $3); delete[] $2; }
    | identifier TOKEN_l_paren expr_list TOKEN_r_paren { $$ = new parser::CallExpr(set_frontlocation($4, $1), $1, $3); delete[] $2; }
    | TOKEN_minus cexpr %prec UMINUS { $$ = new parser::UnaryExpr(set_backlocation($1, $2), string("-"), $2); }
    | cexpr TOKEN_plus cexpr { $$ = new parser::BinaryExpr(reuse_location($2, $1, $3), $1, string("+"), $3); }
    | cexpr TOKEN_minus cexpr { $$ = new parser::BinaryExpr(reuse_location($2, $1, $3), $1, string("-"), $3); }
    | cexpr TOKEN_star cexpr { $$ = new parser::BinaryExpr(reuse_location($2, $1, $3), $1, string("*"), $3); }
    | cexpr TOKEN_slash cexpr { $$ = new parser::BinaryExpr(reuse_location($2, $1, $3), $1, string("//"), $3); }
    | cexpr TOKEN_percent cexpr { $$ = new parser::BinaryExpr(reuse_location($2, $1, $3), $1, string("%"), $3); }
    | cexpr TOKEN_equalequal cexpr { $$ = new parser::BinaryExpr(reuse_location($2, $1, $3), $1, string("=="), $3); }
    | cexpr TOKEN_exclaimequal cexpr { $$ = new parser::BinaryExpr(reuse_location($2, $1, $3), $1, string("!="), $3); }
    | cexpr TOKEN_lessequal cexpr { $$ = new parser::BinaryExpr(reuse_location($2, $1, $3), $1, string("<="), $3); }
    | cexpr TOKEN_greaterequal cexpr { $$ = new parser::BinaryExpr(reuse_location($2, $1, $3), $1, string(">="), $3); }
    | cexpr TOKEN_greater cexpr { $$ = new parser::BinaryExpr(reuse_location($2, $1, $3), $1, string(">"), $3); }
    | cexpr TOKEN_less cexpr { $$ = new parser::BinaryExpr(reuse_location($2, $1, $3), $1, string("<"), $3); }
    | cexpr TOKEN_IS cexpr { $$ = new parser::BinaryExpr(reuse_location($2, $1, $3), $1, string("is"), $3); }

member_expr : cexpr TOKEN_period identifier { $$ = new parser::MemberExpr(reuse_location($2, $1, $3), $1, $3); }
index_expr : cexpr TOKEN_l_square expr TOKEN_r_square { $$ = new parser::IndexExpr(reuse_location($2, $1->location, $4), $1, $3); delete[] $4; }
target : identifier { $$ = $1; }
    | member_expr { $$ = $1; }
    | index_expr { $$ = $1; }
assign_stmt : target TOKEN_equal expr TOKEN_NEWLINE { $$ = new parser::AssignStmt(reuse_location($2, $1, $3), single($1), $3); }
    | target TOKEN_equal assign_stmt { $3->targets->push_back($1); $$ = $3;  set_frontlocation($3->location, $1); delete[] $2; }

identifier : TOKEN_IDENTIFIER { $$ = new parser::Ident(yylloc2array, string($1)); free($1); }
%%

/** The error reporting function. */
void yyerror(const char *s) {
    /** TO STUDENTS: This is just an example.
     * You can customize it as you like. */
    info = string("Parser error near token");
    int *loc = new int[4]{0};
    loc[0] = yylloc.first_line;
    loc[1] = yylloc.first_column;
    loc[2] = yylloc.last_line;
    loc[3] = yylloc.last_column - 1;

    parser::CompilerErr *test = new parser::CompilerErr(loc, info, true);
    ROOT->errors->compiler_errors = new vector<parser::CompilerErr *>();
    ROOT->has_compiler_errors = true;
    ROOT->errors->compiler_errors->push_back(test);
    ((parser::Node *)ROOT)->location[2] = e1;
    ((parser::Node *)ROOT)->location[3] = e2;
}

parser::Program * parse(const char* input_path) {
    if (input_path != NULL) {
        if (!(yyin = fopen(input_path, "r"))) {
            fprintf(stderr, "[ERR] Open input file %s failed.\n", input_path);
            exit(EXIT_FAILURE);
        }
    } else {
        yyin = stdin;
    }
    /** Uncomment to see the middle process of bison*/
    /* yydebug = 1; */
    yyrestart(yyin);
    yyparse();
    return ROOT;
}
