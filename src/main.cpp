#include "visitor.h"
#include "codegen.h"
#include "OlangLexer.h"
#include "OlangParser.h"
#include <iostream>
#include <fstream>
#include <memory>
#include <llvm/Support/raw_ostream.h>
#include <llvm/IR/Module.h>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <input_file> [options]" << std::endl;
        std::cerr << "Options:" << std::endl;
        std::cerr << "  --emit-llvm       Generate LLVM IR (.ll)" << std::endl;
        std::cerr << "  -o <output>       Specify output file" << std::endl;
        std::cerr << "  --target <triple> Specify target triple" << std::endl;
        std::cerr << "  --print-ir        Print LLVM IR to stdout" << std::endl;
        std::cerr << "" << std::endl;
        std::cerr << "Default: Generate object file (.o)" << std::endl;
        std::cerr << "Linking: Use ld.lld or clang to link .o files" << std::endl;
        return 1;
    }
    
    std::string filename = argv[1];
    std::string output_file = "";
    std::string target_triple = "";
    bool emit_llvm = false;
    bool print_ir = false;
    
    // Parse arguments
    for (int i = 2; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--emit-llvm") {
            emit_llvm = true;
        } else if (arg == "-o" && i + 1 < argc) {
            output_file = argv[++i];
        } else if (arg == "--target" && i + 1 < argc) {
            target_triple = argv[++i];
        } else if (arg == "--print-ir") {
            print_ir = true;
        }
    }
    
    // Generate default filename if output file not specified
    if (output_file.empty()) {
        std::string base = filename.substr(0, filename.find_last_of('.'));
        if (emit_llvm) {
            output_file = base + ".ll";
        } else {
            output_file = base + ".o";  // Default: generate .o
        }
    }
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open file " << filename << std::endl;
        return 1;
    }
    
    // Read file content
    std::string input((std::istreambuf_iterator<char>(file)),
                      std::istreambuf_iterator<char>());
    file.close();
    
    try {
        // Create ANTLR input stream
        antlr4::ANTLRInputStream input_stream(input);
        
        // Create lexer
        OlangLexer lexer(&input_stream);
        
        // Create token stream
        antlr4::CommonTokenStream tokens(&lexer);
        
        // Create parser
        OlangParser parser(&tokens);
        
        // Start parsing
        OlangParser::ProgramContext* tree = parser.program();
        
        if (parser.getNumberOfSyntaxErrors() > 0) {
            std::cerr << "Syntax errors found!" << std::endl;
            return 1;
        }
        
        // Create AST visitor
        olang::ASTVisitor visitor;
        
        // Visit AST
        visitor.visitProgram(tree);
        auto program_node = visitor.popNode();
        
        if (!program_node) {
            std::cerr << "Failed to create AST!" << std::endl;
            return 1;
        }
        
        // Create code generation context
        llvm::LLVMContext context;
        olang::CodeGenContext codegen_ctx(context);
        
        // Generate LLVM IR
        program_node->codegen(codegen_ctx);
        
        // Set target triple if specified
        if (!target_triple.empty()) {
            codegen_ctx.setTargetTriple(target_triple);
        }
        
        // Optional: print IR to stdout
        if (print_ir) {
            codegen_ctx.printIR();
        }
        
        // Verify module
        if (!codegen_ctx.verifyModule()) {
            std::cerr << "Module verification failed!" << std::endl;
            return 1;
        }
        
        // Output different formats based on options
        if (emit_llvm) {
            // Output LLVM IR
            codegen_ctx.optimizeAndEmit(output_file);
            if (!print_ir) {
                std::cout << "LLVM IR written to: " << output_file << std::endl;
            }
        } else {
            // Default: output object file
            if (!codegen_ctx.emitObjectFile(output_file, target_triple)) {
                std::cerr << "Failed to emit object file!" << std::endl;
                return 1;
            }
            std::cout << "Object file written to: " << output_file << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
