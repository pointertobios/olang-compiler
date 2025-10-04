grammar Olang;

// Lexer rules
WS : [ \t\r\n]+ -> skip ;
COMMENT : '//' ~[\r\n]* -> skip ;
BLOCK_COMMENT : '/*' .*? '*/' -> skip ;

// Keywords
FUNCTION : 'fn' ;
LET : 'let' ;
IF : 'if' ;
ELSE : 'else' ;
WHILE : 'while' ;
RETURN : 'return' ;
STRUCT : 'struct' ;
ARRAY : 'array' ;
POINTER : 'ptr' ;
TRUE : 'true' ;
FALSE : 'false' ;
EXTERN : 'extern' ;
EXPORT : 'export' ;
INCLUDE : 'include' ;

// Type keywords
I1 : 'i1' ;
I8 : 'i8' ;
I16 : 'i16' ;
I32 : 'i32' ;
I64 : 'i64' ;
F16 : 'f16' ;
F32 : 'f32' ;
F64 : 'f64' ;

// Identifiers and literals
IDENTIFIER : [a-zA-Z_][a-zA-Z0-9_]* ;
INT_LITERAL : [0-9]+ ;
FLOAT_LITERAL : [0-9]+ '.' [0-9]+ ;
STRING_LITERAL : '"' ~["]* '"' ;

// Operators
PLUS : '+' ;
MINUS : '-' ;
MULTIPLY : '*' ;
DIVIDE : '/' ;
MODULO : '%' ;
ASSIGN : '=' ;
EQUAL : '==' ;
NOT_EQUAL : '!=' ;
LESS : '<' ;
GREATER : '>' ;
LESS_EQUAL : '<=' ;
GREATER_EQUAL : '>=' ;
AND : '&&' ;
OR : '||' ;
NOT : '!' ;

// Delimiters
LPAREN : '(' ;
RPAREN : ')' ;
LBRACE : '{' ;
RBRACE : '}' ;
LBRACKET : '[' ;
RBRACKET : ']' ;
SEMICOLON : ';' ;
COMMA : ',' ;
DOT : '.' ;
COLON : ':' ;
ARROW : '->' ;
AMPERSAND : '&' ;

// Parser rules
program : (include_stmt | struct_decl | function_decl | global_var_decl | extern_decl)* EOF ;

include_stmt : INCLUDE STRING_LITERAL SEMICOLON ;

struct_decl : STRUCT IDENTIFIER LBRACE struct_field* RBRACE ;

struct_field : IDENTIFIER COLON type_spec SEMICOLON ;

type_spec : basic_type
          | pointer_type
          | array_type
          | struct_type
          ;

basic_type : I1 | I8 | I16 | I32 | I64 | F16 | F32 | F64 ;

pointer_type : MULTIPLY type_spec ;

array_type : ARRAY LBRACKET INT_LITERAL RBRACKET type_spec ;

struct_type : IDENTIFIER ;

function_decl : EXPORT? FUNCTION IDENTIFIER LPAREN param_list? RPAREN (ARROW type_spec)? LBRACE statement* RBRACE ;

extern_decl : EXTERN FUNCTION IDENTIFIER LPAREN param_list? RPAREN (ARROW type_spec)? SEMICOLON ;

param_list : parameter (COMMA parameter)* ;

parameter : IDENTIFIER COLON type_spec ;

global_var_decl : LET IDENTIFIER COLON type_spec ASSIGN expression SEMICOLON ;

statement : expr_statement
          | let_statement
          | return_statement
          | if_statement
          | while_statement
          | block_statement
          ;

expr_statement : expression SEMICOLON ;

let_statement : LET IDENTIFIER COLON type_spec ASSIGN expression SEMICOLON ;

return_statement : RETURN expression? SEMICOLON ;

if_statement : IF expression LBRACE statement* RBRACE (ELSE LBRACE statement* RBRACE)? ;

while_statement : WHILE expression LBRACE statement* RBRACE ;

block_statement : LBRACE statement* RBRACE ;

expression : assignment_expr ;

assignment_expr : logical_or_expr (ASSIGN assignment_expr)? ;

logical_or_expr : logical_and_expr (OR logical_and_expr)* ;

logical_and_expr : equality_expr (AND equality_expr)* ;

equality_expr : relational_expr ((EQUAL | NOT_EQUAL) relational_expr)* ;

relational_expr : additive_expr ((LESS | GREATER | LESS_EQUAL | GREATER_EQUAL) additive_expr)* ;

additive_expr : multiplicative_expr ((PLUS | MINUS) multiplicative_expr)* ;

multiplicative_expr : unary_expr ((MULTIPLY | DIVIDE | MODULO) unary_expr)* ;

unary_expr : (NOT | MINUS | MULTIPLY | AMPERSAND) unary_expr
           | postfix_expr
           ;

postfix_expr : primary_expr (LBRACKET expression RBRACKET | DOT IDENTIFIER | LPAREN argument_list? RPAREN)* ;

primary_expr : INT_LITERAL
             | FLOAT_LITERAL
             | STRING_LITERAL
             | TRUE
             | FALSE
             | IDENTIFIER
             | LPAREN expression RPAREN
             ;

argument_list : expression (COMMA expression)* ;
