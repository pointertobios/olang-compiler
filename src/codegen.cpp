#include "codegen.h"
#include <llvm/IR/Verifier.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/TargetParser/Host.h>

namespace olang {

llvm::Value* Program::codegen(CodeGenContext& ctx) {
    // Generate all struct declarations
    for (auto& decl : declarations) {
        if (auto struct_decl = dynamic_cast<StructDecl*>(decl.get())) {
            struct_decl->codegen(ctx);
        }
    }
    
    // Generate all external function declarations
    for (auto& decl : declarations) {
        if (auto extern_decl = dynamic_cast<ExternDecl*>(decl.get())) {
            extern_decl->codegen(ctx);
        }
    }
    
    // Generate all function declarations
    for (auto& decl : declarations) {
        if (auto func_decl = dynamic_cast<FunctionDecl*>(decl.get())) {
            func_decl->codegen(ctx);
        }
    }
    
    return nullptr;
}

llvm::Value* StructDecl::codegen(CodeGenContext& ctx) {
    std::vector<llvm::Type*> field_types;
    for (const auto& field : fields) {
        field_types.push_back(ctx.getLLVMType(field.first));
    }
    
    llvm::StructType* struct_type = llvm::StructType::create(
        ctx.getContext(), field_types, name
    );
    
    ctx.addStructType(name, Type(TypeKind::STRUCT, name), struct_type);
    
    return nullptr;
}

llvm::Value* FunctionDecl::codegen(CodeGenContext& ctx) {
    // Prepare parameter types
    std::vector<llvm::Type*> param_types;
    for (const auto& param : params) {
        param_types.push_back(ctx.getLLVMType(param.first));
    }
    
    llvm::FunctionType* func_type = llvm::FunctionType::get(
        ctx.getLLVMType(return_type), param_types, false
    );
    
    // Set linkage type: export uses ExternalLinkage, otherwise InternalLinkage
    llvm::GlobalValue::LinkageTypes linkage = is_export ? 
        llvm::Function::ExternalLinkage : llvm::Function::InternalLinkage;
    
    llvm::Function* function = llvm::Function::Create(
        func_type, linkage, name, ctx.getModule()
    );
    
    // Create basic block
    llvm::BasicBlock* entry_block = llvm::BasicBlock::Create(
        ctx.getContext(), "entry", function
    );
    ctx.getBuilder().SetInsertPoint(entry_block);
    
    // Enter new scope
    ctx.enterScope();
    
    // Create alloca for parameters and save SSA values
    auto arg_iter = function->arg_begin();
    for (const auto& param : params) {
        llvm::AllocaInst* alloca = ctx.createAlloca(param.second, ctx.getLLVMType(param.first));
        ctx.getBuilder().CreateStore(&*arg_iter, alloca);
        // Also save parameter SSA value (for struct member access)
        ctx.setValue(param.second, &*arg_iter);
        arg_iter++;
    }
    
    // Generate function body
    for (auto& stmt : body) {
        stmt->codegen(ctx);
    }
    
    // Check if basic block is already terminated (i.e., has return statement)
    llvm::BasicBlock* current_bb = ctx.getBuilder().GetInsertBlock();
    if (!current_bb->getTerminator()) {
        // Add default return if function has no return statement
        if (return_type.kind == TypeKind::VOID) {
            ctx.getBuilder().CreateRetVoid();
        } else {
            // For non-void function, return default value if no return statement
            llvm::Value* default_value = nullptr;
            switch (return_type.kind) {
                case TypeKind::I1: default_value = llvm::ConstantInt::getFalse(ctx.getContext()); break;
                case TypeKind::I8: default_value = llvm::ConstantInt::get(ctx.getContext(), llvm::APInt(8, 0)); break;
                case TypeKind::I16: default_value = llvm::ConstantInt::get(ctx.getContext(), llvm::APInt(16, 0)); break;
                case TypeKind::I32: default_value = llvm::ConstantInt::get(ctx.getContext(), llvm::APInt(32, 0)); break;
                case TypeKind::I64: default_value = llvm::ConstantInt::get(ctx.getContext(), llvm::APInt(64, 0)); break;
                case TypeKind::F32: default_value = llvm::ConstantFP::get(ctx.getContext(), llvm::APFloat(0.0f)); break;
                case TypeKind::F64: default_value = llvm::ConstantFP::get(ctx.getContext(), llvm::APFloat(0.0)); break;
                default: break;
            }
            if (default_value) {
                ctx.getBuilder().CreateRet(default_value);
            }
        }
    }
    
    // Exit scope
    ctx.exitScope();
    
    return function;
}

llvm::Value* ExternDecl::codegen(CodeGenContext& ctx) {
    // Prepare parameter types
    std::vector<llvm::Type*> param_types;
    for (const auto& param : params) {
        param_types.push_back(ctx.getLLVMType(param.first));
    }
    
    llvm::FunctionType* func_type = llvm::FunctionType::get(
        ctx.getLLVMType(return_type), param_types, false
    );
    
    // Create external function declaration (declare only, no definition)
    llvm::Function* function = llvm::Function::Create(
        func_type, llvm::Function::ExternalLinkage, name, ctx.getModule()
    );
    
    return function;
}

llvm::Value* LetStmt::codegen(CodeGenContext& ctx) {
    llvm::Type* llvm_type = ctx.getLLVMType(type);
    llvm::AllocaInst* alloca = ctx.createAlloca(name, llvm_type);
    
    // For arrays and structs, always use zero initialization
    // (initializer expression is just a placeholder in Olang syntax)
    if (llvm_type->isStructTy() || llvm_type->isArrayTy()) {
        llvm::Value* zero_init = llvm::ConstantAggregateZero::get(llvm_type);
        ctx.getBuilder().CreateStore(zero_init, alloca);
        return alloca;
    }
    
    // For scalar types, evaluate and store the value
    llvm::Value* value = this->value->codegen(ctx);
    if (!value) {
        return nullptr; // Error
    }
    
    ctx.getBuilder().CreateStore(value, alloca);
    return alloca;
}

llvm::Value* ReturnStmt::codegen(CodeGenContext& ctx) {
    if (expr) {
        llvm::Value* return_value = expr->codegen(ctx);
        return ctx.getBuilder().CreateRet(return_value);
    } else {
        return ctx.getBuilder().CreateRetVoid();
    }
}

llvm::Value* ExprStmt::codegen(CodeGenContext& ctx) {
    return expr->codegen(ctx);
}

llvm::Value* IfStmt::codegen(CodeGenContext& ctx) {
    llvm::Value* cond_value = condition->codegen(ctx);
    
    llvm::Function* function = ctx.getBuilder().GetInsertBlock()->getParent();
    
    llvm::BasicBlock* then_block = llvm::BasicBlock::Create(ctx.getContext(), "then", function);
    llvm::BasicBlock* else_block = llvm::BasicBlock::Create(ctx.getContext(), "else");
    llvm::BasicBlock* merge_block = llvm::BasicBlock::Create(ctx.getContext(), "merge");
    
    ctx.getBuilder().CreateCondBr(cond_value, then_block, else_block);
    
    // Generate then branch
    ctx.getBuilder().SetInsertPoint(then_block);
    ctx.enterScope();
    for (auto& stmt : then_body) {
        stmt->codegen(ctx);
    }
    ctx.exitScope();
    // Check if current insertion point block is terminated
    llvm::BasicBlock* current_then = ctx.getBuilder().GetInsertBlock();
    if (!current_then->getTerminator()) {
        ctx.getBuilder().CreateBr(merge_block);
    }
    
    // Generate else branch
    if (!else_body.empty()) {
        else_block->insertInto(function);
        ctx.getBuilder().SetInsertPoint(else_block);
        ctx.enterScope();
        for (auto& stmt : else_body) {
            stmt->codegen(ctx);
        }
        ctx.exitScope();
        llvm::BasicBlock* current_else = ctx.getBuilder().GetInsertBlock();
        if (!current_else->getTerminator()) {
            ctx.getBuilder().CreateBr(merge_block);
        }
    } else {
        else_block->insertInto(function);
        ctx.getBuilder().SetInsertPoint(else_block);
        ctx.getBuilder().CreateBr(merge_block);
    }
    
    // Insert and set merge block only if it has predecessors
    if (merge_block->hasNPredecessorsOrMore(1)) {
        merge_block->insertInto(function);
        ctx.getBuilder().SetInsertPoint(merge_block);
    } else {
        // Merge block not used, delete it
        delete merge_block;
    }
    
    return nullptr;
}

llvm::Value* WhileStmt::codegen(CodeGenContext& ctx) {
    llvm::Function* function = ctx.getBuilder().GetInsertBlock()->getParent();
    
    llvm::BasicBlock* cond_block = llvm::BasicBlock::Create(ctx.getContext(), "while_cond", function);
    llvm::BasicBlock* body_block = llvm::BasicBlock::Create(ctx.getContext(), "while_body", function);
    llvm::BasicBlock* end_block = llvm::BasicBlock::Create(ctx.getContext(), "while_end", function);
    
    // Jump to condition block
    ctx.getBuilder().CreateBr(cond_block);
    
    // Condition block
    ctx.getBuilder().SetInsertPoint(cond_block);
    llvm::Value* cond_value = condition->codegen(ctx);
    ctx.getBuilder().CreateCondBr(cond_value, body_block, end_block);
    
    // Loop body block
    ctx.getBuilder().SetInsertPoint(body_block);
    ctx.enterScope();
    for (auto& stmt : body) {
        stmt->codegen(ctx);
    }
    ctx.exitScope();
    
    // Jump back to condition after loop body (check current insertion point block)
    llvm::BasicBlock* current_block = ctx.getBuilder().GetInsertBlock();
    if (!current_block->getTerminator()) {
        ctx.getBuilder().CreateBr(cond_block);
    }
    
    // End block
    ctx.getBuilder().SetInsertPoint(end_block);
    
    return nullptr;
}

// Expression code generation
llvm::Value* IntLiteral::codegen(CodeGenContext& ctx) {
    // Default to i32 type (most common)
    return llvm::ConstantInt::get(ctx.getContext(), llvm::APInt(32, value, true));
}

llvm::Value* FloatLiteral::codegen(CodeGenContext& ctx) {
    return llvm::ConstantFP::get(ctx.getContext(), llvm::APFloat(value));
}

llvm::Value* StringLiteral::codegen(CodeGenContext& ctx) {
    return ctx.getBuilder().CreateGlobalStringPtr(value);
}

llvm::Value* BoolLiteral::codegen(CodeGenContext& ctx) {
    return llvm::ConstantInt::getBool(ctx.getContext(), value);
}

llvm::Value* Identifier::codegen(CodeGenContext& ctx) {
    llvm::AllocaInst* alloca = ctx.getAlloca(name);
    if (alloca) {
        return ctx.getBuilder().CreateLoad(alloca->getAllocatedType(), alloca, name);
    }
    return nullptr;
}

llvm::Value* BinaryExpr::codegen(CodeGenContext& ctx) {
    llvm::Value* left_value = left->codegen(ctx);
    llvm::Value* right_value = right->codegen(ctx);
    
    if (!left_value || !right_value) {
        return nullptr;
    }
    
    switch (op) {
        case ADD:
            if (left_value->getType()->isFloatingPointTy()) {
                return ctx.getBuilder().CreateFAdd(left_value, right_value, "addtmp");
            } else {
                return ctx.getBuilder().CreateAdd(left_value, right_value, "addtmp");
            }
        case SUB:
            if (left_value->getType()->isFloatingPointTy()) {
                return ctx.getBuilder().CreateFSub(left_value, right_value, "subtmp");
            } else {
                return ctx.getBuilder().CreateSub(left_value, right_value, "subtmp");
            }
        case MUL:
            if (left_value->getType()->isFloatingPointTy()) {
                return ctx.getBuilder().CreateFMul(left_value, right_value, "multmp");
            } else {
                return ctx.getBuilder().CreateMul(left_value, right_value, "multmp");
            }
        case DIV:
            if (left_value->getType()->isFloatingPointTy()) {
                return ctx.getBuilder().CreateFDiv(left_value, right_value, "divtmp");
            } else {
                return ctx.getBuilder().CreateSDiv(left_value, right_value, "divtmp");
            }
        case MOD:
            return ctx.getBuilder().CreateSRem(left_value, right_value, "modtmp");
        case EQ:
            if (left_value->getType()->isFloatingPointTy()) {
                return ctx.getBuilder().CreateFCmpOEQ(left_value, right_value, "eqtmp");
            } else {
                return ctx.getBuilder().CreateICmpEQ(left_value, right_value, "eqtmp");
            }
        case NE:
            if (left_value->getType()->isFloatingPointTy()) {
                return ctx.getBuilder().CreateFCmpONE(left_value, right_value, "netmp");
            } else {
                return ctx.getBuilder().CreateICmpNE(left_value, right_value, "netmp");
            }
        case LT:
            if (left_value->getType()->isFloatingPointTy()) {
                return ctx.getBuilder().CreateFCmpOLT(left_value, right_value, "lttmp");
            } else {
                return ctx.getBuilder().CreateICmpSLT(left_value, right_value, "lttmp");
            }
        case GT:
            if (left_value->getType()->isFloatingPointTy()) {
                return ctx.getBuilder().CreateFCmpOGT(left_value, right_value, "gttmp");
            } else {
                return ctx.getBuilder().CreateICmpSGT(left_value, right_value, "gttmp");
            }
        case LE:
            if (left_value->getType()->isFloatingPointTy()) {
                return ctx.getBuilder().CreateFCmpOLE(left_value, right_value, "letmp");
            } else {
                return ctx.getBuilder().CreateICmpSLE(left_value, right_value, "letmp");
            }
        case GE:
            if (left_value->getType()->isFloatingPointTy()) {
                return ctx.getBuilder().CreateFCmpOGE(left_value, right_value, "getmp");
            } else {
                return ctx.getBuilder().CreateICmpSGE(left_value, right_value, "getmp");
            }
        case AND:
            return ctx.getBuilder().CreateAnd(left_value, right_value, "andtmp");
        case OR:
            return ctx.getBuilder().CreateOr(left_value, right_value, "ortmp");
        default:
            return nullptr;
    }
}

llvm::Value* AssignmentExpr::codegen(CodeGenContext& ctx) {
    // Calculate right value
    llvm::Value* right_value = right->codegen(ctx);
    if (!right_value) {
        return nullptr;
    }
    
    // Handle simple variable assignment: x = value
    if (auto ident = dynamic_cast<Identifier*>(left.get())) {
        llvm::AllocaInst* alloca = ctx.getAlloca(ident->name);
        if (alloca) {
            ctx.getBuilder().CreateStore(right_value, alloca);
            return right_value;
        }
    }
    
    // Handle array element assignment: arr[i] = value
    if (auto array_access = dynamic_cast<ArrayAccess*>(left.get())) {
        if (auto ident = dynamic_cast<Identifier*>(array_access->array.get())) {
            llvm::AllocaInst* alloca = ctx.getAlloca(ident->name);
            if (alloca) {
                llvm::Type* array_type = alloca->getAllocatedType();
                if (array_type->isArrayTy()) {
                    llvm::Value* index_value = array_access->index->codegen(ctx);
                    
                    // Create GEP to get element pointer
                    std::vector<llvm::Value*> indices;
                    indices.push_back(llvm::ConstantInt::get(ctx.getContext(), llvm::APInt(32, 0)));
                    indices.push_back(index_value);
                    
                    llvm::Value* element_ptr = ctx.getBuilder().CreateGEP(
                        array_type, alloca, indices, "arrayidx"
                    );
                    
                    // Store value to array element
                    ctx.getBuilder().CreateStore(right_value, element_ptr);
                    return right_value;
                }
            }
        }
    }
    
    // Handle struct member assignment: obj.member = value
    if (auto member_access = dynamic_cast<MemberAccess*>(left.get())) {
        if (auto ident = dynamic_cast<Identifier*>(member_access->object.get())) {
            llvm::AllocaInst* alloca = ctx.getAlloca(ident->name);
            if (alloca) {
                llvm::Type* struct_type = alloca->getAllocatedType();
                if (struct_type->isStructTy()) {
                    llvm::StructType* llvm_struct = llvm::cast<llvm::StructType>(struct_type);
                    
                    // Find member index
                    unsigned member_idx = 0;
                    if (member_access->member == "x") member_idx = 0;
                    else if (member_access->member == "y") member_idx = 1;
                    else if (member_access->member == "z") member_idx = 2;
                    else return nullptr;
                    
                    // Get member pointer using GEP
                    llvm::Value* member_ptr = ctx.getBuilder().CreateStructGEP(
                        llvm_struct, alloca, member_idx, member_access->member
                    );
                    
                    // Store value to member
                    ctx.getBuilder().CreateStore(right_value, member_ptr);
                    return right_value;
                }
            }
        }
        
        // Handle arr[i].member = value
        if (auto array_access = dynamic_cast<ArrayAccess*>(member_access->object.get())) {
            if (auto ident = dynamic_cast<Identifier*>(array_access->array.get())) {
                llvm::AllocaInst* alloca = ctx.getAlloca(ident->name);
                if (alloca) {
                    llvm::Type* array_type = alloca->getAllocatedType();
                    if (array_type->isArrayTy()) {
                        llvm::Value* index_value = array_access->index->codegen(ctx);
                        
                        // Get array element pointer
                        std::vector<llvm::Value*> indices;
                        indices.push_back(llvm::ConstantInt::get(ctx.getContext(), llvm::APInt(32, 0)));
                        indices.push_back(index_value);
                        
                        llvm::Value* element_ptr = ctx.getBuilder().CreateGEP(
                            array_type, alloca, indices, "arrayidx"
                        );
                        
                        // Get element type (should be struct)
                        llvm::ArrayType* arr_type = llvm::cast<llvm::ArrayType>(array_type);
                        llvm::Type* element_type = arr_type->getElementType();
                        
                        if (element_type->isStructTy()) {
                            llvm::StructType* llvm_struct = llvm::cast<llvm::StructType>(element_type);
                            
                            // Find member index
                            unsigned member_idx = 0;
                            if (member_access->member == "x") member_idx = 0;
                            else if (member_access->member == "y") member_idx = 1;
                            else if (member_access->member == "z") member_idx = 2;
                            else return nullptr;
                            
                            // Get member pointer from array element
                            llvm::Value* member_ptr = ctx.getBuilder().CreateStructGEP(
                                llvm_struct, element_ptr, member_idx, member_access->member
                            );
                            
                            // Store value to member
                            ctx.getBuilder().CreateStore(right_value, member_ptr);
                            return right_value;
                        }
                    }
                }
            }
        }
    }
    
    return nullptr;
}

llvm::Value* UnaryExpr::codegen(CodeGenContext& ctx) {
    llvm::Value* operand_value = operand->codegen(ctx);
    
    if (!operand_value) {
        return nullptr;
    }
    
    switch (op) {
        case NOT:
            return ctx.getBuilder().CreateNot(operand_value, "nottmp");
        case NEG:
            if (operand_value->getType()->isFloatingPointTy()) {
                return ctx.getBuilder().CreateFNeg(operand_value, "negtmp");
            } else {
                return ctx.getBuilder().CreateNeg(operand_value, "negtmp");
            }
        case DEREF:
            // For opaque pointers in LLVM 18+, we need to specify the type explicitly
            // This is a simplified implementation - proper pointer types should be tracked
            return ctx.getBuilder().CreateLoad(llvm::Type::getInt32Ty(ctx.getContext()), operand_value, "dereftmp");
        case ADDR:
            if (auto ident = dynamic_cast<Identifier*>(operand.get())) {
                llvm::AllocaInst* alloca = ctx.getAlloca(ident->name);
                if (alloca) {
                    return alloca;
                }
            }
            return nullptr;
        default:
            return nullptr;
    }
}

llvm::Value* CallExpr::codegen(CodeGenContext& ctx) {
    llvm::Function* callee = ctx.getModule()->getFunction(function_name);
    if (!callee) {
        return nullptr;
    }
    
    std::vector<llvm::Value*> arg_values;
    for (auto& arg : args) {
        arg_values.push_back(arg->codegen(ctx));
    }
    
    // Don't name call result if void function
    if (callee->getReturnType()->isVoidTy()) {
        return ctx.getBuilder().CreateCall(callee, arg_values);
    } else {
        return ctx.getBuilder().CreateCall(callee, arg_values, "calltmp");
    }
}

llvm::Value* MemberAccess::codegen(CodeGenContext& ctx) {
    // Handle simple struct variable: obj.member
    if (auto ident = dynamic_cast<Identifier*>(object.get())) {
        llvm::AllocaInst* alloca = ctx.getAlloca(ident->name);
        if (alloca) {
            llvm::Type* struct_type = alloca->getAllocatedType();
            if (struct_type->isStructTy()) {
                llvm::StructType* llvm_struct = llvm::cast<llvm::StructType>(struct_type);
                
                // Find member index
                unsigned member_idx = 0;
                if (member == "x") member_idx = 0;
                else if (member == "y") member_idx = 1;
                else if (member == "z") member_idx = 2;
                else return nullptr;
                
                // Use GEP to access member
                llvm::Value* member_ptr = ctx.getBuilder().CreateStructGEP(llvm_struct, alloca, member_idx, member);
                llvm::Type* member_type = llvm_struct->getElementType(member_idx);
                return ctx.getBuilder().CreateLoad(member_type, member_ptr, member);
            }
        }
        
        // If parameter (SSA value), use ExtractValue
        llvm::Value* param_value = ctx.getValue(ident->name);
        if (param_value && param_value->getType()->isStructTy()) {
            unsigned member_idx = 0;
            if (member == "x") member_idx = 0;
            else if (member == "y") member_idx = 1;
            else if (member == "z") member_idx = 2;
            else return nullptr;
            
            return ctx.getBuilder().CreateExtractValue(param_value, member_idx, member);
        }
    }
    
    // Handle array element member: arr[i].member
    if (auto array_access = dynamic_cast<ArrayAccess*>(object.get())) {
        if (auto ident = dynamic_cast<Identifier*>(array_access->array.get())) {
            llvm::AllocaInst* alloca = ctx.getAlloca(ident->name);
            if (alloca) {
                llvm::Type* array_type = alloca->getAllocatedType();
                if (array_type->isArrayTy()) {
                    llvm::Value* index_value = array_access->index->codegen(ctx);
                    
                    // Get array element pointer
                    std::vector<llvm::Value*> indices;
                    indices.push_back(llvm::ConstantInt::get(ctx.getContext(), llvm::APInt(32, 0)));
                    indices.push_back(index_value);
                    
                    llvm::Value* element_ptr = ctx.getBuilder().CreateGEP(
                        array_type, alloca, indices, "arrayidx"
                    );
                    
                    // Get element type (should be struct)
                    llvm::ArrayType* arr_type = llvm::cast<llvm::ArrayType>(array_type);
                    llvm::Type* element_type = arr_type->getElementType();
                    
                    if (element_type->isStructTy()) {
                        llvm::StructType* llvm_struct = llvm::cast<llvm::StructType>(element_type);
                        
                        // Find member index
                        unsigned member_idx = 0;
                        if (member == "x") member_idx = 0;
                        else if (member == "y") member_idx = 1;
                        else if (member == "z") member_idx = 2;
                        else return nullptr;
                        
                        // Get member pointer from array element
                        llvm::Value* member_ptr = ctx.getBuilder().CreateStructGEP(
                            llvm_struct, element_ptr, member_idx, member
                        );
                        
                        // Load member value
                        llvm::Type* member_type = llvm_struct->getElementType(member_idx);
                        return ctx.getBuilder().CreateLoad(member_type, member_ptr, member);
                    }
                }
            }
        }
    }
    
    // Generic handling: extract from expression value
    llvm::Value* object_value = object->codegen(ctx);
    if (!object_value || !object_value->getType()->isStructTy()) {
        return nullptr;
    }
    
    unsigned member_idx = 0;
    if (member == "x") member_idx = 0;
    else if (member == "y") member_idx = 1;
    else if (member == "z") member_idx = 2;
    else return nullptr;
    
    return ctx.getBuilder().CreateExtractValue(object_value, member_idx, member);
}

llvm::Value* ArrayAccess::codegen(CodeGenContext& ctx) {
    // Special handling: if array is Identifier, get its alloca
    if (auto ident = dynamic_cast<Identifier*>(array.get())) {
        llvm::AllocaInst* alloca = ctx.getAlloca(ident->name);
        if (alloca) {
            llvm::Type* array_type = alloca->getAllocatedType();
            if (array_type->isArrayTy()) {
                llvm::Value* index_value = index->codegen(ctx);
                
                // Create GEP instruction to access array element
                std::vector<llvm::Value*> indices;
                indices.push_back(llvm::ConstantInt::get(ctx.getContext(), llvm::APInt(32, 0)));
                indices.push_back(index_value);
                
                llvm::Value* element_ptr = ctx.getBuilder().CreateGEP(
                    array_type, alloca, indices, "arrayidx"
                );
                
                llvm::ArrayType* arr_type = llvm::cast<llvm::ArrayType>(array_type);
                return ctx.getBuilder().CreateLoad(arr_type->getElementType(), element_ptr, "arrayload");
            }
        }
    }
    
    return nullptr;
}

void CodeGenContext::setTargetTriple(const std::string& triple) {
    module->setTargetTriple(triple);
}

bool CodeGenContext::emitObjectFile(const std::string& filename, const std::string& target_triple) {
    // Only initialize native target (avoid linking all architecture libraries)
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmParser();
    llvm::InitializeNativeTargetAsmPrinter();
    
    // Get target triple
    std::string triple = target_triple.empty() ? 
        llvm::sys::getDefaultTargetTriple() : target_triple;
    module->setTargetTriple(triple);
    
    std::string error;
    auto target = llvm::TargetRegistry::lookupTarget(triple, error);
    
    if (!target) {
        llvm::errs() << error;
        return false;
    }
    
    auto cpu = "generic";
    auto features = "";
    
    llvm::TargetOptions opt;
    auto target_machine = target->createTargetMachine(
        triple, cpu, features, opt, llvm::Reloc::PIC_
    );
    
    module->setDataLayout(target_machine->createDataLayout());
    
    // Open file
    std::error_code EC;
    llvm::raw_fd_ostream dest(filename, EC, llvm::sys::fs::OF_None);
    
    if (EC) {
        llvm::errs() << "Could not open file: " << EC.message();
        return false;
    }
    
    // Generate object file
    llvm::legacy::PassManager pass;
    auto file_type = llvm::CodeGenFileType::ObjectFile;
    
    if (target_machine->addPassesToEmitFile(pass, dest, nullptr, file_type)) {
        llvm::errs() << "TargetMachine can't emit a file of this type";
        return false;
    }
    
    pass.run(*module);
    dest.flush();
    
    return true;
}

} // namespace olang
