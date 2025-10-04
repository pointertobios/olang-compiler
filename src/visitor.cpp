#include "visitor.h"
#include "OlangLexer.h"
#include "OlangParser.h"
#include <stdexcept>

namespace olang {

std::any ASTVisitor::visitProgram(OlangParser::ProgramContext *ctx) {
    auto program = std::make_unique<Program>();
    
    for (auto decl : ctx->children) {
        if (auto include_stmt = dynamic_cast<OlangParser::Include_stmtContext*>(decl)) {
            // Skip include statements (handled by preprocessor in main.cpp)
            continue;
        } else if (auto struct_decl = dynamic_cast<OlangParser::Struct_declContext*>(decl)) {
            visitStruct_decl(struct_decl);
            program->declarations.push_back(popNode());
        } else if (auto func_decl = dynamic_cast<OlangParser::Function_declContext*>(decl)) {
            visitFunction_decl(func_decl);
            program->declarations.push_back(popNode());
        } else if (auto extern_decl = dynamic_cast<OlangParser::Extern_declContext*>(decl)) {
            visitExtern_decl(extern_decl);
            program->declarations.push_back(popNode());
        }
    }
    
    pushNode(std::move(program));
    return nullptr;
}

std::any ASTVisitor::visitStruct_decl(OlangParser::Struct_declContext *ctx) {
    auto struct_decl = std::make_unique<StructDecl>();
    struct_decl->name = ctx->IDENTIFIER()->getText();
    
    for (auto field : ctx->struct_field()) {
        Type field_type = parseType(field->type_spec());
        std::string field_name = field->IDENTIFIER()->getText();
        struct_decl->fields.emplace_back(field_type, field_name);
    }
    
    pushNode(std::move(struct_decl));
    return nullptr;
}

std::any ASTVisitor::visitFunction_decl(OlangParser::Function_declContext *ctx) {
    auto func_decl = std::make_unique<FunctionDecl>();
    func_decl->name = ctx->IDENTIFIER()->getText();
    func_decl->is_export = (ctx->EXPORT() != nullptr);
    
    // Parse parameters
    if (ctx->param_list()) {
        for (auto param : ctx->param_list()->parameter()) {
            Type param_type = parseType(param->type_spec());
            std::string param_name = param->IDENTIFIER()->getText();
            func_decl->params.emplace_back(param_type, param_name);
        }
    }
    
    // Parse return type
    if (ctx->type_spec()) {
        func_decl->return_type = parseType(ctx->type_spec());
    } else {
        func_decl->return_type = Type(TypeKind::VOID);
    }
    
    // Parse function body
    for (auto stmt : ctx->statement()) {
        if (auto let_stmt = stmt->let_statement()) {
            visitLet_statement(let_stmt);
        } else if (auto return_stmt = stmt->return_statement()) {
            visitReturn_statement(return_stmt);
        } else if (auto expr_stmt = stmt->expr_statement()) {
            visitExpr_statement(expr_stmt);
        } else if (auto if_stmt = stmt->if_statement()) {
            visitIf_statement(if_stmt);
        } else if (auto while_stmt = stmt->while_statement()) {
            visitWhile_statement(while_stmt);
        }
        func_decl->body.push_back(popNode());
    }
    
    pushNode(std::move(func_decl));
    return nullptr;
}

std::any ASTVisitor::visitExtern_decl(OlangParser::Extern_declContext *ctx) {
    auto extern_decl = std::make_unique<ExternDecl>();
    extern_decl->name = ctx->IDENTIFIER()->getText();
    
    // Parse parameters
    if (ctx->param_list()) {
        for (auto param : ctx->param_list()->parameter()) {
            Type param_type = parseType(param->type_spec());
            std::string param_name = param->IDENTIFIER()->getText();
            extern_decl->params.emplace_back(param_type, param_name);
        }
    }
    
    // Parse return type
    if (ctx->type_spec()) {
        extern_decl->return_type = parseType(ctx->type_spec());
    } else {
        extern_decl->return_type = Type(TypeKind::VOID);
    }
    
    pushNode(std::move(extern_decl));
    return nullptr;
}

std::any ASTVisitor::visitLet_statement(OlangParser::Let_statementContext *ctx) {
    auto let_stmt = std::make_unique<LetStmt>();
    let_stmt->type = parseType(ctx->type_spec());
    let_stmt->name = ctx->IDENTIFIER()->getText();
    
    visit(ctx->expression());
    let_stmt->value = popNode();
    
    pushNode(std::move(let_stmt));
    return nullptr;
}

std::any ASTVisitor::visitReturn_statement(OlangParser::Return_statementContext *ctx) {
    auto return_stmt = std::make_unique<ReturnStmt>();
    
    if (ctx->expression()) {
        visit(ctx->expression());
        return_stmt->expr = popNode();
    }
    
    pushNode(std::move(return_stmt));
    return nullptr;
}

std::any ASTVisitor::visitExpr_statement(OlangParser::Expr_statementContext *ctx) {
    auto expr_stmt = std::make_unique<ExprStmt>();
    
    visit(ctx->expression());
    expr_stmt->expr = popNode();
    
    pushNode(std::move(expr_stmt));
    return nullptr;
}

std::any ASTVisitor::visitIf_statement(OlangParser::If_statementContext *ctx) {
    auto if_stmt = std::make_unique<IfStmt>();
    
    visit(ctx->expression());
    if_stmt->condition = popNode();
    
    // All statements in one array
    auto all_statements = ctx->statement();
    
    // Find else position (by searching for RBRACE and ELSE token)
    size_t else_pos = all_statements.size();
    if (ctx->ELSE()) {
        // Simplified handling: assume statements after first RBRACE belong to else branch
        // This needs more precise logic, but use simple method for now
        size_t rbrace_count = 0;
        for (size_t i = 0; i < ctx->children.size(); ++i) {
            if (auto terminal = dynamic_cast<antlr4::tree::TerminalNode*>(ctx->children[i])) {
                if (terminal->getSymbol()->getType() == OlangParser::RBRACE) {
                    rbrace_count++;
                    if (rbrace_count == 1) {
                        // After first RBRACE is else branch
                        // Count how many statements processed
                        size_t stmt_count = 0;
                        for (size_t j = 0; j < i; ++j) {
                            if (dynamic_cast<OlangParser::StatementContext*>(ctx->children[j])) {
                                stmt_count++;
                            }
                        }
                        else_pos = stmt_count;
                        break;
                    }
                }
            }
        }
    }
    
    // Process then branch
    for (size_t i = 0; i < else_pos; ++i) {
        visit(all_statements[i]);
        if_stmt->then_body.push_back(popNode());
    }
    
    // Process else branch
    for (size_t i = else_pos; i < all_statements.size(); ++i) {
        visit(all_statements[i]);
        if_stmt->else_body.push_back(popNode());
    }
    
    pushNode(std::move(if_stmt));
    return nullptr;
}

std::any ASTVisitor::visitWhile_statement(OlangParser::While_statementContext *ctx) {
    auto while_stmt = std::make_unique<WhileStmt>();
    
    visit(ctx->expression());
    while_stmt->condition = popNode();
    
    // Process loop body
    for (auto stmt : ctx->statement()) {
        visit(stmt);
        while_stmt->body.push_back(popNode());
    }
    
    pushNode(std::move(while_stmt));
    return nullptr;
}

std::any ASTVisitor::visitAssignment_expr(OlangParser::Assignment_exprContext *ctx) {
    if (ctx->ASSIGN()) {
        // Assignment expression
        visit(ctx->logical_or_expr());
        auto left = popNode();
        
        visit(ctx->assignment_expr());
        auto right = popNode();
        
        // Create assignment node
        auto assign_expr = std::make_unique<AssignmentExpr>(
            std::unique_ptr<Expr>(static_cast<Expr*>(left.release())),
            std::unique_ptr<Expr>(static_cast<Expr*>(right.release()))
        );
        pushNode(std::move(assign_expr));
    } else {
        visit(ctx->logical_or_expr());
    }
    return nullptr;
}

std::any ASTVisitor::visitLogical_or_expr(OlangParser::Logical_or_exprContext *ctx) {
    if (ctx->OR().size() > 0) {
        auto left = ctx->logical_and_expr(0);
        visit(left);
        auto left_node = popNode();
        
        for (size_t i = 1; i < ctx->logical_and_expr().size(); ++i) {
            visit(ctx->logical_and_expr(i));
            auto right_node = popNode();
            
            auto binary_expr = std::make_unique<BinaryExpr>(
                BinaryExpr::OR, 
                std::unique_ptr<Expr>(static_cast<Expr*>(left_node.release())), 
                std::unique_ptr<Expr>(static_cast<Expr*>(right_node.release()))
            );
            left_node = std::move(binary_expr);
        }
        
        pushNode(std::move(left_node));
    } else {
        visit(ctx->logical_and_expr(0));
    }
    return nullptr;
}

std::any ASTVisitor::visitLogical_and_expr(OlangParser::Logical_and_exprContext *ctx) {
    if (ctx->AND().size() > 0) {
        auto left = ctx->equality_expr(0);
        visit(left);
        auto left_node = popNode();
        
        for (size_t i = 1; i < ctx->equality_expr().size(); ++i) {
            visit(ctx->equality_expr(i));
            auto right_node = popNode();
            
            auto binary_expr = std::make_unique<BinaryExpr>(
                BinaryExpr::AND, 
                std::unique_ptr<Expr>(static_cast<Expr*>(left_node.release())), 
                std::unique_ptr<Expr>(static_cast<Expr*>(right_node.release()))
            );
            left_node = std::move(binary_expr);
        }
        
        pushNode(std::move(left_node));
    } else {
        visit(ctx->equality_expr(0));
    }
    return nullptr;
}

std::any ASTVisitor::visitEquality_expr(OlangParser::Equality_exprContext *ctx) {
    if (!ctx->EQUAL().empty() || !ctx->NOT_EQUAL().empty()) {
        visit(ctx->relational_expr(0));
        auto left_node = popNode();
        
        for (size_t i = 1; i < ctx->relational_expr().size(); ++i) {
            visit(ctx->relational_expr(i));
            auto right_node = popNode();
            
            BinaryExpr::Op op = (!ctx->EQUAL().empty()) ? BinaryExpr::EQ : BinaryExpr::NE;
            auto binary_expr = std::make_unique<BinaryExpr>(
                op, 
                std::unique_ptr<Expr>(static_cast<Expr*>(left_node.release())), 
                std::unique_ptr<Expr>(static_cast<Expr*>(right_node.release()))
            );
            left_node = std::move(binary_expr);
        }
        
        pushNode(std::move(left_node));
    } else {
        visit(ctx->relational_expr(0));
    }
    return nullptr;
}

std::any ASTVisitor::visitRelational_expr(OlangParser::Relational_exprContext *ctx) {
    if (!ctx->LESS().empty() || !ctx->GREATER().empty() || !ctx->LESS_EQUAL().empty() || !ctx->GREATER_EQUAL().empty()) {
        visit(ctx->additive_expr(0));
        auto left_node = popNode();
        
        for (size_t i = 1; i < ctx->additive_expr().size(); ++i) {
            visit(ctx->additive_expr(i));
            auto right_node = popNode();
            
            BinaryExpr::Op op = BinaryExpr::LT; // 默认值
            if (!ctx->LESS().empty()) op = BinaryExpr::LT;
            else if (!ctx->GREATER().empty()) op = BinaryExpr::GT;
            else if (!ctx->LESS_EQUAL().empty()) op = BinaryExpr::LE;
            else if (!ctx->GREATER_EQUAL().empty()) op = BinaryExpr::GE;
            
            auto binary_expr = std::make_unique<BinaryExpr>(
                op, 
                std::unique_ptr<Expr>(static_cast<Expr*>(left_node.release())), 
                std::unique_ptr<Expr>(static_cast<Expr*>(right_node.release()))
            );
            left_node = std::move(binary_expr);
        }
        
        pushNode(std::move(left_node));
    } else {
        visit(ctx->additive_expr(0));
    }
    return nullptr;
}

std::any ASTVisitor::visitAdditive_expr(OlangParser::Additive_exprContext *ctx) {
    if (!ctx->PLUS().empty() || !ctx->MINUS().empty()) {
        visit(ctx->multiplicative_expr(0));
        auto left_node = popNode();
        
        for (size_t i = 1; i < ctx->multiplicative_expr().size(); ++i) {
            visit(ctx->multiplicative_expr(i));
            auto right_node = popNode();
            
            BinaryExpr::Op op = (!ctx->PLUS().empty()) ? BinaryExpr::ADD : BinaryExpr::SUB;
            auto binary_expr = std::make_unique<BinaryExpr>(
                op, 
                std::unique_ptr<Expr>(static_cast<Expr*>(left_node.release())), 
                std::unique_ptr<Expr>(static_cast<Expr*>(right_node.release()))
            );
            left_node = std::move(binary_expr);
        }
        
        pushNode(std::move(left_node));
    } else {
        visit(ctx->multiplicative_expr(0));
    }
    return nullptr;
}

std::any ASTVisitor::visitMultiplicative_expr(OlangParser::Multiplicative_exprContext *ctx) {
    if (!ctx->MULTIPLY().empty() || !ctx->DIVIDE().empty() || !ctx->MODULO().empty()) {
        visit(ctx->unary_expr(0));
        auto left_node = popNode();
        
        for (size_t i = 1; i < ctx->unary_expr().size(); ++i) {
            visit(ctx->unary_expr(i));
            auto right_node = popNode();
            
            BinaryExpr::Op op = BinaryExpr::MUL; // Default
            if (!ctx->MULTIPLY().empty()) op = BinaryExpr::MUL;
            else if (!ctx->DIVIDE().empty()) op = BinaryExpr::DIV;
            else if (!ctx->MODULO().empty()) op = BinaryExpr::MOD;
            
            auto binary_expr = std::make_unique<BinaryExpr>(
                op, 
                std::unique_ptr<Expr>(static_cast<Expr*>(left_node.release())), 
                std::unique_ptr<Expr>(static_cast<Expr*>(right_node.release()))
            );
            left_node = std::move(binary_expr);
        }
        
        pushNode(std::move(left_node));
    } else {
        visit(ctx->unary_expr(0));
    }
    return nullptr;
}

std::any ASTVisitor::visitUnary_expr(OlangParser::Unary_exprContext *ctx) {
    if (ctx->NOT() || ctx->MINUS() || ctx->MULTIPLY() || ctx->AMPERSAND()) {
        visit(ctx->unary_expr());
        auto operand = popNode();
        
        UnaryExpr::Op op = UnaryExpr::NOT; // Default
        if (ctx->NOT()) op = UnaryExpr::NOT;
        else if (ctx->MINUS()) op = UnaryExpr::NEG;
        else if (ctx->MULTIPLY()) op = UnaryExpr::DEREF;
        else if (ctx->AMPERSAND()) op = UnaryExpr::ADDR;
        
        auto unary_expr = std::make_unique<UnaryExpr>(
            op, 
            std::unique_ptr<Expr>(static_cast<Expr*>(operand.release()))
        );
        pushNode(std::move(unary_expr));
    } else {
        visit(ctx->postfix_expr());
    }
    return nullptr;
}

std::any ASTVisitor::visitPostfix_expr(OlangParser::Postfix_exprContext *ctx) {
    visit(ctx->primary_expr());
    auto node = popNode();
    
    // Process postfix operators (array access, member access, function call)
    for (size_t i = 1; i < ctx->children.size(); ) {
        auto child = ctx->children[i];
        
        if (auto terminal = dynamic_cast<antlr4::tree::TerminalNode*>(child)) {
            auto token_type = terminal->getSymbol()->getType();
            
            // Function call '('
            if (token_type == OlangParser::LPAREN) {
                auto ident = dynamic_cast<Identifier*>(node.get());
                if (ident) {
                    std::vector<std::unique_ptr<Expr>> args;
                    
                    if (ctx->argument_list().size() > 0) {
                        auto arg_list = ctx->argument_list(0);
                        for (auto expr_ctx : arg_list->expression()) {
                            visit(expr_ctx);
                            auto arg = popNode();
                            args.push_back(std::unique_ptr<Expr>(static_cast<Expr*>(arg.release())));
                        }
                    }
                    
                    auto call_expr = std::make_unique<CallExpr>(ident->name, std::move(args));
                    node = std::move(call_expr);
                }
                i++;
            }
            // Member access '.'
            else if (token_type == OlangParser::DOT) {
                // Next should be IDENTIFIER
                if (i + 1 < ctx->children.size()) {
                    if (auto next_terminal = dynamic_cast<antlr4::tree::TerminalNode*>(ctx->children[i + 1])) {
                        if (next_terminal->getSymbol()->getType() == OlangParser::IDENTIFIER) {
                            std::string member_name = next_terminal->getText();
                            auto member_access = std::make_unique<MemberAccess>(
                                std::unique_ptr<Expr>(static_cast<Expr*>(node.release())),
                                member_name
                            );
                            node = std::move(member_access);
                            i += 2;  // Skip DOT and IDENTIFIER
                            continue;
                        }
                    }
                }
                i++;
            }
            // Array access '['
            else if (token_type == OlangParser::LBRACKET) {
                // Find corresponding expression
                if (ctx->expression().size() > 0) {
                    visit(ctx->expression(0));
                    auto index_expr = popNode();
                    auto array_access = std::make_unique<ArrayAccess>(
                        std::unique_ptr<Expr>(static_cast<Expr*>(node.release())),
                        std::unique_ptr<Expr>(static_cast<Expr*>(index_expr.release()))
                    );
                    node = std::move(array_access);
                }
                i++;
            }
            else {
                i++;
            }
        } else {
            i++;
        }
    }
    
    pushNode(std::move(node));
    return nullptr;
}

std::any ASTVisitor::visitPrimary_expr(OlangParser::Primary_exprContext *ctx) {
    if (ctx->INT_LITERAL()) {
        int64_t value = std::stoll(ctx->INT_LITERAL()->getText());
        auto int_lit = std::make_unique<IntLiteral>(value);
        pushNode(std::move(int_lit));
    } else if (ctx->FLOAT_LITERAL()) {
        double value = std::stod(ctx->FLOAT_LITERAL()->getText());
        auto float_lit = std::make_unique<FloatLiteral>(value);
        pushNode(std::move(float_lit));
    } else if (ctx->STRING_LITERAL()) {
        std::string value = ctx->STRING_LITERAL()->getText();
        value = value.substr(1, value.length() - 2); // Remove quotes
        auto string_lit = std::make_unique<StringLiteral>(value);
        pushNode(std::move(string_lit));
    } else if (ctx->TRUE()) {
        auto bool_lit = std::make_unique<BoolLiteral>(true);
        pushNode(std::move(bool_lit));
    } else if (ctx->FALSE()) {
        auto bool_lit = std::make_unique<BoolLiteral>(false);
        pushNode(std::move(bool_lit));
    } else if (ctx->IDENTIFIER()) {
        auto ident = std::make_unique<Identifier>(ctx->IDENTIFIER()->getText());
        pushNode(std::move(ident));
    } else if (ctx->LPAREN()) {
        visit(ctx->expression());
        // Parenthesized expression, no extra handling needed
    }
    
    return nullptr;
}

Type ASTVisitor::parseType(OlangParser::Type_specContext *ctx) {
    if (ctx->basic_type()) {
        auto basic = ctx->basic_type();
        if (basic->I1()) return Type(TypeKind::I1);
        else if (basic->I8()) return Type(TypeKind::I8);
        else if (basic->I16()) return Type(TypeKind::I16);
        else if (basic->I32()) return Type(TypeKind::I32);
        else if (basic->I64()) return Type(TypeKind::I64);
        else if (basic->F16()) return Type(TypeKind::F16);
        else if (basic->F32()) return Type(TypeKind::F32);
        else if (basic->F64()) return Type(TypeKind::F64);
    } else if (ctx->pointer_type()) {
        auto element_type = std::make_shared<Type>(parseType(ctx->pointer_type()->type_spec()));
        return Type(TypeKind::POINTER, element_type);
    } else if (ctx->array_type()) {
        int size = std::stoi(ctx->array_type()->INT_LITERAL()->getText());
        auto element_type = std::make_shared<Type>(parseType(ctx->array_type()->type_spec()));
        return Type(TypeKind::ARRAY, size, element_type);
    } else if (ctx->struct_type()) {
        return Type(TypeKind::STRUCT, ctx->struct_type()->IDENTIFIER()->getText());
    }
    
    return Type(TypeKind::VOID);
}

BinaryExpr::Op ASTVisitor::getBinaryOp(antlr4::Token* token) {
    switch (token->getType()) {
        case OlangParser::PLUS: return BinaryExpr::ADD;
        case OlangParser::MINUS: return BinaryExpr::SUB;
        case OlangParser::MULTIPLY: return BinaryExpr::MUL;
        case OlangParser::DIVIDE: return BinaryExpr::DIV;
        case OlangParser::MODULO: return BinaryExpr::MOD;
        case OlangParser::EQUAL: return BinaryExpr::EQ;
        case OlangParser::NOT_EQUAL: return BinaryExpr::NE;
        case OlangParser::LESS: return BinaryExpr::LT;
        case OlangParser::GREATER: return BinaryExpr::GT;
        case OlangParser::LESS_EQUAL: return BinaryExpr::LE;
        case OlangParser::GREATER_EQUAL: return BinaryExpr::GE;
        case OlangParser::AND: return BinaryExpr::AND;
        case OlangParser::OR: return BinaryExpr::OR;
        default: throw std::runtime_error("Unknown binary operator");
    }
}

UnaryExpr::Op ASTVisitor::getUnaryOp(antlr4::Token* token) {
    switch (token->getType()) {
        case OlangParser::NOT: return UnaryExpr::NOT;
        case OlangParser::MINUS: return UnaryExpr::NEG;
        case OlangParser::MULTIPLY: return UnaryExpr::DEREF;
        case OlangParser::AMPERSAND: return UnaryExpr::ADDR;
        default: throw std::runtime_error("Unknown unary operator");
    }
}

} // namespace olang
