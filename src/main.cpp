#include "visitor.h"
#include "codegen.h"
#include "OlangLexer.h"
#include "OlangParser.h"
#include <iostream>
#include <fstream>
#include <memory>
#include <set>
#include <filesystem>
#include <llvm/Support/raw_ostream.h>
#include <llvm/IR/Module.h>

namespace fs = std::filesystem;

// Process includes recursively
std::string processIncludes(const std::string& filename, std::set<std::string>& included_files) {
    // Get canonical path to avoid duplicate includes
    std::string canonical_path = fs::canonical(filename).string();
    
    // Check if already included
    if (included_files.count(canonical_path) > 0) {
        return "";  // Already included, skip
    }
    included_files.insert(canonical_path);
    
    // Read file
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open file " << filename << std::endl;
        return "";
    }
    
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();
    
    // Get directory of current file
    fs::path file_path(filename);
    fs::path file_dir = file_path.parent_path();
    
    // Process includes
    std::string result;
    size_t pos = 0;
    while ((pos = content.find("include \"", pos)) != std::string::npos) {
        // Find end of include statement
        size_t quote_start = pos + 9;  // After 'include "'
        size_t quote_end = content.find("\"", quote_start);
        if (quote_end == std::string::npos) break;
        
        std::string include_file = content.substr(quote_start, quote_end - quote_start);
        
        // Resolve relative path
        fs::path include_path = file_dir / include_file;
        
        // Recursively process included file
        std::string included_content = processIncludes(include_path.string(), included_files);
        
        // Replace include statement with file content
        size_t semicolon = content.find(";", quote_end);
        if (semicolon != std::string::npos) {
            result += content.substr(0, pos);
            result += "// Included from: " + include_file + "\n";
            result += included_content;
            result += "\n// End of: " + include_file + "\n";
            content = content.substr(semicolon + 1);
            pos = 0;
        } else {
            pos = quote_end;
        }
    }
    
    result += content;
    return result;
}

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
    
    // Process includes
    std::set<std::string> included_files;
    std::string input = processIncludes(filename, included_files);
    
    if (input.empty()) {
        std::cerr << "Error: Failed to read input file" << std::endl;
        return 1;
    }
    
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
