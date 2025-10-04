#pragma once
#include <string>
#include <vector>
#include <memory>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Function.h>

namespace olang {

enum class TypeKind {
    I1, I8, I16, I32, I64,
    F16, F32, F64,
    POINTER, ARRAY, STRUCT,
    VOID
};

struct Type {
    TypeKind kind;
    std::string name; // For struct types
    std::shared_ptr<Type> element_type; // For pointer and array
    int array_size = 0; // For array
    
    llvm::Type* llvm_type = nullptr;
    
    Type() : kind(TypeKind::VOID) {}
    Type(TypeKind k) : kind(k) {}
    Type(TypeKind k, const std::string& n) : kind(k), name(n) {}
    Type(TypeKind k, std::shared_ptr<Type> elem) : kind(k), element_type(elem) {}
    Type(TypeKind k, int size, std::shared_ptr<Type> elem) : kind(k), element_type(elem), array_size(size) {}
};

class ASTNode {
public:
    virtual ~ASTNode() = default;
    virtual llvm::Value* codegen(class CodeGenContext& ctx) = 0;
};

class Program : public ASTNode {
public:
    std::vector<std::unique_ptr<ASTNode>> declarations;
    llvm::Value* codegen(class CodeGenContext& ctx) override;
};

class StructDecl : public ASTNode {
public:
    std::string name;
    std::vector<std::pair<Type, std::string>> fields;
    llvm::Value* codegen(class CodeGenContext& ctx) override;
};

class FunctionDecl : public ASTNode {
public:
    std::string name;
    std::vector<std::pair<Type, std::string>> params;
    Type return_type;
    std::vector<std::unique_ptr<ASTNode>> body;
    bool is_export = false;
    llvm::Value* codegen(class CodeGenContext& ctx) override;
};

class ExternDecl : public ASTNode {
public:
    std::string name;
    std::vector<std::pair<Type, std::string>> params;
    Type return_type;
    llvm::Value* codegen(class CodeGenContext& ctx) override;
};

class LetStmt : public ASTNode {
public:
    Type type;
    std::string name;
    std::unique_ptr<ASTNode> value;
    llvm::Value* codegen(class CodeGenContext& ctx) override;
};

class ReturnStmt : public ASTNode {
public:
    std::unique_ptr<ASTNode> expr;
    llvm::Value* codegen(class CodeGenContext& ctx) override;
};

class ExprStmt : public ASTNode {
public:
    std::unique_ptr<ASTNode> expr;
    llvm::Value* codegen(class CodeGenContext& ctx) override;
};

class IfStmt : public ASTNode {
public:
    std::unique_ptr<ASTNode> condition;
    std::vector<std::unique_ptr<ASTNode>> then_body;
    std::vector<std::unique_ptr<ASTNode>> else_body;
    llvm::Value* codegen(class CodeGenContext& ctx) override;
};

class WhileStmt : public ASTNode {
public:
    std::unique_ptr<ASTNode> condition;
    std::vector<std::unique_ptr<ASTNode>> body;
    llvm::Value* codegen(class CodeGenContext& ctx) override;
};

// Expression nodes
class Expr : public ASTNode {};

class IntLiteral : public Expr {
public:
    int64_t value;
    IntLiteral(int64_t v) : value(v) {}
    llvm::Value* codegen(class CodeGenContext& ctx) override;
};

class FloatLiteral : public Expr {
public:
    double value;
    FloatLiteral(double v) : value(v) {}
    llvm::Value* codegen(class CodeGenContext& ctx) override;
};

class StringLiteral : public Expr {
public:
    std::string value;
    StringLiteral(const std::string& v) : value(v) {}
    llvm::Value* codegen(class CodeGenContext& ctx) override;
};

class BoolLiteral : public Expr {
public:
    bool value;
    BoolLiteral(bool v) : value(v) {}
    llvm::Value* codegen(class CodeGenContext& ctx) override;
};

class Identifier : public Expr {
public:
    std::string name;
    Identifier(const std::string& n) : name(n) {}
    llvm::Value* codegen(class CodeGenContext& ctx) override;
};

class BinaryExpr : public Expr {
public:
    enum Op { ADD, SUB, MUL, DIV, MOD, EQ, NE, LT, GT, LE, GE, AND, OR };
    Op op;
    std::unique_ptr<Expr> left;
    std::unique_ptr<Expr> right;
    
    BinaryExpr(Op o, std::unique_ptr<Expr> l, std::unique_ptr<Expr> r) 
        : op(o), left(std::move(l)), right(std::move(r)) {}
    llvm::Value* codegen(class CodeGenContext& ctx) override;
};

class AssignmentExpr : public Expr {
public:
    std::unique_ptr<Expr> left;   // Left value (identifier)
    std::unique_ptr<Expr> right;  // Right value
    AssignmentExpr(std::unique_ptr<Expr> l, std::unique_ptr<Expr> r)
        : left(std::move(l)), right(std::move(r)) {}
    llvm::Value* codegen(class CodeGenContext& ctx) override;
};

class UnaryExpr : public Expr {
public:
    enum Op { NOT, NEG, DEREF, ADDR };
    Op op;
    std::unique_ptr<Expr> operand;
    
    UnaryExpr(Op o, std::unique_ptr<Expr> opd) : op(o), operand(std::move(opd)) {}
    llvm::Value* codegen(class CodeGenContext& ctx) override;
};

class CallExpr : public Expr {
public:
    std::string function_name;
    std::vector<std::unique_ptr<Expr>> args;
    
    CallExpr(const std::string& name, std::vector<std::unique_ptr<Expr>> a) 
        : function_name(name), args(std::move(a)) {}
    llvm::Value* codegen(class CodeGenContext& ctx) override;
};

class MemberAccess : public Expr {
public:
    std::unique_ptr<Expr> object;
    std::string member;
    
    MemberAccess(std::unique_ptr<Expr> obj, const std::string& mem) 
        : object(std::move(obj)), member(mem) {}
    llvm::Value* codegen(class CodeGenContext& ctx) override;
};

class ArrayAccess : public Expr {
public:
    std::unique_ptr<Expr> array;
    std::unique_ptr<Expr> index;
    
    ArrayAccess(std::unique_ptr<Expr> arr, std::unique_ptr<Expr> idx) 
        : array(std::move(arr)), index(std::move(idx)) {}
    llvm::Value* codegen(class CodeGenContext& ctx) override;
};

} // namespace olang
