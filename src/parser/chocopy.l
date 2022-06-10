%option noyywrap
%option yylineno

/* The prefix for imports in c syntax */
%{
/* For Indentation */
#include <stack>
#include <iostream>
#include <cstdlib>
#include <cstdio>

#if __cplusplus > 199711L
#define register      // Deprecated in C++11.
#endif  // #if __cplusplus > 199711L

using namespace std;
std::stack<int> stack_indent;

#include <chocopy_parse.hpp>
#include "chocopy.tab.h"

int e1 = 1, e2 = 1,global_leng=0;
string info = "";

#define LITERAL(type) \
    LOG(DEBUG) << #type << /*" " <<  yytext <<*/ " location: "<< yylloc.first_line<<" " << yylloc.first_column<< " " << (e1 = yylloc.last_line) <<" " << (e2 = yylloc.last_column); \
    return TOKEN_##type;

#define LITERAL_END(type) \
    LOG(DEBUG) << #type << /*" " <<  yytext <<*/ " location: "<< yylloc.first_line<<" " << yylloc.first_column<< " " << (e1 = yylloc.last_line) <<" " << (e2 = yylloc.last_column); \
    return TOKEN_##type;

int ColNum = 1;
int LineNum = 1;
#define SET_LOCATION yylval.location = new int[4]{LineNum, ColNum, LineNum, ColNum+yyleng-1};
#define SET_ACTION                                                           \
  yylloc.first_line = LineNum;                                               \
  yylloc.first_column = ColNum;                                              \
  ColNum += yyleng;                                                          \
  yylloc.last_line = LineNum;                                                \
  yylloc.last_column = ColNum;

#define ANOTHER_LINE                                                         \
  ColNum = 1;                                                                \
  LineNum += 1;
#define CLEAR_ONE                                                         \
  ColNum -= 4;                                                                \

#define YY_ANOTHER_TAB                                                       \
  ColNum += 4;
#define YY_ANOTHER_SPACE                                                     \
  ColNum += 1;
#define YY_ANOTHER_STR(i)                                                    \
  ColNum += (i);
%}

%x CODE

%%

%{
  /* Init indent stack_indent */
  if (stack_indent.empty()) {
    stack_indent.push(0);
  }

%}

<CODE>"True" { SET_LOCATION SET_ACTION LITERAL(TRUE) }
<CODE>"False" { SET_LOCATION SET_ACTION LITERAL(FALSE) }
<CODE>"None" { SET_LOCATION SET_ACTION LITERAL(NONE) }

<CODE>"and" { SET_LOCATION SET_ACTION LITERAL(AND) }
<CODE>"not" { SET_LOCATION SET_ACTION LITERAL(NOT) }
<CODE>"or" { SET_LOCATION SET_ACTION LITERAL(OR) }

<CODE>"as" { SET_LOCATION SET_ACTION LITERAL(AS) }
<CODE>"in" { SET_LOCATION SET_ACTION LITERAL(IN) }
<CODE>"is" { SET_LOCATION SET_ACTION LITERAL(IS) }

<CODE>"global" { SET_LOCATION SET_ACTION LITERAL(GLOBAL) }
<CODE>"nonlocal" { SET_LOCATION SET_ACTION LITERAL(NONLOCAL) }

<CODE>"if" { SET_LOCATION SET_ACTION LITERAL(IF) }
<CODE>"else" { SET_LOCATION SET_ACTION LITERAL(ELSE) }
<CODE>"elif" { SET_LOCATION SET_ACTION LITERAL(ELIF) }

<CODE>"for" { SET_LOCATION SET_ACTION LITERAL(FOR) }
<CODE>"while" { SET_LOCATION SET_ACTION LITERAL(WHILE) }
<CODE>"break" { SET_LOCATION SET_ACTION LITERAL(BREAK) }

<CODE>"def" { SET_LOCATION SET_ACTION LITERAL(DEF) }
<CODE>"return" { SET_LOCATION SET_ACTION LITERAL(RETURN) }
<CODE>"pass" { SET_LOCATION SET_ACTION LITERAL(PASS) }

<CODE>"class" { SET_LOCATION SET_ACTION LITERAL(CLASS) }

<CODE>"+" { SET_LOCATION SET_ACTION LITERAL(plus) }
<CODE>"-" { SET_LOCATION SET_ACTION LITERAL(minus) }
<CODE>"*" { SET_LOCATION SET_ACTION LITERAL(star) }
<CODE>"//" { SET_LOCATION SET_ACTION LITERAL(slash) }
<CODE>"%" { SET_LOCATION SET_ACTION LITERAL(percent) }

<CODE>"=" { SET_LOCATION SET_ACTION LITERAL(equal) }

<CODE>"<=" { SET_LOCATION SET_ACTION LITERAL(lessequal) }
<CODE>">=" { SET_LOCATION SET_ACTION LITERAL(greaterequal) }
<CODE>"==" { SET_LOCATION SET_ACTION LITERAL(equalequal) }
<CODE>"!=" { SET_LOCATION SET_ACTION LITERAL(exclaimequal) }
<CODE>"<" { SET_LOCATION SET_ACTION LITERAL(less) }
<CODE>">" { SET_LOCATION SET_ACTION LITERAL(greater) }

<CODE>"(" { SET_LOCATION SET_ACTION LITERAL(l_paren) }
<CODE>")" { SET_LOCATION SET_ACTION LITERAL(r_paren) }
<CODE>"[" { SET_LOCATION SET_ACTION LITERAL(l_square) }
<CODE>"]" { SET_LOCATION SET_ACTION LITERAL(r_square) }
<CODE>"," { SET_LOCATION SET_ACTION LITERAL(comma) }
<CODE>":" { SET_LOCATION SET_ACTION LITERAL(colon) }
<CODE>"." { SET_LOCATION SET_ACTION LITERAL(period) }
<CODE>"->" { SET_LOCATION SET_ACTION LITERAL(rarrow) }

<CODE>[0-9]+ {
  yylval.raw_int = atoi(yytext);
  SET_ACTION
  LOG(DEBUG) << "INTEGER" << /*" " <<  yytext <<*/ " location: "<< yylloc.first_line<<" " << yylloc.first_column<< " " << (e1 = yylloc.last_line) <<" " << (e2 = yylloc.last_column);
  return TOKEN_INTEGER;
}

<CODE>(assert)|(async)|(await)|(continue)|(del)|(except)|(finally)|(from)|(import)|(lambda)|(raise)|(try)|(with)|(yield) {
  LOG(ERROR) << "Error: reversed token on line " << yylineno << ": " << yytext;
  return ERROR;
}

<CODE>[a-zA-Z_][a-zA-Z_0-9]* {
  yylval.raw_str = strdup(yytext);
  SET_ACTION
  LITERAL(IDENTIFIER);
}

<CODE>[ \t]* { ColNum += yyleng; }
<CODE>(#.*)?(\r|(\r?\n)) {
  // LOG(WARNING) << "INITIAL";
  BEGIN INITIAL;

  SET_ACTION
  ANOTHER_LINE
  LITERAL_END(NEWLINE);
}

^[ \t]*#.* { }

[ \t]*[^\r\n\t ] {
  static int indent_status = 0;

  int tabs = 0;
  for(int i = 0; yytext[i] != '\0'; i++) tabs += (yytext[i] == '\t');
  int spaces = yyleng - 1 + (4-1)*tabs;

  LineNum = yylineno;
  ColNum = 1;

  yylloc.first_line = LineNum;
  yylloc.first_column = ColNum;
  ColNum += yyleng - 1;
  yylloc.last_line = LineNum;
  yylloc.last_column = ColNum;

  if (stack_indent.top() < spaces) {
    if (indent_status == 1) {
      return ERROR;
    }
    indent_status = 2;
    stack_indent.push(spaces);
    // LOG(WARNING) << "CODE";
    BEGIN CODE;
    yyless(yyleng - 1);
    LITERAL(INDENT)
  }

  if (stack_indent.top() > spaces) {
    indent_status = 1;
    stack_indent.pop();
    yyless(0);
    LITERAL(DEDENT)
  }

  // stack_indent.top() == spaces
  indent_status = 0;
  BEGIN CODE;
  yyless(yyleng - 1);
}

<CODE><<EOF>> {
  // LOG(WARNING) << "INITIAL";
  BEGIN INITIAL;

  SET_ACTION ColNum--;
  LITERAL_END(NEWLINE);
}

^[\t ]+ { }

(\r|(\r?\n)) { ANOTHER_LINE }

<<EOF>> {
  if (stack_indent.top() != 0) {
    stack_indent.pop();

    SET_ACTION
    LITERAL(DEDENT)
  }
  return 0;
}

<CODE>(\"([ -!#-\[\]-~]|(\\(n|t|\\|\")))*\") {
  // yylval.raw_str = strdup(yytext);
  yylval.raw_str = new char[yyleng+1];
  int len = 0;

  for (int i = 1; i < yyleng-1; i++) {
    if (yytext[i] == '\\') {
      yylval.raw_str[len++] = yytext[i+1] == 'n' ? '\n' : yytext[i+1] == 't' ? '\t' : yytext[i+1] == '\\' ? '\\' : '\"';
      i++;
    } else {
      yylval.raw_str[len++] = yytext[i];
    }
  }
  yylval.raw_str[len] = '\0';

  SET_ACTION LITERAL(STRING)
}

.   {
        LOG(ERROR) << "Error: Unrecognized token on line " << yylineno << ": " << yytext;
        return ERROR;
    }
<CODE>.   {
        LOG(ERROR) << "Error: Unrecognized token on line " << yylineno << ": " << yytext;
        return ERROR;
    }
%%

