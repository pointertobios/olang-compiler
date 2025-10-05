cmake_minimum_required(VERSION 3.20)
project(olang-compiler)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Use FetchContent to download ANTLR4 runtime
include(FetchContent)

message(STATUS "Downloading ANTLR4 C++ runtime from GitHub...")
FetchContent_Declare(
    antlr4_runtime
    GIT_REPOSITORY https://github.com/antlr/antlr4.git
    GIT_TAG 4.13.2
    SOURCE_SUBDIR runtime/Cpp
)

# Configure ANTLR4 options
set(ANTLR4_WITH_STATIC_CRT OFF CACHE BOOL "Use static CRT" FORCE)
set(ANTLR_BUILD_CPP_TESTS OFF CACHE BOOL "Build C++ tests" FORCE)
set(ANTLR_BUILD_SHARED OFF CACHE BOOL "Build shared library" FORCE)
set(ANTLR_BUILD_STATIC ON CACHE BOOL "Build static library" FORCE)

# Suppress warnings in ANTLR4 build
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-parameter -Wno-deprecated-declarations -Wno-overloaded-virtual")

FetchContent_MakeAvailable(antlr4_runtime)

# Set ANTLR4 include directory
include_directories(${antlr4_runtime_SOURCE_DIR}/runtime/Cpp/runtime/src)

# Find LLVM
find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")

# Include LLVM headers
include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

# Map LLVM components for non-monolithic builds
llvm_map_components_to_libnames(llvm_libs 
    support core irreader
    x86codegen x86asmparser x86desc x86info
    mc mcparser
    target
    asmprinter
)

# Generated files directory
set(GENERATED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/generated")

# ANTLR generated files (in generated/ directory)
set(ANTLR_SOURCES
    ${GENERATED_DIR}/OlangLexer.cpp
    ${GENERATED_DIR}/OlangParser.cpp
    ${GENERATED_DIR}/OlangBaseVisitor.cpp
    ${GENERATED_DIR}/OlangVisitor.cpp
)

# Compiler source files
set(COMPILER_SOURCES
    src/main.cpp
    src/codegen.cpp
    src/visitor.cpp
    ${ANTLR_SOURCES}
)

# Include directories
include_directories(include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${GENERATED_DIR})

# Create executable
add_executable(olc ${COMPILER_SOURCES})

# Ensure the linker can find LLVM libraries
if(DEFINED LLVM_LIBRARY_DIRS)
    # Modern CMake prefers target_link_directories over link_directories
    target_link_directories(olc PRIVATE ${LLVM_LIBRARY_DIRS})
elseif(DEFINED LLVM_LIBRARY_DIR)
    target_link_directories(olc PRIVATE ${LLVM_LIBRARY_DIR})
endif()

# Link ANTLR4 runtime and LLVM (support both monolithic and component builds)
if(TARGET LLVM::LLVM)
    # Preferred: imported CMake target provided by LLVM package
    target_link_libraries(olc PRIVATE antlr4_static LLVM::LLVM)
elseif(TARGET LLVM)
    # Some distros export a plain 'LLVM' target
    target_link_libraries(olc PRIVATE antlr4_static LLVM)
elseif(LLVM_LINK_LLVM_DYLIB)
    # LLVM built as a single shared library (libLLVM)
    target_link_libraries(olc PRIVATE antlr4_static LLVM)
else()
    # LLVM built with individual component libraries
    target_link_libraries(olc PRIVATE antlr4_static ${llvm_libs})
endif()

# Add compile options
target_compile_options(olc PRIVATE -Wall -Wextra)

# Suppress warnings from third-party libraries
target_compile_options(olc PRIVATE 
    -Wno-unused-parameter
    -Wno-deprecated-declarations
    -Wno-overloaded-virtual
)

# Define ANTLR JAR file path
set(ANTLR_JAR "${CMAKE_CURRENT_SOURCE_DIR}/antlr-4.13.2-complete.jar")
set(ANTLR_JAR_URL "https://www.antlr.org/download/antlr-4.13.2-complete.jar")

# Download ANTLR JAR if it doesn't exist
if(NOT EXISTS ${ANTLR_JAR})
    message(STATUS "Downloading ANTLR 4.13.2 tool...")
    file(DOWNLOAD ${ANTLR_JAR_URL} ${ANTLR_JAR}
         SHOW_PROGRESS
         STATUS download_status
         TIMEOUT 60)
    list(GET download_status 0 status_code)
    if(NOT status_code EQUAL 0)
        message(FATAL_ERROR "Failed to download ANTLR JAR! Please manually download to: ${ANTLR_JAR}")
    endif()
    message(STATUS "✅ ANTLR tool downloaded successfully")
endif()

# Custom command: generate ANTLR C++ code from .g4 file to generated/ directory
add_custom_command(
    OUTPUT ${ANTLR_SOURCES}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${GENERATED_DIR}
    COMMAND java -jar ${ANTLR_JAR}
            -Dlanguage=Cpp 
            -visitor 
            -o ${GENERATED_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/Olang.g4
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/Olang.g4 ${ANTLR_JAR}
    COMMENT "Generating ANTLR parser files to generated/ directory..."
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    VERBATIM
)

# Ensure ANTLR files are generated before compilation
add_custom_target(generate_parser DEPENDS ${ANTLR_SOURCES})
add_dependencies(olc generate_parser)
