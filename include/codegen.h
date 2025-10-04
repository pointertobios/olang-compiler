#pragma once
#include "ast.h"
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Verifier.h>
#include <unordered_map>
#include <string>
#include <memory>

namespace olang {

class CodeGenContext {
private:
    llvm::LLVMContext& context;
    std::unique_ptr<llvm::Module> module;
    llvm::IRBuilder<> builder;
    
    // Symbol table - support SSA/alloca
    std::vector<std::unordered_map<std::string, llvm::AllocaInst*>> alloca_table;
    std::vector<std::unordered_map<std::string, llvm::Value*>> value_table;
    
    // Type table
    std::unordered_map<std::string, Type> struct_types;
    std::unordered_map<std::string, llvm::StructType*> llvm_struct_types;
    
public:
    CodeGenContext(llvm::LLVMContext& ctx) 
        : context(ctx), module(std::make_unique<llvm::Module>("olang", ctx)), builder(ctx) {
        alloca_table.push_back({});
        value_table.push_back({});
    }
    
    llvm::LLVMContext& getContext() { return context; }
    llvm::Module* getModule() { return module.get(); }
    llvm::IRBuilder<>& getBuilder() { return builder; }
    
    // SSA/Alloca management
    void enterScope() {
        alloca_table.push_back({});
        value_table.push_back({});
    }
    
    void exitScope() {
        alloca_table.pop_back();
        value_table.pop_back();
    }
    
    llvm::AllocaInst* createAlloca(const std::string& name, llvm::Type* type) {
        llvm::Function* function = builder.GetInsertBlock()->getParent();
        llvm::IRBuilder<> tmp_builder(&function->getEntryBlock(), function->getEntryBlock().begin());
        llvm::AllocaInst* alloca = tmp_builder.CreateAlloca(type, nullptr, name);
        alloca_table.back()[name] = alloca;
        return alloca;
    }
    
    llvm::AllocaInst* getAlloca(const std::string& name) {
        for (auto it = alloca_table.rbegin(); it != alloca_table.rend(); ++it) {
            if (it->find(name) != it->end()) {
                return (*it)[name];
            }
        }
        return nullptr;
    }
    
    void setValue(const std::string& name, llvm::Value* value) {
        value_table.back()[name] = value;
    }
    
    llvm::Value* getValue(const std::string& name) {
        for (auto it = value_table.rbegin(); it != value_table.rend(); ++it) {
            if (it->find(name) != it->end()) {
                return (*it)[name];
            }
        }
        return nullptr;
    }
    
    // Type conversion
    llvm::Type* getLLVMType(const Type& type) {
        switch (type.kind) {
            case TypeKind::I1: return llvm::Type::getInt1Ty(context);
            case TypeKind::I8: return llvm::Type::getInt8Ty(context);
            case TypeKind::I16: return llvm::Type::getInt16Ty(context);
            case TypeKind::I32: return llvm::Type::getInt32Ty(context);
            case TypeKind::I64: return llvm::Type::getInt64Ty(context);
            case TypeKind::F16: return llvm::Type::getHalfTy(context);
            case TypeKind::F32: return llvm::Type::getFloatTy(context);
            case TypeKind::F64: return llvm::Type::getDoubleTy(context);
            case TypeKind::POINTER: return llvm::PointerType::get(getLLVMType(*type.element_type), 0);
            case TypeKind::ARRAY: return llvm::ArrayType::get(getLLVMType(*type.element_type), type.array_size);
            case TypeKind::STRUCT: {
                if (llvm_struct_types.find(type.name) != llvm_struct_types.end()) {
                    return llvm_struct_types[type.name];
                }
                return nullptr;
            }
            case TypeKind::VOID: return llvm::Type::getVoidTy(context);
            default: return nullptr;
        }
    }
    
    void addStructType(const std::string& name, const Type& type, llvm::StructType* llvm_type) {
        struct_types[name] = type;
        llvm_struct_types[name] = llvm_type;
    }
    
    Type* getStructType(const std::string& name) {
        auto it = struct_types.find(name);
        return (it != struct_types.end()) ? &it->second : nullptr;
    }
    
    llvm::StructType* getLLVMStructType(const std::string& name) {
        auto it = llvm_struct_types.find(name);
        return (it != llvm_struct_types.end()) ? it->second : nullptr;
    }
    
    void printIR() {
        module->print(llvm::errs(), nullptr);
    }
    
    void optimizeAndEmit(const std::string& filename) {
        std::error_code EC;
        llvm::raw_fd_ostream OS(filename, EC);
        module->print(OS, nullptr);
    }
    
    void setTargetTriple(const std::string& triple);
    bool emitObjectFile(const std::string& filename, const std::string& target_triple = "");
    
    bool verifyModule() {
        std::string ErrorStr;
        llvm::raw_string_ostream OS(ErrorStr);
        if (llvm::verifyModule(*module, &OS)) {
            llvm::errs() << "Module verification failed:\n" << ErrorStr << "\n";
            return false;
        }
        return true;
    }
};

} // namespace olang
