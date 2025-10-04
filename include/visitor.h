#pragma once
#include "ast.h"
#include "OlangBaseVisitor.h"

namespace olang {

class ASTVisitor : public OlangBaseVisitor {
private:
    std::vector<std::unique_ptr<ASTNode>> node_stack;
    
public:
    // Program
    std::any visitProgram(OlangParser::ProgramContext *ctx) override;
    
    // Struct declarations
    std::any visitStruct_decl(OlangParser::Struct_declContext *ctx) override;
    
    // Function declarations
    std::any visitFunction_decl(OlangParser::Function_declContext *ctx) override;
    
    // External function declarations
    std::any visitExtern_decl(OlangParser::Extern_declContext *ctx) override;
    
    // Statements
    std::any visitLet_statement(OlangParser::Let_statementContext *ctx) override;
    std::any visitReturn_statement(OlangParser::Return_statementContext *ctx) override;
    std::any visitExpr_statement(OlangParser::Expr_statementContext *ctx) override;
    std::any visitIf_statement(OlangParser::If_statementContext *ctx) override;
    std::any visitWhile_statement(OlangParser::While_statementContext *ctx) override;
    
    // Expressions
    std::any visitAssignment_expr(OlangParser::Assignment_exprContext *ctx) override;
    std::any visitLogical_or_expr(OlangParser::Logical_or_exprContext *ctx) override;
    std::any visitLogical_and_expr(OlangParser::Logical_and_exprContext *ctx) override;
    std::any visitEquality_expr(OlangParser::Equality_exprContext *ctx) override;
    std::any visitRelational_expr(OlangParser::Relational_exprContext *ctx) override;
    std::any visitAdditive_expr(OlangParser::Additive_exprContext *ctx) override;
    std::any visitMultiplicative_expr(OlangParser::Multiplicative_exprContext *ctx) override;
    std::any visitUnary_expr(OlangParser::Unary_exprContext *ctx) override;
    std::any visitPostfix_expr(OlangParser::Postfix_exprContext *ctx) override;
    std::any visitPrimary_expr(OlangParser::Primary_exprContext *ctx) override;
    
    // Type parsing
    Type parseType(OlangParser::Type_specContext *ctx);
    
    // Helper methods
    BinaryExpr::Op getBinaryOp(antlr4::Token* token);
    UnaryExpr::Op getUnaryOp(antlr4::Token* token);
    
    // Get last node
    template<typename T>
    T* getLastNode() {
        if (!node_stack.empty()) {
            return dynamic_cast<T*>(node_stack.back().get());
        }
        return nullptr;
    }
    
    // Pop last node
    std::unique_ptr<ASTNode> popNode() {
        if (!node_stack.empty()) {
            auto node = std::move(node_stack.back());
            node_stack.pop_back();
            return node;
        }
        return nullptr;
    }
    
    // Push node
    void pushNode(std::unique_ptr<ASTNode> node) {
        node_stack.push_back(std::move(node));
    }
};

} // namespace olang
